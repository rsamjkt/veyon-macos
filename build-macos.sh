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
	-DWITH_TRANSLATIONS=OFF \
	-DWITH_PCH=OFF \
	"$@"

# Build the components that are expected to work first. The mac-platform plugin
# is required at runtime (otherwise the apps abort with "no platform plugin").
# Drop the --target list to attempt a full-tree build once these succeed.
cmake --build "${BUILD_DIR}" --parallel \
	--target veyon-core mac-platform veyon-master veyon-configurator veyon-cli

# Assemble a runtime plugin directory so the apps can locate plugins.
# Veyon looks for plugins in <bindir>/../lib/veyon (relative to each executable).
RUNTIME_PLUGIN_DIR="${BUILD_DIR}/lib/veyon"
mkdir -p "${RUNTIME_PLUGIN_DIR}"
while IFS= read -r plugin; do
	ln -sf "$plugin" "${RUNTIME_PLUGIN_DIR}/$(basename "$plugin")"
done < <(find "${BUILD_DIR}/plugins" -name '*.so' 2>/dev/null)

echo
echo "Build finished. Binaries are under ${BUILD_DIR}/ (core, master, configurator, cli)."
echo "Plugins linked into ${RUNTIME_PLUGIN_DIR}."
echo
echo "Run the configurator with:  ${BUILD_DIR}/configurator/veyon-configurator"
echo "Run the master with:        ${BUILD_DIR}/master/veyon-master"
