# Veyon macOS port (experimental / work in progress)

Upstream Veyon officially supports **Linux, Windows and Android only**. This
tree contains an *in-progress* native macOS port. It is not complete and not
endorsed by the Veyon project.

## Current status

Verified on: macOS 26.4 (Apple Silicon / arm64), Qt 6.11, libvncserver 0.9.15.

| Area | State |
|------|-------|
| CMake: `APPLE` is a first-class platform (no longer builds the Linux/X11 plugin) | ✅ done |
| `plugins/platform/mac` platform plugin scaffold (7 function groups) | ✅ builds & loads (stubs + POSIX) |
| Veyon Core library (`libveyon-core.dylib`) | ✅ builds |
| **Full tree builds** (core, master, server, service, configurator, cli, worker, 16 plugins) | ✅ compiles cleanly |
| Veyon CLI runs, loads all plugins incl. `MacPlatformPlugin` | ✅ verified (`veyon-cli plugin list`) |
| Veyon Master / Configurator launch on macOS | ✅ launch & initialise (run from a real Terminal/`open`) |
| Veyon Server VNC plugin (`mac-vnc-server`) — screen capture + remote input | ✅ implemented (ScreenCaptureKit + CGEvent) |
| &nbsp;&nbsp;↳ screen capture | ✅ ScreenCaptureKit (`SCScreenshotManager`) — needs Screen Recording permission |
| &nbsp;&nbsp;↳ remote keyboard/mouse | ✅ `CGEventPost` — needs Accessibility permission |
| &nbsp;&nbsp;↳ auto-selected by `veyon-server` for `console` sessions | ✅ verified |
| &nbsp;&nbsp;↳ serves the RFB protocol (greets `RFB 003.008` on the VNC port) | ✅ verified end-to-end |
| &nbsp;&nbsp;↳ capture runs on a worker thread (RFB loop stays responsive) | ✅ |
| Self-contained `Veyon.app` bundle (Qt + plugins + deps, ad-hoc signed) | ✅ `./package-macos.sh` (runs on a clean Mac) |
| LaunchAgent to auto-start the server in the user session | ✅ template generated (`dist/io.veyon.server.plist`) |
| LDAP plugin | ⏭️ excluded on macOS for now (system OpenLDAP removed) |
| Automatic logon / service via launchd | ❌ stubbed |
| Blocking local input during remote control / screen lock | ❌ not implemented |
| Signed/notarized distribution build | ❌ ad-hoc signature only (per-build TCC identity) |

## First-run setup (important)

AruniControl Master needs **authentication keys** to start (otherwise it shows
"Authentication impossible" and exits). Create a key pair once:

```bash
./build/cli/veyon-cli authkeys create master
```

(or set up logon authentication in the Configurator). The keys are stored under
`~/Library/Application Support/AruniControl/keys/`.

## Add-ons

Open-source add-ons built for AruniControl (inspired by, not derived from,
Veyon's commercial add-ons):

- **Network Discovery** (`plugins/networkdiscovery`) — a NetworkObjectDirectory
  that scans the local subnet(s) for hosts running an AruniControl Server
  (port 11100) and lists them automatically. Enable it in the Configurator under
  the *network object directory* plugin setting, or via:
  `veyon-cli config set NetworkObjectDirectory/Plugin "{3c5e9a14-2b7d-4e6f-8a1c-9d0f2e4b6c81}"`.
  Requires the macOS **Local Network** permission (granted to the app on first run).

- **Screen Recorder** (`plugins/screenrecorder`) — records the screens of the
  selected computers to H.264 `.mov` files using AVFoundation. Toggle the
  *Record screen* button in the Master toolbar; click again to stop. Videos are
  saved to `~/Movies/AruniControl/`.

- **Chat** (`plugins/chat`) — two-way text chat between the Master and the users
  of selected computers. Click *Chat* in the Master toolbar to open a chat window
  per computer; messages are shown in the user's session and they can reply.
  (The client side relies on the worker running in the user's GUI session.)

- **Internet Access Control** (`plugins/internetaccess`) — toggle *Block internet*
  from the Master to restrict internet access on selected computers (e.g. during
  exams). The client loads a `pf` ruleset that blocks outbound traffic to the
  public internet while keeping loopback/LAN reachable. **Requires administrator
  rights on the client** (pf firewall) — for unattended deployment grant `pfctl`
  passwordless sudo or run the service as a privileged LaunchDaemon.

## Packaging a distributable app bundle

```bash
./build-macos.sh      # build everything
./package-macos.sh    # assemble dist/Veyon.app (self-contained)
```

`package-macos.sh` collects every executable, `libveyon-core`, all plugins, Qt,
QCA (incl. its crypto provider plugins), OpenSSL and libvncserver into
`dist/Veyon.app`, rewrites all install names/rpaths to be bundle-relative
(0 references to Homebrew remain), and ad-hoc code-signs it. Install with
`cp -R dist/Veyon.app /Applications/`.

### Auto-starting the server (controlled client)

The server must run inside the logged-in GUI session, so it is a **LaunchAgent**
(not a root LaunchDaemon):

```bash
cp -R dist/Veyon.app /Applications/
cp dist/io.veyon.server.plist ~/Library/LaunchAgents/
launchctl load ~/Library/LaunchAgents/io.veyon.server.plist
```

### ⚠️ Important caveat for the *build* machine (has Homebrew)

`Veyon.app` is self-contained and runs correctly on a Mac that does **not** have
Homebrew's `qca` installed (verified end-to-end: CLI `rc=0`, the Master GUI
starts, RSA crypto works, no duplicate-Qt). On the build machine itself, QCA
hard-codes its build-time provider directory (`$(brew --prefix qca)/lib/qt/plugins`)
and loads the Homebrew `qca` crypto plugins from there in *addition* to the
bundled ones. Those Homebrew plugins drag in a second copy of Qt and crash the
app. This is a **build-host-only** conflict (neither `brew unlink qca` nor
`QCA_PLUGIN_PATH` avoid it — the path is absolute and baked into QCA).

To use Veyon on the machine you built on, either run the binaries directly from
`build/` (they link Homebrew's single Qt/QCA and work fine), or build the
**thin** double-clickable apps which reuse the Homebrew Qt instead of bundling
their own:

```bash
./package-macos-local.sh
# produces dist/Veyon Master.app and dist/Veyon Configurator.app
```

| Script | Output | Use it for |
|--------|--------|-----------|
| `package-macos.sh` | self-contained `Veyon.app` (bundles Qt) | deploying to **other** Macs (no Homebrew) |
| `package-macos-local.sh` | thin `Veyon Master.app` / `Veyon Configurator.app` | running on **this** dev machine (has Homebrew) |

The thin apps reference `libveyon-core` in `build/core` and the Homebrew Qt in
`/opt/homebrew`, so keep the `build/` directory in place. A fully clean
self-contained build that also runs on the dev machine would require building
QCA from source with a bundle-relative provider path (future work).

## Using the Mac as a controlled client (Veyon Server)

The `mac-vnc-server` plugin turns the Mac into a Veyon client that can be
viewed and controlled from a Veyon Master. macOS gates this behind two
privacy permissions which must be granted to the **veyon-server** binary:

1. **Screen Recording** — required to capture the screen.
   System Settings → Privacy & Security → Screen Recording → add/enable
   `build/server/veyon-server`.
2. **Accessibility** — required to inject remote keyboard/mouse input.
   System Settings → Privacy & Security → Accessibility → add/enable
   `build/server/veyon-server`.

Because the build produces a plain unsigned executable (not a signed `.app`
bundle yet), macOS may not show the permission prompt automatically — add the
binary to the lists manually with the **+** button. After granting both
permissions, start the server:

```bash
./build/server/veyon-server
```

then connect from a Veyon Master (on this or another machine) to this Mac's IP.
Without the permissions the server still runs but shows a black screen and
ignores remote input.

> Known limitations of the current server: full-frame updates (no dirty-region
> diffing beyond a whole-frame change check), single (main) display only, no
> local-input blocking, and permissions/packaging rely on a future signed
> `.app` bundle for a smooth prompt-based flow.

> Note: the Configurator tries to relaunch itself with administrator
> privileges at startup (like the Windows version), which triggers a macOS
> password prompt via AppleScript. Set `VEYON_CONFIGURATOR_NO_ELEVATION=1` to
> skip that and run with normal privileges.

### What the macOS platform plugin implements today

- **Core**: native logging via `syslog`, reboot / power down via AppleScript,
  run-as-admin via `osascript ... with administrator privileges`,
  `getApplicationName` via `libproc`, window raising via Qt. Screen-saver
  inhibition and kiosk UI state are TODO (need IOKit / Presentation Options).
- **Filesystem**: POSIX `chown`/`stat`/`open` (`O_NOFOLLOW`), app-data paths.
- **Network**: `ping` via `/sbin/ping`, TCP keep-alive via macOS socket options,
  default route interface via Qt.
- **Input devices / keyboard shortcut trapper**: stubbed (blocking local input
  on macOS needs a CGEventTap + Accessibility permission).
- **Service**: stubbed (launchd integration TODO).
- **Session**: single console session, uptime via `sysctl(KERN_BOOTTIME)`.
- **User**: current user / group enumeration via `getpwuid`/`getgrouplist`.
  Authentication and automated logon are TODO (Open Directory / PAM).

## Building

1. Install the toolchain (one time):

   ```bash
   /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
   eval "$(/opt/homebrew/bin/brew shellenv)"
   brew install cmake pkg-config ninja qt qca openssl@3 jpeg-turbo lzo libpng libvncserver
   ```

2. Configure + build the first-milestone components:

   ```bash
   ./build-macos.sh
   ```

   Binaries are produced under `build/` (`veyon-master`, `veyon-configurator`,
   `veyon-cli`, and the `veyon-core` library).

   To attempt a full-tree build, edit `build-macos.sh` and remove the
   `--target ...` list from the final `cmake --build` call.

## Roadmap

- **Milestone 1** – Run Veyon **Master** + **Configurator** on macOS to monitor
  and control existing Linux/Windows Veyon clients (viewer side only).
- **Milestone 2** – Make a Mac usable as a Veyon **client**: screen capture
  (ScreenCaptureKit), remote input injection (`CGEventPost`), session/user
  integration, and the required Screen Recording + Accessibility entitlements.
- **Milestone 3** – Packaging: `.app` bundles (`MACOSX_BUNDLE`), `macdeployqt`,
  code signing, launchd `LaunchDaemon`/`LaunchAgent` for the service.
