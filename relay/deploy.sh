#!/bin/bash
# Deploys the Aruni Relay to a Linux server/container over SSH.
#
#   SSH_PORT=2222 ./deploy.sh root@vps.example.com relay.arunihealth.id
#
# Works on a VPS as well as a Proxmox LXC container behind a MikroTik port
# forward. Options (environment):
#   SSH_PORT=22        SSH port (e.g. a forwarded non-default port)
#   MODE=native        native: static Go binary + systemd + Caddy (no Docker,
#                      simplest in an LXC container); docker: docker compose
#   TLS=caddy          caddy:    Caddy on 80/443 gets a Let's Encrypt certificate
#                               (ports 80+443 forwarded to this machine)
#                      cloudflare: Cloudflare Tunnel (cloudflared) - no port forwarding,
#                               TLS by Cloudflare; needs CF_TUNNEL_TOKEN from
#                               Zero Trust > Networks > Tunnels (public hostname
#                               <domain> -> http://localhost:8080)
#                      external: an existing reverse proxy terminates TLS and
#                               forwards the domain to this machine's port 8080
#   RELAY_PORT=8080    plain relay port (behind Caddy or the external proxy)
set -euo pipefail

TARGET="${1:?usage: [SSH_PORT=..] $0 user@host domain}"
DOMAIN="${2:?usage: [SSH_PORT=..] $0 user@host domain}"
SSH_PORT="${SSH_PORT:-22}"
MODE="${MODE:-native}"
TLS="${TLS:-caddy}"
RELAY_PORT="${RELAY_PORT:-8080}"
CF_TUNNEL_TOKEN="${CF_TUNNEL_TOKEN:-}"
if [ "${TLS}" = "cloudflare" ] && [ -z "${CF_TUNNEL_TOKEN}" ]; then
	echo "TLS=cloudflare needs CF_TUNNEL_TOKEN (Cloudflare Zero Trust > Networks > Tunnels)" >&2
	exit 1
fi
REMOTE_DIR="${REMOTE_DIR:-/opt/aruni-relay}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

SSH=(ssh -p "${SSH_PORT}" -o ServerAliveInterval=15 "${TARGET}")
SUDO='$( [ "$(id -u)" = 0 ] || echo sudo )'

echo "==> Connecting to ${TARGET} (port ${SSH_PORT})"
ARCH=$("${SSH[@]}" uname -m)
case "${ARCH}" in
	x86_64|amd64) GOARCH=amd64 ;;
	aarch64|arm64) GOARCH=arm64 ;;
	*) echo "unsupported architecture ${ARCH}" >&2; exit 1 ;;
esac

if [ "${MODE}" = "native" ]; then
	echo "==> Building relay for linux/${GOARCH}"
	export PATH="${HOME}/.local/go/bin:${PATH}"
	BIN="$(mktemp -d)/aruni-relay"
	( cd "${SCRIPT_DIR}" && CGO_ENABLED=0 GOOS=linux GOARCH="${GOARCH}" go build -trimpath -ldflags="-s -w" -o "${BIN}" . )

	LISTEN="127.0.0.1:${RELAY_PORT}"
	[ "${TLS}" = "external" ] && LISTEN="0.0.0.0:${RELAY_PORT}"

	echo "==> Installing service"
	cat "${BIN}" | "${SSH[@]}" "set -e; S=${SUDO}
		\$S mkdir -p '${REMOTE_DIR}' /var/lib/aruni-relay
		\$S tee '${REMOTE_DIR}/aruni-relay' >/dev/null
		\$S chmod 755 '${REMOTE_DIR}/aruni-relay'
		id aruni-relay >/dev/null 2>&1 || \$S useradd --system --home /var/lib/aruni-relay --shell /usr/sbin/nologin aruni-relay
		\$S chown aruni-relay: /var/lib/aruni-relay && \$S chmod 700 /var/lib/aruni-relay
		\$S tee /etc/systemd/system/aruni-relay.service >/dev/null <<UNIT
[Unit]
Description=Aruni Relay (AruniControl gateway relay)
After=network-online.target
Wants=network-online.target

[Service]
User=aruni-relay
ExecStart=${REMOTE_DIR}/aruni-relay -listen ${LISTEN} -data /var/lib/aruni-relay
Restart=always
RestartSec=2
NoNewPrivileges=true
ProtectSystem=strict
ProtectHome=true
ReadWritePaths=/var/lib/aruni-relay
PrivateTmp=true

[Install]
WantedBy=multi-user.target
UNIT
		\$S systemctl daemon-reload
		\$S systemctl enable --now aruni-relay
		\$S systemctl restart aruni-relay"

	if [ "${TLS}" = "cloudflare" ]; then
		echo "==> Installing cloudflared (Cloudflare Tunnel)"
		# the token is passed on stdin so it never shows up in a process list
		printf '%s' "${CF_TUNNEL_TOKEN}" | "${SSH[@]}" "set -e; S=${SUDO}
			TOKEN=\$(cat)
			if ! command -v cloudflared >/dev/null 2>&1; then
				if command -v apt-get >/dev/null 2>&1; then
					\$S mkdir -p --mode=0755 /usr/share/keyrings
					curl -fsSL https://pkg.cloudflare.com/cloudflare-main.gpg | \$S tee /usr/share/keyrings/cloudflare-main.gpg >/dev/null
					echo 'deb [signed-by=/usr/share/keyrings/cloudflare-main.gpg] https://pkg.cloudflare.com/cloudflared any main' | \$S tee /etc/apt/sources.list.d/cloudflared.list >/dev/null
					\$S apt-get update -qq && \$S apt-get install -y -qq cloudflared
				else
					echo 'Please install cloudflared manually' >&2; exit 1
				fi
			fi
			\$S cloudflared service uninstall >/dev/null 2>&1 || true
			\$S cloudflared service install \"\$TOKEN\"
			\$S systemctl enable --now cloudflared >/dev/null 2>&1 || true"
	fi

	if [ "${TLS}" = "caddy" ]; then
		echo "==> Installing Caddy (automatic HTTPS for ${DOMAIN})"
		"${SSH[@]}" "set -e; S=${SUDO}
			if ! command -v caddy >/dev/null 2>&1; then
				if command -v apt-get >/dev/null 2>&1; then
					\$S apt-get update -qq
					\$S apt-get install -y -qq debian-keyring debian-archive-keyring apt-transport-https curl gnupg
					curl -1sLf 'https://dl.cloudsmith.io/public/caddy/stable/gpg.key' | \$S gpg --dearmor --yes -o /usr/share/keyrings/caddy-stable-archive-keyring.gpg
					curl -1sLf 'https://dl.cloudsmith.io/public/caddy/stable/debian.deb.txt' | \$S tee /etc/apt/sources.list.d/caddy-stable.list >/dev/null
					\$S apt-get update -qq && \$S apt-get install -y -qq caddy
				else
					echo 'Please install Caddy manually (https://caddyserver.com/docs/install)' >&2; exit 1
				fi
			fi
			\$S tee /etc/caddy/Caddyfile >/dev/null <<CADDY
${DOMAIN} {
	reverse_proxy 127.0.0.1:${RELAY_PORT} {
		flush_interval -1
		transport http {
			read_timeout 0
			write_timeout 0
		}
	}
}
CADDY
			\$S systemctl enable caddy >/dev/null 2>&1 || true
			\$S systemctl restart caddy"
	fi
else
	echo "==> Uploading relay sources (docker compose)"
	"${SSH[@]}" "set -e; S=${SUDO}; command -v docker >/dev/null 2>&1 || curl -fsSL https://get.docker.com | \$S sh
		\$S mkdir -p '${REMOTE_DIR}' && \$S chown \"\$(id -u):\$(id -g)\" '${REMOTE_DIR}'"
	tar -C "${SCRIPT_DIR}" -czf - Dockerfile Caddyfile docker-compose.yml go.mod go.sum main.go |
		"${SSH[@]}" "tar -C '${REMOTE_DIR}' -xzf -"
	"${SSH[@]}" "cd '${REMOTE_DIR}' && echo 'RELAY_DOMAIN=${DOMAIN}' > .env && S=${SUDO} && \$S docker compose up -d --build"
fi

echo "==> Local check on the server"
"${SSH[@]}" "curl -fsS http://127.0.0.1:${RELAY_PORT}/healthz 2>/dev/null || wget -qO- http://127.0.0.1:${RELAY_PORT}/healthz" && echo

echo "==> Public check: https://${DOMAIN}/healthz"
for i in $(seq 1 20); do
	if curl -fsS --max-time 5 "https://${DOMAIN}/healthz"; then
		echo; echo "Aruni Relay is live: wss://${DOMAIN}"
		exit 0
	fi
	sleep 5
done
echo "The relay runs, but https://${DOMAIN} is not reachable yet. Check: DNS A record -> public IP," >&2
if [ "${TLS}" = "cloudflare" ]; then
	echo "Cloudflare: check the tunnel is HEALTHY and its public hostname ${DOMAIN} points to http://localhost:${RELAY_PORT}." >&2
else
	echo "MikroTik dst-nat for TCP 80 and 443 -> this container, and firewall rules." >&2
fi
exit 1
