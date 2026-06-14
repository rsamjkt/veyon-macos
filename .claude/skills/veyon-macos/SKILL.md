---
name: veyon-macos
description: >
  Build, package, debug and extend the experimental macOS (Apple Silicon) port of
  Veyon in this repository. Use whenever working on Veyon for macOS: the mac
  platform plugin, the mac VNC server (screen capture / remote input), CMake/Qt/QCA
  build issues, .app bundling, launchd service, TCC permissions, or the
  build-macos.sh / package-macos*.sh scripts. NOT for Linux/Windows Veyon.
---

# Veyon macOS port

Upstream Veyon officially supports **Linux, Windows, Android only**. This repo adds
a native **macOS (Apple Silicon)** port. Everything below is macOS-specific.

## Quick commands

```bash
./build-macos.sh            # configure + build everything into build/
./package-macos-local.sh    # thin apps for THIS dev machine (uses Homebrew Qt)
./package-macos.sh          # self-contained Veyon.app for OTHER Macs (bundles Qt)
```

Run a single component during development:
```bash
eval "$(/opt/homebrew/bin/brew shellenv)"
cmake --build build --parallel --target <target>   # e.g. mac-platform, mac-vnc-server, veyon-core
cp build/plugins/.../X.so build/lib/veyon/          # refresh runtime plugin dir, then re-run the app
```

Toolchain (Homebrew): `cmake pkg-config ninja qt qca openssl@3 jpeg-turbo lzo libpng libvncserver`.

## Where the macOS code lives

- `plugins/platform/mac/` — the **MacPlatformPlugin** (7 function groups: Core, Filesystem,
  InputDevice, Network, Service, Session, User). Model on the Windows plugin, not X11.
- `plugins/vncserver/mac/` — **MacVncServer** (libvncserver) + `MacScreenCapture.mm`
  (ScreenCaptureKit) + `MacVncInput.cpp` (CGEvent keyboard/mouse injection).
- CMake hooks: root `CMakeLists.txt` (APPLE is first-class, NOT VEYON_BUILD_LINUX),
  `plugins/platform/CMakeLists.txt`, `plugins/vncserver/CMakeLists.txt`, `plugins/CMakeLists.txt`
  (excludes `ldap`), `cmake/modules/SetDefaultTargetProperties.cmake` (no -Werror / -no-undefined on Apple).
- `README.macos.md` — status table, permissions, packaging, caveats.

## Hard-won gotchas (read before debugging)

1. **`CGDisplayCreateImage` is REMOVED in macOS 15+** (hard error, not just deprecated).
   Screen capture MUST use **ScreenCaptureKit** (`SCScreenshotManager`) — see `MacScreenCapture.mm`.
2. **qca + Homebrew duplicate-Qt crash.** The self-contained `Veyon.app` crashes on any machine
   that ALSO has Homebrew `qca` installed: QCA loads its provider plugins from the baked-in
   Homebrew path *in addition* to the bundled ones, pulling a 2nd Qt → SIGSEGV in
   `QCA::get_logger()`. `QCA_PLUGIN_PATH` and `brew unlink qca` do NOT fix it (verified).
   → On the dev machine use **`package-macos-local.sh`** (thin apps using Homebrew Qt) or the
   `build/` binaries. The self-contained bundle is only for machines without Homebrew.
3. **Framework version dirs differ.** Qt frameworks use `Versions/A`, but `qca-qt6` uses
   `Versions/2`. When rewriting install names in `package-macos.sh`, preserve the real
   suffix after `.framework/` — never hard-code `Versions/A`.
4. **Plugin discovery suffix.** Veyon filters plugins by `*-platform` + `VEYON_SHARED_LIBRARY_SUFFIX`.
   On macOS that must be the MODULE suffix `.so` (not `.dylib`). Set in root CMakeLists for APPLE.
5. **rpath cleanup when bundling.** macdeployqt leaves Homebrew rpaths and 2nd-level deps; strip
   any `/opt/homebrew` LC_RPATH and relink remaining absolute deps to `@rpath`, else Qt loads twice.
6. **Config is user-level on macOS.** `LocalStore` maps System scope → `QSettings::UserScope`
   (`~/Library/Preferences/com.veyon-solutions.Veyon.plist`) so the Configurator needs no admin.
   The Configurator's admin self-elevation is `#ifndef Q_OS_MACOS`. `globalAppDataPath()` is
   `~/Library/Application Support/Veyon`.
7. **The "Service" is a launchd LaunchAgent.** `MacServiceFunctions` writes
   `~/Library/LaunchAgents/io.veyon.server.plist` running **veyon-server** (resolved robustly for
   build-tree AND bundle layouts) and controls it with `launchctl bootstrap/kickstart/bootout gui/$UID`.
   No admin needed.
8. **TCC permissions.** The controlled-client `veyon-server` needs **Screen Recording** (capture)
   and **Accessibility** (input). Granted per-binary; a process must be (re)started AFTER granting.
   launchd-launched servers get cleaner TCC attribution than shell-launched ones. Check with
   `CGPreflightScreenCaptureAccess()` (logged at server start).
9. **`-fno-exceptions`** is on project-wide — no try/catch, including in `.mm` files (Objective-C
   exceptions off too; `MacScreenCapture.mm` is built with `-fobjc-arc -fexceptions`).
10. **`QT_USE_QSTRINGBUILDER`**: `auto x = strA + strB` deduces a lazy `QStringBuilder`, not
    `QString`. Use an explicit `const QString` when you need `.contains()` etc.

## Architecture notes

- VNC ports: **11100** = VncProxyServer (Master entry, Veyon protocol+auth), **11200** =
  mac-vnc-server (raw RFB), **11300** = FeatureWorkerManager. The proxy on 11100 proxies to 11200.
- `mac-vnc-server` is auto-selected because it's the only VNC server plugin with
  `Plugin::ProvidesDefaultImplementation` that supports the `console` session type.
- Screen capture runs on a **worker thread** so slow/denied captures never stall the RFB loop.
- Verifying end-to-end without the GUI: read the RFB greeting from 11200 with a real socket
  (NOT `nc </dev/null`, which closes too early — use Python `socket.recv`).

## Still TODO / not implemented

Blocking local input during control, screen lock, signed/notarized distribution build, a clean
self-contained bundle that also runs on a Homebrew dev machine (needs QCA built from source with a
bundle-relative provider path), LDAP plugin (excluded — system OpenLDAP removed from macOS).
