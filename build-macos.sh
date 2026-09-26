#!/usr/bin/env bash
#
# build-macos.sh - configure and build the experimental macOS port of Veyon
#
# Prerequisites (install via Homebrew):
#   brew install cmake pkg-config ninja qt qca openssl@3 jpeg-turbo lzo libpng libvncserver
#
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/build"

if ! command -v brew >/dev/null 2>&1; then
	echo "error: Homebrew not found. Install it from https://brew.sh first." >&2
	exit 1
fi

if ! command -v cmake >/dev/null 2>&1; then
	echo "error: cmake not found. Run: brew install cmake" >&2
	exit 1
fi

# Collect Homebrew prefixes for the dependencies so CMake can find them.
prefix_path=""
for pkg in qt qca openssl@3 libvncserver jpeg-turbo lzo libpng; do
	if p="$(brew --prefix "$pkg" 2>/dev/null)"; then
		prefix_path="${prefix_path:+$prefix_path;}$p"
	fi
done

echo "Using CMAKE_PREFIX_PATH=${prefix_path}"

GENERATOR="Unix Makefiles"
if command -v ninja >/dev/null 2>&1; then
	GENERATOR="Ninja"
fi

cmake -S "${SCRIPT_DIR}" -B "${BUILD_DIR}" -G "${GENERATOR}" \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_PREFIX_PATH="${prefix_path}" \
	-DWITH_TRANSLATIONS=ON \
	-DWITH_PCH=OFF \
	"$@"

# The whole tree builds on macOS these days - server, worker, CLI and all
# plugins included - so no target list is needed anymore.
cmake --build "${BUILD_DIR}" --parallel

# Assemble a runtime plugin directory so the apps can locate plugins.
# Veyon looks for plugins in <bindir>/../lib/veyon (relative to each executable).
RUNTIME_PLUGIN_DIR="${BUILD_DIR}/lib/veyon"
mkdir -p "${RUNTIME_PLUGIN_DIR}"
while IFS= read -r plugin; do
	ln -sf "$plugin" "${RUNTIME_PLUGIN_DIR}/$(basename "$plugin")"
done < <(find "${BUILD_DIR}/plugins" -name '*.so' 2>/dev/null)

# veyon-server looks for veyon-worker right next to itself, which is how an
# installed bundle is laid out. In the build tree every component sits in its
# own directory, so link the worker over - otherwise worker-based features (the
# tray icon in the user session, for instance) silently fail to start here.
for exeDir in server master configurator cli; do
	if [ -d "${BUILD_DIR}/${exeDir}" ] && [ -x "${BUILD_DIR}/worker/veyon-worker" ]; then
		ln -sf "${BUILD_DIR}/worker/veyon-worker" "${BUILD_DIR}/${exeDir}/veyon-worker"
	fi
done

echo
echo "Build finished. Binaries are under ${BUILD_DIR}/ (core, master, configurator, cli)."
echo "Plugins linked into ${RUNTIME_PLUGIN_DIR}."
echo
echo "Run the configurator with:  ${BUILD_DIR}/configurator/veyon-configurator"
echo "Run the master with:        ${BUILD_DIR}/master/veyon-master"
