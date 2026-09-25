// Aruni Relay - pairs AruniControl Masters (e.g. the Android app on mobile
// data) with Aruni Gateways (a client PC inside a school/office LAN).
//
// Both sides connect *outbound* over WebSocket (TLS terminated by a reverse
// proxy such as Caddy), so gateways work without port forwarding or public IPs.
// The relay never sees plaintext: Master and gateway run an end-to-end
// encrypted handshake (core/src/AruniTunnel.*) over the paired sockets; the
// relay just copies binary messages.
//
//	GET /v1/gateway/{id}          gateway control socket (header X-Aruni-Secret)
//	GET /v1/connect/{id}          master session; relay announces it to the gateway
//	GET /v1/accept/{id}/{session} gateway session socket, bridged to the master
//	GET /healthz
//
// A gateway id is claimed trust-on-first-use: the first registration stores a
// hash of its secret and later registrations must present the same secret.
package main

import (
	"context"
	"crypto/rand"
	"crypto/sha256"
	"crypto/subtle"
	"encoding/hex"
	"encoding/json"
	"errors"
	"flag"
	"log"
	"net"
	"net/http"
	"os"
	"path/filepath"
	"regexp"
	"strings"
	"sync"
	"sync/atomic"
	"time"

	"github.com/coder/websocket"
)

const (
	maxMessageSize        = 1 << 20
	acceptTimeout         = 15 * time.Second
	pingInterval          = 25 * time.Second
	maxSessionsPerGateway = 64
	maxPendingBytes       = 256 << 10

	closeGatewayOffline websocket.StatusCode = 4404
	closeGatewayBusy    websocket.StatusCode = 4429
	closeUnauthorized   websocket.StatusCode = 4401
	closeReplaced       websocket.StatusCode = 4409
)

var idPattern = regexp.MustCompile(`^[A-Za-z0-9_-]{8,64}$`)

type gateway struct {
	id       string
	conn     *websocket.Conn
	ctx      context.Context
	cancel   context.CancelFunc
	writeMu  sync.Mutex
	sessions atomic.Int32
}

func (g *gateway) announce(sessionID string) error {
	msg, _ := json.Marshal(map[string]string{"type": "session", "sid": sessionID})
	g.writeMu.Lock()
	defer g.writeMu.Unlock()
	ctx, cancel := context.WithTimeout(g.ctx, 5*time.Second)
	defer cancel()
	return g.conn.Write(ctx, websocket.MessageText, msg)
}

type acceptedSocket struct {
	conn *websocket.Conn
	done chan struct{} // closed by the master side when the session ends
}

type pendingSession struct {
	gatewayID string
	accepted  chan acceptedSocket
}

type relay struct {
	mu       sync.Mutex
	gateways map[string]*gateway
	pending  map[string]*pendingSession
	secrets  map[string]string // gateway id -> sha256(secret) hex
	store    string

	limiter *rateLimiter
}

func newRelay(dataDir string) *relay {
	r := &relay{
		gateways: map[string]*gateway{},
		pending:  map[string]*pendingSession{},
		secrets:  map[string]string{},
		store:    filepath.Join(dataDir, "gateways.json"),
		limiter:  newRateLimiter(30, time.Minute),
	}
	if data, err := os.ReadFile(r.store); err == nil {
		_ = json.Unmarshal(data, &r.secrets)
	}
	return r
}

func (r *relay) saveSecretsLocked() {
	data, _ := json.MarshalIndent(r.secrets, "", " ")
	tmp := r.store + ".tmp"
	if err := os.WriteFile(tmp, data, 0o600); err == nil {
		_ = os.Rename(tmp, r.store)
	} else {
		log.Printf("cannot persist gateway secrets: %v", err)
	}
}

func hashSecret(secret string) string {
	sum := sha256.Sum256([]byte(secret))
	return hex.EncodeToString(sum[:])
}

func randomID() string {
	b := make([]byte, 16)
	_, _ = rand.Read(b)
	return hex.EncodeToString(b)
}

func clientIP(req *http.Request) string {
	// set by Cloudflare (tunnel) and not forgeable by clients behind it
	if cf := req.Header.Get("CF-Connecting-IP"); cf != "" {
		return cf
	}
	if fwd := req.Header.Get("X-Forwarded-For"); fwd != "" {
		return strings.TrimSpace(strings.Split(fwd, ",")[0])
	}
	host, _, _ := net.SplitHostPort(req.RemoteAddr)
	return host
}

func accept(w http.ResponseWriter, req *http.Request) (*websocket.Conn, error) {
	conn, err := websocket.Accept(w, req, &websocket.AcceptOptions{InsecureSkipVerify: true})
	if err != nil {
		return nil, err
	}
	conn.SetReadLimit(maxMessageSize)
	return conn, nil
}

// authorizeGateway checks (or on first use claims) a gateway id
func (r *relay) authorizeGateway(id, secret string) bool {
	if len(secret) < 16 {
		return false
	}
	hashed := hashSecret(secret)
	r.mu.Lock()
	defer r.mu.Unlock()
	known, ok := r.secrets[id]
	if !ok {
		r.secrets[id] = hashed
		r.saveSecretsLocked()
		log.Printf("gateway %s registered", id)
		return true
	}
	return subtle.ConstantTimeCompare([]byte(known), []byte(hashed)) == 1
}

func (r *relay) handleGateway(w http.ResponseWriter, req *http.Request) {
	id := req.PathValue("id")
	if !idPattern.MatchString(id) || !r.authorizeGateway(id, req.Header.Get("X-Aruni-Secret")) {
		http.Error(w, "unauthorized", http.StatusUnauthorized)
		return
	}

	conn, err := accept(w, req)
	if err != nil {
		return
	}

	ctx, cancel := context.WithCancel(req.Context())
	g := &gateway{id: id, conn: conn, ctx: ctx, cancel: cancel}

	r.mu.Lock()
	if old := r.gateways[id]; old != nil {
		old.conn.Close(closeReplaced, "replaced by a new connection")
		old.cancel()
	}
	r.gateways[id] = g
	r.mu.Unlock()
	log.Printf("gateway %s online from %s", id, clientIP(req))

	go keepAlive(ctx, conn, cancel)

	// the control socket carries nothing from the gateway - just read until it closes
	for {
		if _, _, err := conn.Read(ctx); err != nil {
			break
		}
	}

	cancel()
	r.mu.Lock()
	if r.gateways[id] == g {
		delete(r.gateways, id)
	}
	r.mu.Unlock()
	conn.CloseNow()
	log.Printf("gateway %s offline", id)
}

func (r *relay) handleConnect(w http.ResponseWriter, req *http.Request) {
	id := req.PathValue("id")
	if !idPattern.MatchString(id) {
		http.Error(w, "bad gateway id", http.StatusBadRequest)
		return
	}
	if !r.limiter.allow(clientIP(req)) {
		http.Error(w, "too many requests", http.StatusTooManyRequests)
		return
	}

	conn, err := accept(w, req)
	if err != nil {
		return
	}
	defer conn.CloseNow()

	r.mu.Lock()
	g := r.gateways[id]
	r.mu.Unlock()
	if g == nil {
		conn.Close(closeGatewayOffline, "gateway offline")
		return
	}
	if g.sessions.Load() >= maxSessionsPerGateway {
		conn.Close(closeGatewayBusy, "too many sessions")
		return
	}

	sessionID := randomID()
	p := &pendingSession{gatewayID: id, accepted: make(chan acceptedSocket, 1)}
	r.mu.Lock()
	r.pending[sessionID] = p
	r.mu.Unlock()
	defer func() {
		r.mu.Lock()
		delete(r.pending, sessionID)
		r.mu.Unlock()
		// a gateway socket that arrived after we gave up must not hang
		select {
		case late := <-p.accepted:
			late.conn.Close(closeGatewayOffline, "session expired")
			close(late.done)
		default:
		}
	}()

	if err := g.announce(sessionID); err != nil {
		conn.Close(closeGatewayOffline, "gateway unreachable")
		return
	}

	// the master starts the handshake right away - keep its first messages
	// until the gateway has picked the session up
	ctx, cancel := context.WithTimeout(req.Context(), acceptTimeout)
	var buffered [][]byte
	bufferedBytes := 0
	readErr := make(chan error, 1)
	messages := make(chan []byte, 16)
	go func() {
		for {
			_, data, err := conn.Read(req.Context())
			if err != nil {
				readErr <- err
				return
			}
			messages <- data
		}
	}()

	var gatewayConn *websocket.Conn
wait:
	for {
		select {
		case socket := <-p.accepted:
			gatewayConn = socket.conn
			defer close(socket.done)
			break wait
		case data := <-messages:
			bufferedBytes += len(data)
			if bufferedBytes > maxPendingBytes {
				cancel()
				conn.Close(websocket.StatusMessageTooBig, "too much data before session start")
				return
			}
			buffered = append(buffered, data)
		case <-readErr:
			cancel()
			return
		case <-ctx.Done():
			cancel()
			conn.Close(closeGatewayOffline, "gateway did not answer")
			return
		}
	}
	cancel()

	g.sessions.Add(1)
	defer g.sessions.Add(-1)

	sessionCtx, stop := context.WithCancel(req.Context())
	defer stop()
	defer gatewayConn.CloseNow()

	for _, data := range buffered {
		if err := gatewayConn.Write(sessionCtx, websocket.MessageBinary, data); err != nil {
			return
		}
	}

	var transferred atomic.Int64
	started := time.Now()

	// master -> gateway (continuing the reader goroutine's channel)
	go func() {
		defer stop()
		for {
			select {
			case data := <-messages:
				if gatewayConn.Write(sessionCtx, websocket.MessageBinary, data) != nil {
					return
				}
				transferred.Add(int64(len(data)))
			case <-readErr:
				return
			case <-sessionCtx.Done():
				return
			}
		}
	}()

	// gateway -> master
	go func() {
		defer stop()
		for {
			_, data, err := gatewayConn.Read(sessionCtx)
			if err != nil {
				return
			}
			if conn.Write(sessionCtx, websocket.MessageBinary, data) != nil {
				return
			}
			transferred.Add(int64(len(data)))
		}
	}()

	go keepAlive(sessionCtx, conn, stop)

	<-sessionCtx.Done()
	log.Printf("session %s via gateway %s closed after %s, %d bytes", sessionID[:8], id,
		time.Since(started).Round(time.Second), transferred.Load())
}

func (r *relay) handleAccept(w http.ResponseWriter, req *http.Request) {
	id := req.PathValue("id")
	sessionID := req.PathValue("session")
	if !idPattern.MatchString(id) || !r.authorizeGateway(id, req.Header.Get("X-Aruni-Secret")) {
		http.Error(w, "unauthorized", http.StatusUnauthorized)
		return
	}

	r.mu.Lock()
	p := r.pending[sessionID]
	if p != nil && p.gatewayID == id {
		delete(r.pending, sessionID)
	} else {
		p = nil
	}
	r.mu.Unlock()
	if p == nil {
		http.Error(w, "unknown session", http.StatusNotFound)
		return
	}

	conn, err := accept(w, req)
	if err != nil {
		return
	}

	// hand the socket over; the master's handler owns it from now on and
	// signals the end of the session
	socket := acceptedSocket{conn: conn, done: make(chan struct{})}
	p.accepted <- socket
	<-socket.done
}

func keepAlive(ctx context.Context, conn *websocket.Conn, cancel context.CancelFunc) {
	ticker := time.NewTicker(pingInterval)
	defer ticker.Stop()
	for {
		select {
		case <-ctx.Done():
			return
		case <-ticker.C:
			pingCtx, pingCancel := context.WithTimeout(ctx, 15*time.Second)
			err := conn.Ping(pingCtx)
			pingCancel()
			if err != nil && !errors.Is(err, context.Canceled) {
				cancel()
				return
			}
		}
	}
}

// rateLimiter allows n new master sessions per window and IP
type rateLimiter struct {
	mu     sync.Mutex
	n      int
	window time.Duration
	hits   map[string][]time.Time
}

func newRateLimiter(n int, window time.Duration) *rateLimiter {
	return &rateLimiter{n: n, window: window, hits: map[string][]time.Time{}}
}

func (l *rateLimiter) allow(key string) bool {
	l.mu.Lock()
	defer l.mu.Unlock()
	now := time.Now()
	recent := l.hits[key][:0]
	for _, t := range l.hits[key] {
		if now.Sub(t) < l.window {
			recent = append(recent, t)
		}
	}
	if len(recent) >= l.n {
		l.hits[key] = recent
		return false
	}
	l.hits[key] = append(recent, now)
	return true
}

func main() {
	listen := flag.String("listen", ":8080", "listen address (plain HTTP/WebSocket behind a TLS reverse proxy)")
	dataDir := flag.String("data", "./data", "directory for persistent state")
	flag.Parse()

	if err := os.MkdirAll(*dataDir, 0o700); err != nil {
		log.Fatalf("cannot create data directory: %v", err)
	}

	r := newRelay(*dataDir)
	mux := http.NewServeMux()
	mux.HandleFunc("GET /v1/gateway/{id}", r.handleGateway)
	mux.HandleFunc("GET /v1/connect/{id}", r.handleConnect)
	mux.HandleFunc("GET /v1/accept/{id}/{session}", r.handleAccept)
	mux.HandleFunc("GET /healthz", func(w http.ResponseWriter, _ *http.Request) {
		r.mu.Lock()
		online := len(r.gateways)
		r.mu.Unlock()
		_ = json.NewEncoder(w).Encode(map[string]any{"status": "ok", "gateways": online})
	})

	server := &http.Server{
		Addr:              *listen,
		Handler:           mux,
		ReadHeaderTimeout: 10 * time.Second,
	}
	log.Printf("Aruni Relay listening on %s", *listen)
	log.Fatal(server.ListenAndServe())
}
