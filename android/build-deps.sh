#!/bin/bash
# Cross-compile the third-party libraries AruniControl Master needs for Android
# (arm64-v8a): OpenSSL, QCA (+ ossl provider), LZO, libjpeg-turbo and libpng.
# zlib comes with the NDK.
#
# Environment (defaults match the layout used by build-android.sh):
#   ANDROID_SDK_ROOT  ~/Android/sdk
#   ANDROID_NDK_ROOT  $ANDROID_SDK_ROOT/ndk/27.2.12479018
#   QT_ANDROID        ~/Qt/6.11.3/android_arm64_v8a
#   QT_HOST_PATH      ~/Qt/6.11.3/macos   (host Qt of the same version)
#   DEPS_PREFIX       ~/Android/arunicontrol-deps/arm64-v8a   (install prefix)
set -euo pipefail

ANDROID_SDK_ROOT="${ANDROID_SDK_ROOT:-$HOME/Android/sdk}"
ANDROID_NDK_ROOT="${ANDROID_NDK_ROOT:-$ANDROID_SDK_ROOT/ndk/27.2.12479018}"
QT_ANDROID="${QT_ANDROID:-$HOME/Qt/6.11.3/android_arm64_v8a}"
QT_HOST_PATH="${QT_HOST_PATH:-$(dirname "$QT_ANDROID")/macos}"
DEPS_PREFIX="${DEPS_PREFIX:-$HOME/Android/arunicontrol-deps/arm64-v8a}"
ANDROID_ABI=arm64-v8a
ANDROID_API=28
WORK="${DEPS_WORK:-$(dirname "$DEPS_PREFIX")/src}"
JOBS="${JOBS:-$(getconf _NPROCESSORS_ONLN)}"

OPENSSL_VER=3.6.4;   OPENSSL_SHA=9bffaa1ad1e07b354c21bd3324ec02fa15579f45a7d0494b3e74bc449b7333ef
QCA_VER=2.3.12;      QCA_SHA=d4a2b3aa0272d73ea0c4cd2140960177fa34ddc2030e59a48ecfb80c757572c3
LZO_VER=2.10;        LZO_SHA=c0f892943208266f9b6543b3ae308fab6284c5c90e627931446fb49b4221a072
JPEG_VER=3.2.0;      JPEG_SHA=6f30092cef9fb839779646608f4ee14ae3cbac989c47fa05e841b0841f09878e
PNG_VER=1.6.58;      PNG_SHA=28eb403f51f0f7405249132cecfe82ea5c0ef97f1b32c5a65828814ae0d34775

mkdir -p "$WORK" "$DEPS_PREFIX"
cd "$WORK"

fetch() { # url sha256
	local f; f=$(basename "$1")
	if ! echo "$2  $f" | shasum -a 256 -c - >/dev/null 2>&1; then
		curl -fsSL --retry 10 --retry-all-errors -C - -o "$f" "$1"
		echo "$2  $f" | shasum -a 256 -c - >/dev/null
	fi
	tar xf "$f"
}

CMAKE_ANDROID=(
	-G Ninja
	-DCMAKE_TOOLCHAIN_FILE="$ANDROID_NDK_ROOT/build/cmake/android.toolchain.cmake"
	-DANDROID_ABI=$ANDROID_ABI -DANDROID_PLATFORM=android-$ANDROID_API
	-DCMAKE_BUILD_TYPE=Release
	-DCMAKE_INSTALL_PREFIX="$DEPS_PREFIX"
	-DCMAKE_FIND_ROOT_PATH="$DEPS_PREFIX"
)

# --- OpenSSL (the android-* targets produce unversioned libcrypto.so/libssl.so,
# which is what Android needs - it cannot package sonames like .so.3)
if [ ! -f "$DEPS_PREFIX/lib/libssl.so" ]; then
	fetch https://github.com/openssl/openssl/releases/download/openssl-$OPENSSL_VER/openssl-$OPENSSL_VER.tar.gz $OPENSSL_SHA
	(
		cd openssl-$OPENSSL_VER
		export ANDROID_NDK_ROOT
		export PATH="$ANDROID_NDK_ROOT/toolchains/llvm/prebuilt/darwin-x86_64/bin:$ANDROID_NDK_ROOT/toolchains/llvm/prebuilt/linux-x86_64/bin:$PATH"
		./Configure android-arm64 -D__ANDROID_API__=$ANDROID_API shared no-tests no-docs no-apps --prefix="$DEPS_PREFIX" --libdir=lib
		make -j"$JOBS" build_libs
		mkdir -p "$DEPS_PREFIX/lib" "$DEPS_PREFIX/include"
		cp libcrypto.so libssl.so "$DEPS_PREFIX/lib/"
		cp -R include/openssl "$DEPS_PREFIX/include/"
	)
fi

# --- LZO (shared, so it can be packaged into the APK like the other libs)
if [ ! -f "$DEPS_PREFIX/lib/liblzo2.a" ]; then
	fetch https://www.oberhumer.com/opensource/lzo/download/lzo-$LZO_VER.tar.gz $LZO_SHA
	cmake -S lzo-$LZO_VER -B build-lzo "${CMAKE_ANDROID[@]}" -DENABLE_STATIC=ON -DENABLE_SHARED=OFF \
		-DCMAKE_POSITION_INDEPENDENT_CODE=ON -DCMAKE_POLICY_VERSION_MINIMUM=3.5
	cmake --build build-lzo --target lzo_static_lib -j"$JOBS"
	mkdir -p "$DEPS_PREFIX/lib" "$DEPS_PREFIX/include"
	cp build-lzo/liblzo2.a "$DEPS_PREFIX/lib/"
	cp -R lzo-$LZO_VER/include/lzo "$DEPS_PREFIX/include/"
fi

# --- libjpeg-turbo (static, PIC; used by LibVNCClient for Tight encoding)
if [ ! -f "$DEPS_PREFIX/lib/libjpeg.a" ]; then
	fetch https://github.com/libjpeg-turbo/libjpeg-turbo/releases/download/$JPEG_VER/libjpeg-turbo-$JPEG_VER.tar.gz $JPEG_SHA
	cmake -S libjpeg-turbo-$JPEG_VER -B build-jpeg "${CMAKE_ANDROID[@]}" \
		-DENABLE_SHARED=OFF -DENABLE_STATIC=ON -DWITH_TURBOJPEG=OFF -DWITH_TOOLS=OFF -DWITH_TESTS=OFF \
		-DCMAKE_POSITION_INDEPENDENT_CODE=ON
	cmake --build build-jpeg -j"$JOBS" && cmake --install build-jpeg
fi

# --- libpng (static, PIC)
if [ ! -f "$DEPS_PREFIX/lib/libpng.a" ]; then
	fetch https://downloads.sourceforge.net/project/libpng/libpng16/$PNG_VER/libpng-$PNG_VER.tar.xz $PNG_SHA
	cmake -S libpng-$PNG_VER -B build-png "${CMAKE_ANDROID[@]}" \
		-DPNG_SHARED=OFF -DPNG_STATIC=ON -DPNG_TESTS=OFF -DPNG_TOOLS=OFF -DPNG_FRAMEWORK=OFF \
		-DCMAKE_POSITION_INDEPENDENT_CODE=ON
	cmake --build build-png -j"$JOBS" && cmake --install build-png
fi

# --- QCA with only the OpenSSL provider (Veyon uses RSA keys + SHA via QCA)
if [ ! -f "$DEPS_PREFIX/lib/libqca-qt6.so" ] && [ ! -f "$DEPS_PREFIX/lib/libqca-qt6_$ANDROID_ABI.so" ]; then
	fetch https://download.kde.org/stable/qca/$QCA_VER/qca-$QCA_VER.tar.xz $QCA_SHA
	"$QT_ANDROID/bin/qt-cmake" -S qca-$QCA_VER -B build-qca -G Ninja \
		-DANDROID_SDK_ROOT="$ANDROID_SDK_ROOT" -DANDROID_NDK_ROOT="$ANDROID_NDK_ROOT" \
		-DQT_HOST_PATH="$QT_HOST_PATH" \
		-DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$DEPS_PREFIX" \
		-DCMAKE_FIND_ROOT_PATH="$DEPS_PREFIX" -DOPENSSL_ROOT_DIR="$DEPS_PREFIX" \
		-DBUILD_WITH_QT6=ON -DBUILD_TESTS=OFF -DBUILD_TOOLS=OFF -DBUILD_PLUGINS=ossl \
		-DQCA_FEATURE_INSTALL_DIR="$DEPS_PREFIX/mkspecs/features" \
		-DQCA_PLUGINS_INSTALL_DIR="$DEPS_PREFIX/lib/qca-qt6"
	cmake --build build-qca -j"$JOBS" && cmake --install build-qca
fi

echo "Android dependencies installed to $DEPS_PREFIX"
ls "$DEPS_PREFIX/lib"
