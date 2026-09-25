# Aruni Relay

Pairs AruniControl Masters (e.g. the Android app on mobile data) with an **Aruni
Gateway** (a client PC inside the school/office LAN with the gateway enabled in the
Configurator). Both sides connect outbound over WebSocket/TLS, so no port forwarding,
public IP or VPN is needed at the site. The relay only forwards opaque binary messages:
Master and gateway run an end-to-end encrypted handshake (`core/src/AruniTunnel.*`).

## Deploy on a VPS (Docker)

1. Point a DNS name at the VPS, e.g. `relay.arunika.id` (A/AAAA record).
2. Open TCP 80 and 443.
3. Copy this directory to the VPS and start it:

```bash
RELAY_DOMAIN=relay.arunika.id docker compose up -d --build
curl https://relay.arunika.id/healthz      # {"gateways":0,"status":"ok"}
```

Caddy obtains the Let's Encrypt certificate automatically. With the GHCR image
(`ghcr.io/rsamjkt/aruni-relay`, built by `.github/workflows/relay-image.yml`) use
`docker compose pull && docker compose up -d` instead of `--build`.

## Endpoints

| Path | Who | |
|---|---|---|
| `GET /v1/gateway/{id}` | gateway | control socket, header `X-Aruni-Secret`; receives `{"type":"session","sid":…}` |
| `GET /v1/connect/{id}` | Master | a new session; closed with 4404 if the gateway is offline |
| `GET /v1/accept/{id}/{sid}` | gateway | session socket, bridged to the Master |
| `GET /healthz` | monitoring | |

Gateway ids are claimed trust-on-first-use: the first registration stores a hash of the
gateway's secret in `/data/gateways.json`; later registrations must present the same secret.

## Resources

A session only carries what the Master shows: live remote view ≈ 30–300 KB/s depending
on screen activity (tunnelled connections are capped at medium JPEG quality), monitoring
thumbnails a few KB/s per computer. A 1 vCPU / 1 GB VPS handles dozens of sessions;
bandwidth is the limiting factor.
