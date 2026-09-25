#!/bin/bash
# Build the AruniControl Android app (touch-friendly Master) as an APK.
#
# Needs: Android SDK + NDK r27c, JDK 17, Qt 6.11 for Android (arm64-v8a) plus the
# matching host Qt, and the third-party libraries from android/build-deps.sh
# (run automatically if missing). All paths can be overridden via environment.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${BUILD_DIR:-${SCRIPT_DIR}/build-android}"

export ANDROID_SDK_ROOT="${ANDROID_SDK_ROOT:-$HOME/Android/sdk}"
export ANDROID_NDK_ROOT="${ANDROID_NDK_ROOT:-$ANDROID_SDK_ROOT/ndk/27.2.12479018}"
export QT_ANDROID="${QT_ANDROID:-$HOME/Qt/6.11.3/android_arm64_v8a}"
export QT_HOST_PATH="${QT_HOST_PATH:-$(dirname "$QT_ANDROID")/macos}"
export DEPS_PREFIX="${DEPS_PREFIX:-$HOME/Android/arunicontrol-deps/arm64-v8a}"
if [ -z "${JAVA_HOME:-}" ] && [ -d /opt/homebrew/opt/openjdk@17 ]; then
	export JAVA_HOME=/opt/homebrew/opt/openjdk@17/libexec/openjdk.jdk/Contents/Home
fi

# signing: QT_ANDROID_KEYSTORE_{PATH,ALIAS,STORE_PASS,KEY_PASS}, e.g. from a local env file
KEYSTORE_ENV="${KEYSTORE_ENV:-$HOME/Android/arunicontrol-keystore.env}"
if [ -z "${QT_ANDROID_KEYSTORE_PATH:-}" ] && [ -f "$KEYSTORE_ENV" ]; then
	set -a; . "$KEYSTORE_ENV"; set +a
fi
SIGN=OFF
[ -n "${QT_ANDROID_KEYSTORE_PATH:-}" ] && SIGN=ON

if [ ! -f "$DEPS_PREFIX/lib/libqca-qt6.so" ]; then
	"${SCRIPT_DIR}/android/build-deps.sh"
fi

"$QT_ANDROID/bin/qt-cmake" -S "${SCRIPT_DIR}" -B "${BUILD_DIR}" -G Ninja \
	-DANDROID_SDK_ROOT="$ANDROID_SDK_ROOT" \
	-DANDROID_NDK_ROOT="$ANDROID_NDK_ROOT" \
	-DQT_HOST_PATH="$QT_HOST_PATH" \
	-DCMAKE_BUILD_TYPE="${BUILD_TYPE:-Release}" \
	-DCMAKE_FIND_ROOT_PATH="$DEPS_PREFIX" \
	-DCMAKE_PREFIX_PATH="$DEPS_PREFIX" \
	-DOPENSSL_ROOT_DIR="$DEPS_PREFIX" \
	-DDEPS_PREFIX="$DEPS_PREFIX" \
	-DWITH_PCH=OFF \
	-DQT_ANDROID_SIGN_APK=$SIGN \
	-DQT_ANDROID_SDK_BUILD_TOOLS_REVISION="${BUILD_TOOLS:-36.0.0}" \
	"$@"

cmake --build "${BUILD_DIR}" --parallel --target apk

find "${BUILD_DIR}" -name "*.apk" -path "*outputs*" -exec ls -la {} \;
