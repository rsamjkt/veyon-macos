// Aruni Relay on Cloudflare Workers + Durable Objects.
//
// Same protocol as the Go relay (relay/main.go): an Aruni Gateway keeps a
// control WebSocket open, a Master connects for a session, the relay announces
// the session to the gateway, the gateway opens a matching session socket and
// the relay forwards the (end-to-end encrypted) binary messages between them.
//
//   GET /v1/gateway/{id}          gateway control socket (header X-Aruni-Secret)
//   GET /v1/connect/{id}          master session
//   GET /v1/accept/{id}/{session} gateway session socket
//   GET /healthz
//
// Everything for one gateway id lives in one Durable Object, so the sockets of
// a session always meet in the same place. The hibernation API keeps idle
// gateways free of charge.

const ID_PATTERN = /^[A-Za-z0-9_-]{8,64}$/;
const SESSION_PATTERN = /^[a-f0-9]{32}$/;
const ACCEPT_TIMEOUT_MS = 15000;
const MAX_PENDING_BYTES = 256 * 1024;
// phones and roaming laptops (one long-lived session each)
const MAX_SESSIONS = 1000;

const CLOSE_GATEWAY_OFFLINE = 4404;
const CLOSE_UNAUTHORIZED = 4401;
const CLOSE_BUSY = 4429;
const CLOSE_REPLACED = 4409;

export default {
	async fetch(request, env) {
		const url = new URL(request.url);

		if (url.pathname === "/healthz") {
			return Response.json({ status: "ok", relay: "cloudflare" });
		}

		const match = url.pathname.match(/^\/v1\/(gateway|connect|accept)\/([^/]+)(?:\/([^/]+))?$/);
		if (!match || !ID_PATTERN.test(match[2])) {
			return new Response("not found", { status: 404 });
		}
		if (request.headers.get("Upgrade")?.toLowerCase() !== "websocket") {
			return new Response("websocket expected", { status: 426 });
		}

		const room = env.GATEWAYS.get(env.GATEWAYS.idFromName(match[2]));
		return room.fetch(request);
	},
};

async function sha256Hex(text) {
	const digest = await crypto.subtle.digest("SHA-256", new TextEncoder().encode(text));
	return [...new Uint8Array(digest)].map((b) => b.toString(16).padStart(2, "0")).join("");
}

function randomSessionId() {
	const bytes = new Uint8Array(16);
	crypto.getRandomValues(bytes);
	return [...bytes].map((b) => b.toString(16).padStart(2, "0")).join("");
}

function timingSafeEqual(a, b) {
	if (a.length !== b.length) {
		return false;
	}
	let diff = 0;
	for (let i = 0; i < a.length; ++i) {
		diff |= a.charCodeAt(i) ^ b.charCodeAt(i);
	}
	return diff === 0;
}

export class GatewayRoom {
	constructor(state, env) {
		this.state = state;
		this.env = env;
		// keep-alive pings from the gateway are answered without waking the object
		this.state.setWebSocketAutoResponse(new WebSocketRequestResponsePair("ping", "pong"));
	}

	async fetch(request) {
		const url = new URL(request.url);
		const [, kind, gatewayId, sessionId] = url.pathname.match(/^\/v1\/(gateway|connect|accept)\/([^/]+)(?:\/([^/]+))?$/);

		switch (kind) {
			case "gateway":
				return this.handleGateway(request, gatewayId);
			case "connect":
				return this.handleConnect();
			case "accept":
				return this.handleAccept(request, sessionId);
		}
		return new Response("not found", { status: 404 });
	}

	// trust on first use: the first registration stores a hash of the secret
	async authorize(request) {
		const secret = request.headers.get("X-Aruni-Secret") ?? "";
		if (secret.length < 16) {
			return false;
		}
		const hashed = await sha256Hex(secret);
		const known = await this.state.storage.get("secret");
		if (!known) {
			await this.state.storage.put("secret", hashed);
			return true;
		}
		return timingSafeEqual(known, hashed);
	}

	acceptSocket(tags, attachment) {
		const pair = new WebSocketPair();
		this.state.acceptWebSocket(pair[1], tags);
		if (attachment) {
			pair[1].serializeAttachment(attachment);
		}
		return { client: pair[0], server: pair[1] };
	}

	async handleGateway(request, gatewayId) {
		if (!(await this.authorize(request))) {
			return new Response("unauthorized", { status: 401 });
		}

		for (const old of this.state.getWebSockets("control")) {
			try {
				old.close(CLOSE_REPLACED, "replaced by a new connection");
			} catch {}
		}

		const { client } = this.acceptSocket(["control"], { role: "control", gatewayId });
		return new Response(null, { status: 101, webSocket: client });
	}

	async handleConnect() {
		const controls = this.state.getWebSockets("control");
		const masters = this.state.getWebSockets("master");

		// the socket is accepted in any case so the Master gets a close code
		// it understands instead of a failed HTTP upgrade
		if (controls.length === 0 || masters.length >= MAX_SESSIONS) {
			const pair = new WebSocketPair();
			pair[1].accept();
			pair[1].close(controls.length === 0 ? CLOSE_GATEWAY_OFFLINE : CLOSE_BUSY,
				controls.length === 0 ? "gateway offline" : "too many sessions");
			return new Response(null, { status: 101, webSocket: pair[0] });
		}

		const sessionId = randomSessionId();
		const { client } = this.acceptSocket(["master", `m:${sessionId}`], {
			role: "master", sessionId, created: Date.now(),
		});

		// a control socket replaced by a reconnect can linger (closing) until its
		// dead peer times out - announce to all of them, only the live gateway
		// answers; if none does, the timeout below closes the session
		for (const control of controls) {
			try {
				control.send(JSON.stringify({ type: "session", sid: sessionId }));
			} catch {}
		}

		await this.state.storage.setAlarm(Date.now() + ACCEPT_TIMEOUT_MS);
		return new Response(null, { status: 101, webSocket: client });
	}

	async handleAccept(request, sessionId) {
		if (!SESSION_PATTERN.test(sessionId ?? "") || !(await this.authorize(request))) {
			return new Response("unauthorized", { status: 401 });
		}
		if (this.state.getWebSockets(`m:${sessionId}`).length === 0 ||
			this.state.getWebSockets(`g:${sessionId}`).length > 0) {
			return new Response("unknown session", { status: 404 });
		}

		const { client, server } = this.acceptSocket(["gateway", `g:${sessionId}`], { role: "gateway", sessionId });

		// deliver what the Master sent before the gateway picked the session up
		const pending = (await this.state.storage.get(`pending:${sessionId}`)) ?? [];
		for (const message of pending) {
			server.send(message);
		}
		await this.state.storage.delete(`pending:${sessionId}`);

		return new Response(null, { status: 101, webSocket: client });
	}

	partnerOf(ws) {
		const info = ws.deserializeAttachment() ?? {};
		if (info.role === "master") {
			return this.state.getWebSockets(`g:${info.sessionId}`)[0];
		}
		if (info.role === "gateway") {
			return this.state.getWebSockets(`m:${info.sessionId}`)[0];
		}
		return undefined;
	}

	async webSocketMessage(ws, message) {
		const info = ws.deserializeAttachment() ?? {};
		if (info.role === "control") {
			return; // nothing but keep-alives travel on the control socket
		}

		const partner = this.partnerOf(ws);
		if (partner) {
			partner.send(message);
			return;
		}

		if (info.role === "master") {
			// gateway not there yet - keep the handshake until it is
			const key = `pending:${info.sessionId}`;
			const pending = (await this.state.storage.get(key)) ?? [];
			const size = pending.reduce((sum, m) => sum + (m.byteLength ?? m.length), 0) + (message.byteLength ?? message.length);
			if (size > MAX_PENDING_BYTES) {
				ws.close(1009, "too much data before session start");
				await this.state.storage.delete(key);
				return;
			}
			pending.push(message);
			await this.state.storage.put(key, pending);
		}
	}

	async webSocketClose(ws, code, reason) {
		const info = ws.deserializeAttachment() ?? {};
		const partner = this.partnerOf(ws);
		if (partner) {
			try {
				partner.close(1000, "peer closed");
			} catch {}
		}
		if (info.sessionId) {
			await this.state.storage.delete(`pending:${info.sessionId}`);
		}
		try {
			ws.close(code === 1005 ? 1000 : code, reason);
		} catch {}
	}

	async webSocketError(ws) {
		await this.webSocketClose(ws, 1011, "error");
	}

	// masters whose gateway never picked the session up
	async alarm() {
		const now = Date.now();
		let next = 0;
		for (const ws of this.state.getWebSockets("master")) {
			const info = ws.deserializeAttachment() ?? {};
			if (this.state.getWebSockets(`g:${info.sessionId}`).length > 0) {
				continue;
			}
			if (now - (info.created ?? 0) >= ACCEPT_TIMEOUT_MS) {
				try {
					ws.close(CLOSE_GATEWAY_OFFLINE, "gateway did not answer");
				} catch {}
				await this.state.storage.delete(`pending:${info.sessionId}`);
			} else {
				next = Math.max(next, (info.created ?? now) + ACCEPT_TIMEOUT_MS);
			}
		}
		if (next > 0) {
			await this.state.storage.setAlarm(next);
		}
	}
}
