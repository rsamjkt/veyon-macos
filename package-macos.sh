#!/usr/bin/env bash
#
# package-macos.sh - assemble a self-contained AruniControl.app bundle (experimental)
#
# Run ./build-macos.sh first (it must have produced build/ with all targets).
# Produces dist/AruniControl.app, bundling every Veyon executable, libveyon-core, all
# plugins, Qt and the other Homebrew dependencies, then ad-hoc code-signs it.
#
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/build"
DIST_DIR="${SCRIPT_DIR}/dist"
APP="${DIST_DIR}/AruniControl.app"
CONTENTS="${APP}/Contents"
MACOS_DIR="${CONTENTS}/MacOS"
FRAMEWORKS_DIR="${CONTENTS}/Frameworks"
PLUGIN_DIR="${CONTENTS}/lib/veyon"
RES_DIR="${CONTENTS}/Resources"

VERSION="1.5.0"
BUNDLE_ID="id.arunika.arunicontrol"

if ! command -v brew >/dev/null 2>&1; then echo "error: Homebrew required" >&2; exit 1; fi
eval "$(brew shellenv)"
MACDEPLOYQT="$(brew --prefix qt)/bin/macdeployqt"
if [ ! -x "${MACDEPLOYQT}" ]; then echo "error: macdeployqt not found" >&2; exit 1; fi
if [ ! -f "${BUILD_DIR}/core/libveyon-core.dylib" ]; then
	echo "error: build/ not found - run ./build-macos.sh first" >&2; exit 1
fi

echo "==> Creating bundle skeleton at ${APP}"
rm -rf "${APP}"
mkdir -p "${MACOS_DIR}" "${FRAMEWORKS_DIR}" "${PLUGIN_DIR}" "${RES_DIR}"

echo "==> Copying executables"
for exe in master/veyon-master configurator/veyon-configurator server/veyon-server \
		   service/veyon-service cli/veyon-cli worker/veyon-worker; do
	cp "${BUILD_DIR}/${exe}" "${MACOS_DIR}/"
done

echo "==> Copying core library and plugins"
cp "${BUILD_DIR}/core/libveyon-core.dylib" "${FRAMEWORKS_DIR}/"
find "${BUILD_DIR}/plugins" -name '*.so' -exec cp {} "${PLUGIN_DIR}/" \;

echo "==> Writing Info.plist"
cat > "${CONTENTS}/Info.plist" <<PLIST
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
	<key>CFBundleName</key>            <string>AruniControl</string>
	<key>CFBundleDisplayName</key>     <string>AruniControl Master</string>
	<key>CFBundleIdentifier</key>      <string>${BUNDLE_ID}</string>
	<key>CFBundleExecutable</key>      <string>veyon-master</string>
	<key>CFBundleIconFile</key>        <string>AruniControl</string>
	<key>CFBundlePackageType</key>     <string>APPL</string>
	<key>CFBundleVersion</key>         <string>${VERSION}</string>
	<key>CFBundleShortVersionString</key> <string>${VERSION}</string>
	<key>LSMinimumSystemVersion</key>  <string>14.0</string>
	<key>NSHighResolutionCapable</key> <true/>
	<key>NSPrincipalClass</key>        <string>NSApplication</string>
	<key>NSAppleEventsUsageDescription</key>
	<string>AruniControl controls system power and session state on your behalf.</string>
	<key>NSLocalNetworkUsageDescription</key>
	<string>AruniControl discovers and connects to computers running AruniControl Server on your local network.</string>
	<key>NSMicrophoneUsageDescription</key>
	<string>AruniControl uses the microphone for two-way voice (AruniVoice) with selected computers.</string>
</dict>
</plist>
PLIST

# qt.conf so Qt finds its plugins inside the bundle (PlugIns) - macdeployqt also
# writes this, but create it up-front for safety.
cat > "${RES_DIR}/qt.conf" <<'QTCONF'
[Paths]
Plugins = PlugIns
QTCONF

echo "==> Installing app icon"
cp "${SCRIPT_DIR}/master/data/AruniControl.icns" "${RES_DIR}/AruniControl.icns"

echo "==> Normalising install names / rpaths"
install_name_tool -id "@rpath/libveyon-core.dylib" "${FRAMEWORKS_DIR}/libveyon-core.dylib" 2>/dev/null || true

add_rpath() { # add an LC_RPATH if not already present
	local bin="$1" rp="$2"
	if ! otool -l "$bin" 2>/dev/null | grep -q "path ${rp} "; then
		install_name_tool -add_rpath "$rp" "$bin" 2>/dev/null || true
	fi
}

EXECUTABLE_ARGS=()
for bin in "${MACOS_DIR}"/* "${PLUGIN_DIR}"/*.so; do
	add_rpath "$bin" "@executable_path/../Frameworks"
	# strip the absolute build-tree rpaths so only bundle-relative ones remain
	install_name_tool -delete_rpath "${BUILD_DIR}/core" "$bin" 2>/dev/null || true
	# don't pass the primary executable twice
	if [ "$bin" != "${MACOS_DIR}/veyon-master" ]; then
		EXECUTABLE_ARGS+=("-executable=$bin")
	fi
done

echo "==> Running macdeployqt (bundling Qt + dependencies)"
"${MACDEPLOYQT}" "${APP}" "${EXECUTABLE_ARGS[@]}" -verbose=1 || true

echo "==> Copying translations"
mkdir -p "${RES_DIR}/translations"
find "${BUILD_DIR}/translations" -name '*.qm' -exec cp {} "${RES_DIR}/translations/" \;

echo "==> Bundling QCA crypto provider plugins"
# QCA loads its crypto providers from its build-time plugin dir inside Homebrew,
# so they have to be bundled to be found on a machine without Homebrew. Only the
# providers Veyon actually needs are taken: 'ossl' does the RSA key
# authentication, the other two are tiny and dependency-free. The remaining
# providers (botan, nss, pkcs11, ...) would only add dylibs that cannot resolve
# their own Homebrew dependencies on the target machine.
QCA_SRC="$(brew --prefix qca)/lib/qt/plugins/crypto"
if [ -d "${QCA_SRC}" ]; then
	mkdir -p "${CONTENTS}/PlugIns/crypto"
	for provider in ossl softstore logger; do
		cp "${QCA_SRC}/libqca-${provider}.dylib" "${CONTENTS}/PlugIns/crypto/" 2>/dev/null || true
	done
	if [ ! -f "${CONTENTS}/PlugIns/crypto/libqca-ossl.dylib" ]; then
		echo "error: QCA 'ossl' provider not found - RSA key authentication would fail" >&2
		exit 1
	fi
fi

echo "==> Relinking any remaining absolute dependencies to @rpath"
# macdeployqt sometimes leaves second-level dependencies pointing at Homebrew.
# Rewrite every such reference (whose target we actually bundled) to @rpath.
relink_file() {
	local f="$1"
	otool -L "$f" 2>/dev/null | awk '/\/opt\/homebrew/ {print $1}' | while read -r dep; do
		case "$dep" in
		*.framework/*)
			fw="$(printf '%s\n' "$dep" | sed -E 's#.*/([^/]+)\.framework/.*#\1#')"
			if [ -d "${FRAMEWORKS_DIR}/${fw}.framework" ]; then
				# preserve the framework's real internal path (e.g. Versions/2/qca-qt6),
				# which is not always "Versions/A"
				suffix="${dep#*.framework/}"
				install_name_tool -change "$dep" "@rpath/${fw}.framework/${suffix}" "$f" 2>/dev/null || true
			fi
			;;
		*)
			base="$(basename "$dep")"
			if [ -f "${FRAMEWORKS_DIR}/${base}" ]; then
				install_name_tool -change "$dep" "@rpath/${base}" "$f" 2>/dev/null || true
			fi
			;;
		esac
	done
}

# set self-ids of bundled dylibs and frameworks to @rpath, then relink every Mach-O
for dylib in "${FRAMEWORKS_DIR}"/*.dylib; do
	[ -f "$dylib" ] && install_name_tool -id "@rpath/$(basename "$dylib")" "$dylib" 2>/dev/null || true
done
for fw in "${FRAMEWORKS_DIR}"/*.framework; do
	name="$(basename "$fw" .framework)"
	# locate the real versioned binary (qca uses Versions/2, Qt uses Versions/A)
	bin="$(ls "$fw"/Versions/*/"${name}" 2>/dev/null | grep -v Current | head -1)"
	[ -z "$bin" ] && bin="$fw/${name}"
	if [ -f "$bin" ]; then
		ver="$(basename "$(dirname "$bin")")"
		install_name_tool -id "@rpath/${name}.framework/Versions/${ver}/${name}" "$bin" 2>/dev/null || true
	fi
done
# strip rpaths that point outside the bundle (build tree or Homebrew); otherwise
# @rpath/QtCore.framework can resolve to the Homebrew copy and load Qt twice.
strip_bad_rpaths() {
	local f="$1"
	otool -l "$f" 2>/dev/null | awk '/ LC_RPATH$/{c=2;next} c>0{c--; if($1=="path") print $2}' | while read -r rp; do
		case "$rp" in
		*/opt/homebrew/*|"${BUILD_DIR}"*)
			install_name_tool -delete_rpath "$rp" "$f" 2>/dev/null || true
			;;
		esac
	done
}

while IFS= read -r -d '' f; do
	relink_file "$f"
	strip_bad_rpaths "$f"
	# every loadable object resolves @rpath via the executable's Frameworks dir
	add_rpath "$f" "@executable_path/../Frameworks"
done < <(find "${MACOS_DIR}" "${PLUGIN_DIR}" "${FRAMEWORKS_DIR}" "${CONTENTS}/PlugIns" -type f -print0 2>/dev/null)

# A stable signing identity keeps the macOS permissions (Screen Recording,
# Accessibility) across updates: they are bound to the code's designated
# requirement, which for ad-hoc signatures is the hash of each build. The
# self-signed "AruniControl Code Signing" certificate lives in its own
# keychain (see ~/Library/AruniSigning/signing.env); without it the bundle is
# signed ad hoc as before.
SIGN_ID="-"
SIGN_ENV="${ARUNI_SIGNING_ENV:-$HOME/Library/AruniSigning/signing.env}"
if [ -f "$SIGN_ENV" ]; then
	# shellcheck disable=SC1090
	. "$SIGN_ENV"
	if security unlock-keychain -p "$ARUNI_SIGN_KEYCHAIN_PASS" "$ARUNI_SIGN_KEYCHAIN" 2>/dev/null; then
		case "$(security list-keychains -d user)" in
			*aruni-signing*) ;;
			*) security list-keychains -d user -s $(security list-keychains -d user | tr -d '"') "$ARUNI_SIGN_KEYCHAIN" ;;
		esac
		SIGN_ID="$ARUNI_SIGN_IDENTITY"
	fi
fi

echo "==> Code signing with identity '${SIGN_ID}' (must be the final step)"
# sign nested code first (deepest first), then the bundle
find "${APP}" \( -name '*.dylib' -o -name '*.so' \) -exec codesign --force --timestamp=none --sign "${SIGN_ID}" {} \; 2>/dev/null || true
find "${FRAMEWORKS_DIR}" -name '*.framework' -maxdepth 1 -exec codesign --force --timestamp=none --sign "${SIGN_ID}" {} \; 2>/dev/null || true
for exe in "${MACOS_DIR}"/*; do codesign --force --timestamp=none --sign "${SIGN_ID}" "$exe" 2>/dev/null || true; done
codesign --force --timestamp=none --sign "${SIGN_ID}" "${APP}" 2>/dev/null || true
codesign -d -r- "${MACOS_DIR}/veyon-server" 2>&1 | sed -n 's/^designated => /    designated requirement: /p'

echo "==> Verifying no Homebrew paths leak into the bundle"
# Contents/PlugIns has to be part of this: a crypto provider or Qt plugin that
# still points at Homebrew simply fails to load on the target machine.
LEAKS="$(find "${MACOS_DIR}" "${PLUGIN_DIR}" "${FRAMEWORKS_DIR}" "${CONTENTS}/PlugIns" -type f -print0 2>/dev/null \
	| xargs -0 -n1 otool -L 2>/dev/null | grep -c "/opt/homebrew" || true)"
echo "    remaining /opt/homebrew references: ${LEAKS}"
if [ "${LEAKS}" -gt 0 ]; then
	echo "    warning: the bundle will not run on a Mac without Homebrew" >&2
fi

echo "==> Writing LaunchAgent template for the Veyon Server"
# A LaunchAgent (not a LaunchDaemon) is required: the server must run inside the
# logged-in user's GUI session to capture the screen and inject input.
cat > "${DIST_DIR}/id.arunika.arunicontrol.plist" <<AGENT
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
	<key>Label</key>            <string>id.arunika.arunicontrol</string>
	<key>ProgramArguments</key>
	<array>
		<string>/Applications/AruniControl.app/Contents/MacOS/veyon-server</string>
	</array>
	<key>RunAtLoad</key>        <true/>
	<key>KeepAlive</key>        <true/>
	<key>ProcessType</key>      <string>Interactive</string>
</dict>
</plist>
AGENT

echo
echo "Done. Bundle: ${APP}"
echo
echo "Install (optional):   cp -R '${APP}' /Applications/"
echo "Launch the Master:    open '${APP}'"
echo "Run the server:       '${MACOS_DIR}/veyon-server'"
echo
echo "Auto-start the server in your session (after copying AruniControl.app to /Applications):"
echo "  cp '${DIST_DIR}/id.arunika.arunicontrol.plist' ~/Library/LaunchAgents/"
echo "  launchctl load ~/Library/LaunchAgents/id.arunika.arunicontrol.plist"
