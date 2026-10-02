#!/bin/bash
# AruniControl - pasang agent (client) di Mac dengan satu baris:
#
#   curl -fsSL https://arunicontrol.arunihealth.id/pasang.sh | ARUNI_KEY='ARUNISETUP1:...' bash
#
# Dijalankan sebagai pengguna Mac yang akan dipantau (bukan sudo). Installer
# diperiksa SHA-256-nya terhadap manifest rilis. ARUNI_UNINSTALL=1 menghapus.

set -euo pipefail

SITE="https://arunicontrol.arunihealth.id"
APP="/Applications/AruniControl.app"
CLI="$APP/Contents/MacOS/veyon-cli"

say() { printf '\033[36m[AruniControl]\033[0m %s\n' "$1"; }
ok() { printf '\033[32m[AruniControl]\033[0m %s\n' "$1"; }
fail() { printf '\033[31m[AruniControl] GAGAL:\033[0m %s\n' "$1" >&2; exit 1; }

[ "$(uname -s)" = "Darwin" ] || fail "Skrip ini untuk macOS. Untuk Windows pakai pasang.ps1."
[ "$(id -u)" != "0" ] || fail "Jalankan sebagai pengguna Mac biasa, tanpa sudo."

if [ "${ARUNI_UNINSTALL:-}" = "1" ]; then
	[ -x "$CLI" ] && "$CLI" service unregister >/dev/null 2>&1 || true
	rm -rf "$APP"
	ok "AruniControl sudah dihapus."
	exit 0
fi

KEY="${ARUNI_KEY:-}"
if [ -n "$KEY" ] && ! printf '%s' "$KEY" | grep -Eq '^ARUNISETUP1:[A-Za-z0-9_-]+$'; then
	fail "ARUNI_KEY bukan kode pemasangan yang valid (harus diawali ARUNISETUP1:)."
fi

WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

say "Mengambil informasi versi terbaru..."
MANIFEST="$(curl -fsSL -A 'AruniControl-Agent (macos)' "$SITE/unduh/versi.json")"
read -r URL SHA VERSION < <(printf '%s' "$MANIFEST" | /usr/bin/python3 -c 'import json,sys; m=json.load(sys.stdin); f=m["files"]["macos"]; print(f["url"], f["sha256"], m["version"])')

say "Mengunduh AruniControl $VERSION..."
curl -fsSL -A 'AruniControl-Agent (macos)' -o "$WORK/app.zip" "$URL"
[ "$(shasum -a 256 "$WORK/app.zip" | cut -d' ' -f1)" = "$SHA" ] || fail "Checksum tidak cocok - unduhan rusak atau diubah. Dibatalkan."
ok "Unduhan terverifikasi (SHA-256 cocok)."

say "Memasang ke /Applications..."
[ -x "$CLI" ] && "$CLI" service stop >/dev/null 2>&1 || true
/usr/bin/ditto -x -k "$WORK/app.zip" "$WORK/app"
rm -rf "$APP"
mv "$WORK/app/AruniControl.app" "$APP"
/usr/bin/xattr -dr com.apple.quarantine "$APP" 2>/dev/null || true

if [ -n "$KEY" ]; then
	printf '%s' "$KEY" > "$WORK/aruni-setup.txt"
	"$CLI" gateway setup "$WORK/aruni-setup.txt"
fi

"$CLI" service register >/dev/null 2>&1 || true
"$CLI" service start >/dev/null 2>&1 || true

ok "AruniControl agent terpasang di $(scutil --get ComputerName 2>/dev/null || hostname)."
say "Langkah terakhir (sekali saja): izinkan AruniControl di System Settings > Privacy & Security >"
say "Screen Recording dan Accessibility, lalu jalankan: $CLI service restart"
