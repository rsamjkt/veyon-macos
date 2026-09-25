#!/bin/bash
# Deploys the Aruni Relay to a VPS over SSH (Docker + Caddy with automatic HTTPS).
#
#   ./deploy.sh user@vps.example.com relay.arunika.id
#
# Needs: SSH access, Docker with the compose plugin on the VPS (installed if
# missing on Debian/Ubuntu), the DNS name pointing at the VPS, ports 80/443 open.
set -euo pipefail

TARGET="${1:?usage: $0 user@host domain}"
DOMAIN="${2:?usage: $0 user@host domain}"
REMOTE_DIR="${REMOTE_DIR:-/opt/aruni-relay}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

echo "==> Checking DNS: ${DOMAIN}"
dig +short "${DOMAIN}" || true

echo "==> Preparing ${TARGET}:${REMOTE_DIR}"
ssh "${TARGET}" "set -e
	if ! command -v docker >/dev/null 2>&1; then
		curl -fsSL https://get.docker.com | sh
	fi
	sudo mkdir -p '${REMOTE_DIR}' && sudo chown \"\$(id -u):\$(id -g)\" '${REMOTE_DIR}'"

echo "==> Uploading relay sources"
tar -C "${SCRIPT_DIR}" -czf - Dockerfile Caddyfile docker-compose.yml go.mod go.sum main.go |
	ssh "${TARGET}" "tar -C '${REMOTE_DIR}' -xzf -"

echo "==> Starting (docker compose up -d --build)"
ssh "${TARGET}" "cd '${REMOTE_DIR}' && echo 'RELAY_DOMAIN=${DOMAIN}' > .env &&
	(docker compose up -d --build || sudo docker compose up -d --build)"

echo "==> Health check"
for i in $(seq 1 30); do
	if curl -fsS "https://${DOMAIN}/healthz"; then
		echo; echo "Aruni Relay is live at wss://${DOMAIN}"
		exit 0
	fi
	sleep 5
done
echo "Relay not reachable over HTTPS yet - check DNS, firewall (80/443) and 'docker compose logs' on the VPS" >&2
exit 1
