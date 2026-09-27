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
    The Basic `Drawer` does the same (paddings = SafeArea margins) → Sheet.qml zeroes them, else the
    last row of every sheet is clipped by the navigation-bar height.
    Remote page chrome auto-hides after 3.5 s: with adb, tap the screen first, then ⋮ within ~3 s.
14. The screen image provider id is percent-encoded (`%7B…%7D`) — decode before lookup.
15. **Qt for Android's TLS backend dlopen()s `libssl_3.so`/`libcrypto_3.so`** - with plain
    `libssl.so` every `wss://`/https connection fails ("No TLS backend is available"; plain
    `ws://` tests still pass!). build-deps.sh renames + `patchelf --set-soname` them and keeps
    `libssl.so` symlinks for linking.
16. Desktop dev build must never touch a real installation: app name "AruniControl Mobile",
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
Chat / AruniVoice / application monitoring (one computer each): `FeatureSession` subclasses
(`ChatController`, `VoiceController`, `AppMonitorController`, exposed as `App.chat/voice/appMonitor`)
send the plugins' FeatureMessages directly (plugin headers for the enums, feature uid by name) and get
replies via the core signal `FeatureManager::featureMessageReceived` - the plugins' QWidget windows are
never created (the chat plugin only pops its master window for chats started from it). Voice reuses
`plugins/arunivoice/AudioEngine.cpp` (compiled into the app, Qt Multimedia, 16 kHz mono PCM) and asks
for RECORD_AUDIO via `QMicrophonePermission`. UI: ChatPage (bubbles, quick replies, reply banner in
Main.qml + unread badges on Home/card/⋮), VoiceSheet (hold-to-talk, intercom, speaker mute, levels),
AppsPage (3 s polling, foreground app card, close-app confirm). The appmonitoring plugin builds on Android
with `NullApplicationList.cpp` (master side only).
Qt Multimedia for Android isn't in the base aqt install: download
`qt.qt6.6113.addons.qtmultimedia.android_arm64_v8a/…qtmultimedia…ARM64.7z` from download.qt.io, check the
`.sha1`, `bsdtar -xf` it into `~/Qt/6.11.3/android_arm64_v8a` (macOS bsdtar reads .7z). The manifest has no
`%%INSERT_PERMISSIONS` marker on purpose (Qt Multimedia would add CAMERA/BLUETOOTH) - list permissions by hand.
Not yet: Spotlight/Slideshow views, QR import of VPN configs, sharing the phone's own screen.

## Aruni Gateway + Relay (primary way to connect over the internet)

```
phone ──wss──► Aruni Relay (VPS, relay/) ◄──wss (outbound)── Aruni Gateway (client PC, plugins/gateway)──► LAN PCs :11100
```
- **Production relay = Cloudflare Worker** `relay/cloudflare/` (Durable Object `GatewayRoom`
  per gateway id, hibernation, auto-response "ping"→"pong"; deploy `cd relay/cloudflare &&
  npx wrangler deploy`, account randy@rsanggrekmas.com). Live at
  `https://aruni-relay.randymandala.workers.dev`; custom domain `relay.arunihealth.id` (route in
  wrangler.toml) needs the old tunnel DNS record deleted first (error 100117). Gateway sends a
  text "ping" every 30 s (Cloudflare drops idle WebSockets after ~100 s). wrangler dev's workerd
  may not support the newest compatibility_date - keep it a few months back.
- Old path (fallback): Go relay on the Arunika VPS (Ubuntu 20.04 LXC behind the provider's NAT, <vps-host>, internal
  10.0.2.198 - ports 80/443 are NOT forwarded) as systemd `aruni-relay` on 127.0.0.1:8080,
  published through **Cloudflare Tunnel** `aruni-relay` (systemd `cloudflared`,
  `/etc/cloudflared/config.yml`, zone arunihealth.id). Redeploy the binary with
  `TLS=external ./relay/deploy.sh root@<vps-host> relay.arunihealth.id` style or rebuild +
  scp + `systemctl restart aruni-relay`. HTTP 530 from the domain = tunnel/VPS down.
- **Relay** `relay/` (Go, coder/websocket): `/v1/gateway/{id}` control socket (TOFU secret
  hash in `/data/gateways.json`), `/v1/connect/{id}` for masters, `/v1/accept/{id}/{sid}` for the
  gateway's session socket; it only copies binary messages. Docker + Caddy (`docker-compose.yml`,
  `RELAY_DOMAIN`), image via `.github/workflows/relay-image.yml` → `ghcr.io/rsamjkt/aruni-relay`.
- **Crypto** `core/src/AruniTunnel.*`: Noise-KK-style handshake (X25519 static+ephemeral both
  sides, HKDF-SHA256, ChaCha20-Poly1305, counters), pairing token sealed to the gateway key,
  frames `type|stream|payload` (Open/Opened/Close/Data/Hosts/Wake/Ping/Info/KeyRequest/Key).
  Pairing code `ARUNI1:<base64url JSON>` or `arunicontrol://pair?c=…`.
- **Gateway plugin** (ships with every client install): starts only in `veyon-server`
  (`VeyonCore::component() == Server`), single instance per PC via `QLockFile`, state in
  `%GLOBALAPPDATA%/gateway/gateway.json` (id, relay secret, X25519 key, paired devices,
  one-time pairing token, optional *shared access key*), status in `status.json`. Configurator
  page "Aruni Gateway": enable, location name, relay URL, QR (vendored `3rdparty/qrcodegen`,
  built with `-fexceptions`), "give new phones this access key", paired phones + revoke.
  Only private IPs and ports 11100–11199 may be opened (no open proxy); WoL on the LAN.
- **Mobile** `mobile/src/GatewayManager.*`: sites in mobile.ini, device key, one GatewayLink
  per site. `VncConnection::setConnectionRedirector()` (core hook) maps LAN host:port →
  local `127.0.0.1:<listener>` streams; hosts keep their real LAN IPs (demo etc. still work).
  Hosts are published as managed BuiltinDirectory locations (hidden in RoomsPage); skipped
  when the phone is on the gateway's subnet ("onSite"). Tunnelled connections are capped at
  Medium JPEG quality (measured: 241 MB/3.5 min lossless → ~34 KB/s).
  QR via Google Code Scanner (play-services-code-scanner, no camera permission); links via
  `AruniActivity` (intent filter `arunicontrol://pair`). Qt calls URL handlers from the
  Android UI thread → always bounce `handleUrl()` to the main thread.
- Test locally: `/tmp/aruni-relay -listen :8787`, write `gateway.json` `{"enabled":true,
  "relayUrl":"ws://<mac-lan-ip>:8787","sharedKey":"<key>"}`, run `build/server/veyon-server`,
  create a pairing code, `adb shell am start -a android.intent.action.VIEW -d 'arunicontrol://pair?c=…'`.
  Remove the test `gateway/` dir afterwards.

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

## Website unduhan resmi (website/)

- Live: https://arunicontrol.arunihealth.id (Worker `arunicontrol`, static assets + R2 `arunicontrol-unduhan`).
- `/unduh/{windows,windows-zip,macos,android,sha256}` → file rilis di R2 (`<tag>/<file>`); mendukung Range (resume).
  Repo GitHub private → link rilis GitHub TIDAK bisa dipakai publik, makanya file di R2.
- Edit `website/index.src.html`, lalu `python3 build.py` (inline ikon dari mobile/icons) + `npx wrangler deploy`.
- Rilis baru: upload ke R2 dengan awalan tag, ubah `RELEASE` di `src/worker.js` + versi/ukuran di HTML (lihat website/README.md).
- Screenshot: Configurator punya `ARUNI_SCREENSHOT`/`ARUNI_SCREENSHOT_PAGE` (render diri sendiri, tanpa izin Screen Recording);
  Windows lewat workflow `windows-screenshots.yml` (hasil di draft release "CI screenshots", hapus setelahnya — kuota artifact sering penuh).
- Di runner Windows beberapa `veyon-wcli config set` berturut-turut saling menimpa → pakai satu `config import`.

## Laptop jelajah / roaming laptops (1.3.0 "Diana")

Laptops taken home stay monitored through the office Aruni Gateway (hub).
- **Agent = reverse master**: `plugins/gateway/RoamingAgent` (runs in veyon-server, lock `aruni-roaming.lock`)
  connects to the relay as a *master* of the hub (`/v1/connect/<hubId>`) with `MasterHandshake(..., agent=true)`
  (flag `0x02` in M1). The enrollment token (`ARUNIL1:` code, reusable, `GatewayState::enrollmentToken`) is sent only
  until the first success (`roaming.registered`) — a removed laptop cannot re-enroll itself.
- **Hub**: `GatewaySession` with `isAgent()` → `GatewayService::agentConnected` listens on `0.0.0.0:<11600+slot>` (LAN peers only) and
  forwards each accepted TCP connection as a stream *to* the agent (`forwardToAgent`, Open(11100,"127.0.0.1")).
  Agent only allows loopback targets. `TunnelEndpoint` = shared stream/flow-control code (streams opened by the hub
  wait for `Opened` before reading).
- **Phone**: hub `hostsJson` adds online, non-local agents as `{host:"<8-byte-hex>.roam.aruni", roaming:true}`;
  `GatewaySession::mapTarget` maps that host to `127.0.0.1:<slot port>`. Mobile puts them in room
  "<site> · Di luar kantor" (`MobileApp::publishGatewayHosts`). Entries `IP:11601..11698` found by discovery are
  filtered from hostsJson (`isRoamingForward`) to avoid duplicates.
- **Desktop Master**: hub serves JSON on port 11699 (`{site, laptops:[{name,port,id}]}`); `NetworkDiscoveryDirectory`
  queries 11699 on every found host and publishes location "<site> - outside the office" with hosts `gatewayIP:port`
  (Veyon parses `host:port`).
- **"local"** (1.3.1): the agent probes the hub's LAN addresses (sent in gatewayInfo "addresses") on the directory
  port and checks the returned gateway `id` → hello `local:true`; the hub hides local agents. Matching IPs was wrong
  (home and office often share 192.168.1.0/24). `ARUNI_ROAMING_FORCE_AWAY=1` skips the probe for tests.
- **Ports** (1.3.1): forwards `11600+slot` (11601–11698), directory 11699 — 11100–11499 belong to Veyon's per-session
  server/VNC/worker/demo ports (multi-session mode). Forward + directory sockets accept LAN peers only.
- **Review fixes 1.3.1**: never set `m_stateModified` after `GatewayState::update()` (the service must reload, or a laptop
  removed in the Configurator stays authorized); `TunnelEndpoint::resetStreams()` on every agent reconnect (pending-byte
  counter + lookup epoch); agent refuses roaming when `LocalConnectOnly` or an access rule with *AccessFromLocalHost* is
  active (forwarded connections arrive from 127.0.0.1); relay alarm uses the earliest deadline and is not postponed.
- CLI: `veyon-cli gateway enroll <code> | leave | status | enrollmentcode | runroaming` (foreground agent for tests).
  Windows installer: `/S /ENROLL=<code>`; firewall rule now `localport=11100,11601-11699`.
- Testing on one Mac: `ARUNI_GATEWAY_DIR=/tmp/agentstate ARUNI_ROAMING_FORCE_AWAY=1 veyon-cli gateway runroaming`
  acts as a second computer (its own state dir; hide addresses so the hub doesn't treat it as local).
- Relay gotcha: after a gateway reconnect the old control socket can linger in the Durable Object (closing, dead peer);
  sessions must be announced to **all** control sockets (fixed in `relay/cloudflare/src/index.js`). Limit raised to 1000
  sessions per gateway (each agent holds one permanently). Agent pings every 40 s.
- Not available while away: Demo (clients connect back to the master) and Wake-on-LAN.

## 1.4.0 "Elena": updates, alerts, history, Indonesian UI

- **Indonesian UI**: `translations/veyon_id.ts` fully translated (incl. Qt standard buttons via `core/src/QtStandardTexts.h`,
  context QPlatformTheme). `TranslationLoader` defaults to Indonesian when no language is configured. Builds use
  `-DWITH_TRANSLATIONS=ON`; lupdate is no longer run by normal builds (explicit `<lang>_ts` target). Mac bundle:
  `Contents/Resources/translations`; Windows: `translations\` next to the exes. Update strings with
  `lupdate -locations none -no-obsolete @files.txt -ts translations/veyon_id.ts` (exclude mobile/, its source is Indonesian).
- **Auto update** (`plugins/autoupdate`, runs in veyon-server, lock `aruni-update.lock`): reads
  `https://arunicontrol.arunihealth.id/unduh/versi.json` (Worker builds it from `RELEASE` + SHA256SUMS in R2), every
  6 h (first after 3 min; Configurator "Periksa sekarang" drops `%GLOBALAPPDATA%/update/check-now`). Windows: runs
  `setup.exe /S /UPDATE` (installer stops service + kills veyon-*.exe first; never `taskkill /T` — the installer is a child
  of veyon-server). macOS: ditto-extract, swap `AruniControl.app` ↔ `.app.old`, `launchctl kickstart -k gui/$UID/<label>`.
  Android: `mobile/src/AppUpdater` + `UpdateHelper.java` (Qt FileProvider `.qtprovider`, REQUEST_INSTALL_PACKAGES).
  Cloudflare blocks the Python-urllib user agent — clients send `AruniControl/<ver> (<platform>)`.
- **macOS signing**: stable self-signed identity "AruniControl Code Signing" in `~/Library/Keychains/aruni-signing.keychain-db`
  (password + paths in `~/Library/AruniSigning/signing.env`, cert/key in `~/Library/AruniSigning/`). DR becomes
  `certificate root = H"…"`, so TCC permissions survive updates. Losing it = users re-grant once. Keychain must be in the
  user search list (package-macos.sh adds it).
- **Gateway tabs** (Gateway / Laptop / Notifikasi / Riwayat): `ActivityLog` (`activity.jsonl`, 8 MB rotation, CSV export with
  BOM + `;`, CLI `veyon-cli gateway activity [file.csv] [days]`), `Notifier` (Telegram bot; alerts: laptop offline > N h +
  back online, refused access (max 1/10 min), new laptop), `ScreenshotScheduler` (gateway connects to agents like a Master
  with a private key — `sharedKeyName` or the first private key — and waits for a non-uniform frame; the first updates are black).
  Agents report the foreground app (compiled from plugins/appmonitoring platform code).
- Configurator screenshot helpers: `ARUNI_SCREENSHOT`, `ARUNI_SCREENSHOT_PAGE`, `ARUNI_SCREENSHOT_TAB`, `ARUNI_SCREENSHOT_HEIGHT`.

## 1.5.0 "Fiona": site blocking, access log, timelapse, broadcast, CSV import

- **Site blocking** `plugins/sitefilter` (feature uid c3a5e2d1-…): SetSites/Query/Status. Windows: marked block in the hosts
  file (+ www./m./mobile.) and Chrome/Edge `DnsOverHttpsMode=off`, Firefox DNSOverHTTPS policy, `ipconfig /flushdns`.
  macOS: osascript "with administrator privileges" **asynchronously** (a synchronous prompt would freeze veyon-server);
  only works from a server running in the user's GUI session (LaunchAgent) — started from a plain shell it fails silently.
  State in `%GLOBALAPPDATA%/sitefilter.json`. Mobile copy of the presets in `mobile/src/SiteFilterController`.
- **Access log**: `core/src/AccessLog` written by `ComputerControlServer` (connected/disconnected+seconds/auth_failed/
  access_denied/feature — features with Meta/Builtin flags skipped, same feature+command at most every 10 min) to
  `%GLOBALAPPDATA%/logs/access.jsonl`; `plugins/accesslog` serves it (Query/Entries) with a Master dialog + CSV.
  `AuthenticationCredentials::announcedUsername` lets the gateway's own visits appear as "Aruni Gateway" (filtered).
- **Gateway `MonitoringCollector`** (replaced ScreenshotScheduler): visits online roaming laptops and — with
  "Semua komputer kantor" — every host of the gateway's network discovery directory (max 4 parallel, 60 s timeout):
  screenshots every N min, access logs every 15 min into the activity history (`access.*` events, original time via
  `ActivityLog::appendAt`; read() sorts by time). Cursors in `gateway/access-cursors.json`. Tab "Rekaman" = TimelapseView.
  A computer with two IPs appears twice (discovery lists addresses).
- **Voice broadcast** feature `AruniVoiceBroadcast` (uid 6d3f8a52-…) in plugins/arunivoice: one engine, 100 ms chunks to all
  targets, worker shows `BroadcastNotice` (paints its own rounded background — stylesheets aren't drawn on translucent windows).
- **CSV import/export** `core/src/ComputerListFile` (Ruangan|Nama|Alamat IP|MAC; separator + header detection), used by
  Configurator → Lokasi & komputer and the app (Ruangan & komputer → import icon).
- Known: the app froze (ANR) on the emulator when typing into the site-block field opened over the live remote page;
  not reproduced from the Home sheet — verify on a real phone.
