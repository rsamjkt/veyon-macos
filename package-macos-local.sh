#!/usr/bin/env bash
#
# package-macos-local.sh - build a *thin* AruniControl.app for THIS machine
#
# Unlike package-macos.sh (which bundles Qt for distribution to clean Macs),
# this produces a double-clickable app that uses the Homebrew Qt/QCA already
# installed here. That avoids the duplicate-Qt crash caused by Homebrew's qca
# crypto provider plugins. It is NOT portable to machines without Homebrew.
#
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/build"
DIST_DIR="${SCRIPT_DIR}/dist"

if [ ! -f "${BUILD_DIR}/master/veyon-master" ]; then
	echo "error: build/ not found - run ./build-macos.sh first" >&2; exit 1
fi

# remove the (self-contained) distribution bundle to avoid confusion on this host
rm -rf "${DIST_DIR}/AruniControl.app"

# build one thin .app per GUI tool. They share the same Veyon binaries and
# plugins, and rely on the Homebrew Qt/QCA already installed on this machine
# (which avoids the duplicate-Qt crash that the self-contained bundle hits here).
make_app() { # <App display name> <primary executable> <bundle id suffix>
	local appname="$1" mainexe="$2" idsuffix="$3"
	local app="${DIST_DIR}/${appname}.app"
	local contents="${app}/Contents"
	local macos="${contents}/MacOS"
	local plugins="${contents}/lib/veyon"

	echo "==> Creating ${app}"
	rm -rf "${app}"
	mkdir -p "${macos}" "${plugins}" "${contents}/Resources"

	for exe in master/veyon-master configurator/veyon-configurator server/veyon-server \
			   service/veyon-service cli/veyon-cli worker/veyon-worker; do
		cp "${BUILD_DIR}/${exe}" "${macos}/"
	done
	find "${BUILD_DIR}/plugins" -name '*.so' -exec cp {} "${plugins}/" \;

	cp "${SCRIPT_DIR}/master/data/AruniControl.icns" "${contents}/Resources/AruniControl.icns"

	cat > "${contents}/Info.plist" <<PLIST
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
	<key>CFBundleName</key>            <string>${appname}</string>
	<key>CFBundleDisplayName</key>     <string>${appname}</string>
	<key>CFBundleIdentifier</key>      <string>id.arunika.${idsuffix}</string>
	<key>CFBundleExecutable</key>      <string>${mainexe}</string>
	<key>CFBundleIconFile</key>        <string>AruniControl</string>
	<key>CFBundlePackageType</key>     <string>APPL</string>
	<key>CFBundleVersion</key>         <string>1.2.0</string>
	<key>CFBundleShortVersionString</key> <string>1.2.0</string>
	<key>LSMinimumSystemVersion</key>  <string>14.0</string>
	<key>NSHighResolutionCapable</key> <true/>
	<key>NSPrincipalClass</key>        <string>NSApplication</string>
	<key>NSLocalNetworkUsageDescription</key>
	<string>AruniControl discovers and connects to computers running AruniControl Server on your local network.</string>
	<key>NSMicrophoneUsageDescription</key>
	<string>AruniControl uses the microphone for two-way voice (AruniVoice) with selected computers.</string>
</dict>
</plist>
PLIST

	for f in "${macos}"/* "${plugins}"/*.so; do
		codesign --force --sign - "$f" 2>/dev/null || true
	done
	codesign --force --sign - "${app}" 2>/dev/null || true
}

make_app "AruniControl Master"       "veyon-master"       "arunicontrol"
make_app "AruniControl Configurator" "veyon-configurator" "configurator"

echo
echo "Done. Double-click:"
echo "  ${DIST_DIR}/AruniControl Master.app"
echo "  ${DIST_DIR}/AruniControl Configurator.app"
echo
echo "Note: these thin apps use the Homebrew Qt at /opt/homebrew and reference"
echo "libveyon-core in ${BUILD_DIR}/core - keep the build/ directory in place."
