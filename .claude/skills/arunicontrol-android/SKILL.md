---
name: arunicontrol-android
description: Build, run, test and extend AruniControl Mobile — the Android (and desktop-dev) touch Master of this Veyon fork (Qt 6.11 QML UI in mobile/, Android platform plugin, cross-compiled deps, embedded WireGuard VPN). Use for anything about the APK, the mobile UI, Android build errors, emulator testing, remote access over mobile data (WireGuard/MikroTik, ZeroTier), or adding Veyon features to the mobile app.
---

# AruniControl Mobile (Android)

A native Android **Master** (the controlling side) for AruniControl/Veyon. It reuses
`veyon-core`, the feature plugins and the Master's data models, with a new
touch-first **Qt Quick (QML)** UI. Screens come in over Veyon's own VNC
connection (incremental updates, like the desktop Master), not via the WebAPI,
which is why it is smooth. Android never runs the client side (no VNC server).

## Quick commands

```bash
./build-android.sh                       # deps (first run) + configure + signed APK
# APK: build-android/mobile/android-build/build/outputs/apk/release/android-build-release-signed.apk

# desktop build of the same UI for fast iteration (Homebrew Qt)
cmake -B build -DWITH_MOBILE=ON && cmake --build build --target arunicontrol-mobile
build/mobile/arunicontrol-mobile         # separate config: app name "AruniControl Mobile"

# emulator (arm64 image, headless) + install
~/Android/sdk/emulator/emulator -avd aruni -no-window -no-audio -gpu swiftshader_indirect &
~/Android/sdk/platform-tools/adb install -r <apk>
adb logcat --pid=$(adb shell pidof id.arunika.arunicontrol) | grep " Master  :"   # Veyon logs (tag = app component)
adb exec-out screencap -p > shot.png     # screenshots; drive UI with `adb shell input tap x y`
```

Toolchain (all user-space, no admin): JDK 17 (`brew install openjdk@17`), Android SDK
in `~/Android/sdk` (platforms 35+36, build-tools 36.0.0, **NDK 27.2.12479018 = r27c**, the
one Qt 6.11 is built with), Qt 6.11.3 `android_arm64_v8a` + matching **host Qt**
`~/Qt/6.11.3/macos` (qtbase, qttools, qtdeclarative, qtsvg), deps prefix
`~/Android/arunicontrol-deps/arm64-v8a` from `android/build-deps.sh` (OpenSSL 3.6,
QCA 2.3.12 + ossl provider, LZO, libjpeg-turbo, libpng). Signing keystore
`~/Android/arunicontrol-release.keystore`, passwords in `~/Android/arunicontrol-keystore.env`
(never commit; keep a backup — updates must be signed with the same key).

## Where things live

- `mobile/` — the app. `src/`: `MobileApp` (QML backend "App": auth, rooms, features),
  `ComputerGridModel` (proxy over the Master's ComputerMonitoringModel: roles, search,
  status/room filter, selection), `ScreenImageProvider` (`image://screen/<uid>/<rev>`),
  `RemoteViewItem` (QQuickItem + VncView, GPU texture, touch→RFB), `VpnController`
  (WireGuard/ZeroTier/extra subnets), `main.cpp`. `qml/`: pages + design system.
  `android/`: manifest, launcher icons, `build.gradle` (copy of Qt template + WireGuard
  AAR), `src/id/arunika/arunicontrol/NetworkHelper.java`.
- `plugins/platform/android/` — AndroidPlatformPlugin (Master-only; services, logon,
  input lock are no-ops; canonical app paths; logcat logging; Build.MODEL as name).
- Shared Master sources compiled into the app with `ARUNICONTROL_MOBILE`:
  `master/src/{VeyonMaster,ComputerManager,ComputerControlListModel,...}.cpp`
  (VeyonMaster gets a hidden placeholder QWidget as mainWindow and a settable selection).
- CMake: root `CMakeLists.txt` (`ANDROID` → core + plugins + mobile only, bundled LibVNC,
  no WebAPI/translations/LTO), `plugins/CMakeLists.txt` (Android exclusions),
  `cmake/modules/BuildVeyonPlugin.cmake` (plugin prefix `libveyon-plugin-`).

## Hard-won gotchas (read before debugging)

1. **Plugins on Android** must be named `lib*.so` to be packaged → prefix
   `libveyon-plugin-` (`VEYON_PLUGIN_FILE_PREFIX` in veyonconfig.h, used by PluginManager).
   PluginManager lists `applicationDirPath()`, so native libs must be **extracted**:
   target property `QT_ANDROID_LEGACY_PACKAGING TRUE`.
2. **QCA provider**: QCA looks for `<libpath>/crypto/`, which doesn't exist on Android.
   CryptoCore loads `libqca-ossl.so` via QPluginLoader and `QCA::insertProvider()` (#ifdef
   Q_OS_ANDROID). OpenSSL's android targets already produce unversioned libcrypto.so/libssl.so.
3. **Config was never saved**: LocalStore's non-mac/win branch uses SystemScope = /etc/xdg
   (read-only on Android). Android branch writes an INI in AppDataLocation.
4. **Symlinked /data/user/0** → Veyon's logger (and key checks) refuse symlinked paths.
   Android platform plugin returns canonical paths; log dir set to canonical temp.
5. **First-run directory**: AccessControlProvider instantiates the configured directory
   before MobileApp::applyDefaults() sets NetworkDiscovery → call
   `networkObjectDirectoryManager().reloadConfiguredDirectory()` after applying defaults.
6. **Key name must match the computers' key name**. Android document pickers return
   `content://…/document/1000000018`; get the display name with
   `QFileInfo(contentUri).fileName()` (Qt's Android file engine). The UI shows an
   editable key-name field and self-tests sign+verify before storing.
7. `QT_ANDROID_EXTRA_LIBS` mangles generator expressions (`$<TARGET_FILE:x>` →
   `$<TARGET_FILE;x>`): use plain paths from `get_target_property(... BINARY_DIR)`.
8. Host Qt must include **qtdeclarative** (qmlcachegen/qmltyperegistrar) or `find_package(Qt6 Quick)`
   fails with "Qt6QmlTools not found". aqt often fails on fresh releases ("Failed to download
   checksum"): download the `.7z` from download.qt.io directly, verify `.sha1`, extract with py7zr.
9. Gradle needs JDK 17 in the environment of `cmake --build` (not just configure).
   `androidx.core 1.17` needs compileSdk 36 → install `platforms;android-36`.
10. `Qt6::CorePrivate` (for `QtAndroidPrivate::startActivity`, VPN consent) must be
    `find_package`d explicitly (Qt ≥ 6.9).
11. `Q_OS_LINUX` is also defined on Android — guard X11 code with `&& !defined(Q_OS_ANDROID)`.
    `-Werror` is disabled for ANDROID like APPLE/WIN32.
12. QML: keep QML files at the module root (`QT_RESOURCE_ALIAS`) or singletons (Theme) are
    invisible; `IconImage.name` is FINAL (wrap it); properties named `onXxx` are parsed as
    signal handlers; a *bound* `StackView.initialItem` was ignored → push in onCompleted.
13. ApplicationWindow pads content by the safe area by default (Qt 6.9+) → set its paddings
    to 0 and pad each page with `window.safeTop/safeBottom` (edge-to-edge, Android 15).
14. The screen image provider id is percent-encoded (`%7B…%7D`) — decode before lookup.
15. Desktop dev build must never touch a real installation: app name "AruniControl Mobile",
    keys/user-config/mobile.ini under AppDataLocation; key dir wipe is guarded.

## Features (mobile ↔ Veyon)

Via `controlFeature()` (no desktop dialogs; QML sheets collect the parameters): ScreenLock,
InputDevicesLock, TextMessage, PowerOn/Reboot/PowerDownNow/PowerDownDelayed, UserLogin,
UserLogoff, StartApp, OpenWebsite, InternetAccessControl, AruniMediaMute, DistributeFiles
(Initialize with local paths → Start; content:// files are copied to cache first),
FileCollect (→ `collected/`). Demo "share a student's screen" via
`featureManager().startFeature()` with the master selection set to one computer.
Remote view/control: RemoteViewItem (tap=click, long-press=right click, 1-finger drag=drag,
pinch=zoom/pan, right-edge strip=scroll, soft keyboard via sentinel TextInput, key bar).
Screenshots saved in-app and shared via FileProvider intent.
Not yet: Chat, AruniVoice (needs Qt Multimedia for Android), App monitoring, Spotlight/
Slideshow views, QR import of VPN configs, sharing the phone's own screen.

## Remote access (mobile data)

- Android 12+ removed L2TP/PPTP from the built-in VPN client — don't build on it.
- **Embedded WireGuard** (`com.wireguard.android:tunnel`, Apache-2.0; AAR declares
  `GoBackend$VpnService`). Config imported from .conf/paste; `IncludedApplications =
  <package>` is forced so only AruniControl uses the tunnel. VPN consent via
  `VpnService.prepare` + `QtAndroidPrivate::startActivity`. AllowedIPs /20…/30 subnets are
  written to `NetworkDiscovery/ExtraSubnets` so computers behind the tunnel are discovered.
  MikroTik RouterOS 7: `/interface wireguard`, peers with `client-*` + `show-client-config`.
- **ZeroTier**: official app `com.zerotier.one` only (no join API; libzt is a userspace
  stack + licensing) — the app opens/installs it and explains the steps.
- Only one VPN can be active on Android at a time.

## UI / UX design system (qml/Theme.qml)

Warm sunrise palette (accent `#F2812F`, gradient `#FFB054→#E0602A`), light/dark/auto,
Plus Jakarta Sans (OFL, bundled), Material Symbols Rounded icons (Apache-2.0, `mobile/icons/*.svg`,
fetch more from google/material-design-icons `symbols/web/<name>/materialsymbolsrounded/`).
Components: AppButton (filled/tonal/outline/ghost/danger), IconButton, Chip, InputField,
AppText (display/title/heading/body/label/caption), Card, ListRow, Sheet (bottom drawer),
Toast, EmptyState. All copy is Indonesian. Rules: 52 dp primary buttons, ≥44 dp touch
targets, bottom sheets for every parameter/confirmation, destructive actions red + confirm,
quick-action bar (Kunci/Buka/Pesan/Aksi) always one tap away, selection by long-press.

## Dev hooks (desktop builds only)

`AC_IMPORT_KEY=<private key file>` imports a key at start; `AC_PAGE=auth|settings|rooms|shots|
vpn|actions|remote|welcome` (+ `AC_DELAY` ms) opens a page for screenshots. Window capture on
macOS without accessibility: `CGWindowListCopyWindowInfo` → `screencapture -l <id>`.
