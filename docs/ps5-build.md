# Unified PS5 build and browser fallback

The native SDL app remains the UI reference. `make ps5` requires the PS5 Payload
SDK and its SDL2/curl/image/font dependencies. The SDK is installed in WSL at
`/opt/ps5-payload-sdk`; set PS5_PAYLOAD_SDK accordingly. A Linux build does not
prove PS5 rendering works. On the tested console the original native startup
failed at SDL_CreateRenderer, after accelerated and software renderer attempts.
Native startup checkpoints are written to /data/wolodrive-startup.log.

### Renderer startup investigation (2026-10-21)

Device logs supplied on this date show SDL/window initialization succeeded but
both automatic renderer attempts failed. They do not identify a storage permission
failure. SDL's `SDL_CreateRenderer(window, -1, ...)` replaces backend errors with
`Couldn't find matching render driver`, so these logs cannot establish the
underlying VideoOut or direct-memory failure.

The PS5 startup path now selects the compiled `software` driver by explicit
index and disables GPU framebuffer emulation. The PS5 SDL port presents its CPU
surface through VideoOut; this path does not require an OpenGL/Vulkan stack.
It logs the SDL version, video driver, renderer flags and original backend error.
Desktop selection and the browser/static-hosting contract are unchanged. This is
a startup correction and diagnostic build, **not a verified fix on the console**.
If the backend still fails, further fixes depend on the newly exposed error.

The follow-up device log confirmed that this path ran and failed specifically at
`sceKernelAllocateMainDirectMemory: Resource temporarily unavailable`. The user
selected a filesystem bridge rather than native-application repackaging. The
browser fallback now accesses storage through the payload's native file backend
without depending on SDL display initialization. The native renderer remains
unfixed in this launch environment; do not describe the bridge as a graphics fix.

Research: [SDL renderer selection](https://github.com/ps5-payload-dev/SDL/blob/ee4c47dc0d617b3bc8f35108f9956baf228a1322/src/render/SDL_render.c),
[PS5 surface presentation](https://github.com/ps5-payload-dev/SDL/blob/ee4c47dc0d617b3bc8f35108f9956baf228a1322/src/video/ps5/SDL_ps5video.c),
and [SDK package's Mesa/VideoOut patch](https://github.com/ps5-payload-dev/pacbrew-repo/blob/master/SDL2/0001-ps5-opengl-opengles-and-vulkan-through-mesa.patch).
The installed SDK's static archive also contains the software renderer and the
lazy `PS5_VideoOut_Open` path; upstream sources alone do not prove a device fix.

Run `make test-ps5-renderer` for a real SDL dummy-surface drawing check and an
injected backend failure proving its message survives. This test does not
exercise PS5 VideoOut. Rollback removes the PS5-specific helper call and restores
the previous automatic selection; no SDK install or user files are modified.

Replace `/data/pldmgr/payloads/poldrive/poldrive.elf` with the rebuilt
`build/poldrive.elf` and launch through Payload Manager once. If native starts,
verify Files opens `/data`, parent navigation and a disposable folder listing.
If the browser opens again, retrieve fresh `/data/wolodrive-startup.log` and
`/data/poldrive-launcher.log`; look for `PS5 software renderer backend` and its
specific error. PS5 display, storage access and parity remain unverified until
that device check. **AI Studio deployment not verified.**

Local verification for this change: `make test-ps5-renderer`, `make build-linux`,
`make test-native-snapshot`, PS5 SDK `make ps5`, `npm run lint`, `npm run test:web`,
`npm run build:static` (through `make ps5`) and `python tests/check_static_preview.py`
passed. The checker served root HTML, JS/CSS and 16 artwork assets over ordinary
HTTP. Browser smoke checks covered Home/details, search filtering, catalog
categories, Files and Settings, with no captured console errors. The existing
full `make test` remains blocked by obsolete symbols in
`tests/test_regressions.c`, including `DESCRIPTION_BUTTON`, `library_alpha_bounds`
and `load_additional_catalog`. No PS5 device or native/browser visual parity
verification was performed for this startup change.

## One distributable: poldrive.elf

Run `make ps5` (or `make ps5-shortcut`) with the SDK available. Both now build
the same unified `build/poldrive.elf`. Distribute only that file: it contains
the native application, Media shortcut installer and embedded web fallback.
The existing static build workflow is preserved. Helper objects/ELFs in build/
are internal intermediates; an older `build/wolodrive.elf` is not updated by
these targets and must not be mistaken for the new distributable.

Startup checks the registered dashboard shortcut marker before dispatching the
isolated installer to the console's loopback ELF loader on port 9021.
When the marker is absent, this **first invocation is installer-only**: it waits
up to 15 seconds for the installer to confirm registration, then exits without
opening either native UI or browser. A failed transfer or unconfirmed registration
also exits without opening the app; see /data/poldrive-launcher.log and
/data/wolodrive-shortcut.log, then retry the saved payload through Payload Manager.
If the marker was already present at invocation start, the launcher continues
into the native application while the helper checks for shortcut updates.
A normal native exit does not open the browser. A nonzero native startup return
opens the bundled browser fallback with a translated native-startup-failure
notice and an optional filesystem bridge. This handles startup errors, not
process crashes or hangs. Files > Open folder browses `/data` through that bridge.
It does not fix the previously observed SDL renderer problem or claim PS5 parity.
Native success does not start an additional web server in the background.

App-service dependencies remain isolated in the helper; they are not linked into
the SDL/server executable. Helper transfer success does not prove registration
success. An unavailable loader does not block native startup. Check the logs
below for the actual registration and native outcomes.

`make ps5-web` remains available for browser-only diagnostics and produces
`build/wolodrive-web.elf`; it cannot overwrite `build/poldrive.elf`. Building the
web diagnostic ELF still embeds its own shortcut helper, but the dashboard link
always targets the unified file at the installation path below. To revert the
combined launcher locally, restore the earlier Makefile and remove the new
launcher/helper embed/test sources; the ordinary AI Studio preview is unchanged.

Save this one ELF as
/data/pldmgr/payloads/poldrive/poldrive.elf and launch it through
Payload Manager. The embedded helper prepares the PolDrive red P icon under
Home / Media automatically. No separate shortcut-installer ELF needs copying.
Subsequent launches skip registration when the recorded shortcut metadata and
icon match. Failed updates invalidate the registration marker and retry later.
Folder and filename are case-sensitive. This build repairs the previous
uppercase `Poldrive` shortcut when first activated from Payload Manager. Replace
the saved ELF and activate it there once before using the Media icon again;
closing the browser does not remove the icon. The initial activation installs the dashboard shortcut and exits without
opening the UI. Subsequent launches from the Media tile or Payload Manager
open the app (reusing a matching fallback server). If the shortcut was already
registered before upgrading the ELF, the upgraded payload opens the app immediately.
The installer refuses an existing title without its ownership marker and writes
only this title's files. It does not uninstall existing apps. If TitleDir is
unavailable it uses Payload Manager's registration-scan fallback. The fallback
and browser API both returned 0 on firmware 13.20 with the bundled helper.

Old PolDrive diagnostic builds were moved outside Payload Manager's scan tree
into /data/wolodrive-debug-backup-20261020-1920. Reload Payload Manager to refresh
its list. Other applications and payloads were not moved.

The shortcut calls Payload Manager's loopback /loadpayload: route to the saved
ELF. Payload Manager on port 8084 and the ELF loader must be active after each
jailbreak. The installer does not alter autoload or jailbreak settings. No user's
LAN address is compiled into the app.

When the browser fallback runs, its server listens on all IPv4 interfaces,
preferring port 8097 and selecting a
free port if occupied. /data/wolodrive-web.endpoint records its port and PID.
Relaunch validates the loopback health endpoint against the server protocol and
embedded asset SHA256, then reopens the matching running service. Different or
stale services are not reused. LAN clients open http://<PS5-IP>:<logged-port>/.
Different ports have separate browser localStorage preferences.

The listener serves embedded GET/HEAD assets and a health endpoint to console
and LAN clients. PS5 builds additionally expose a session-protected
filesystem API using the native file backend; see [file-manager.md](file-manager.md).
Console-local Files displays the pairing code required for PC file editing.
There are no package-download, installation, proxy or mounting routes.
It embeds the ordinary browser preview, without runtime Node/Vite or PC assets.
Optional remote artwork still depends on its existing sources/fallbacks.
Native SDL/browser data, visual and controller parity remain unverified.

Browser launch uses a separate helper ELF sent to the console's local loader.
The SDK browser ABI requires (url, NULL) and UserService initialization first.
The earlier one-argument call stalled startup; the corrected helper returned 0
and the user confirmed the catalogue visible. Read logs through FTP:

- /data/wolodrive-web.log: server startup, port and helper dispatch.
- /data/wolodrive-browser.log: UserService and browser API outcomes.
- /data/wolodrive-shortcut.log: metadata and registration results.
- /data/poldrive-launcher.log: helper transfer and native/fallback launch decisions.
- /data/wolodrive-startup.log: native startup checkpoints and SDL errors.

Link the installer with -lSceIpmi BEFORE -lSceAppInstUtil. Without that dependency
it did not reach main on this console. References:
[SDK dependency issue](https://github.com/ps5-payload-dev/sdk/issues/26) and
[Payload Manager registration](https://github.com/itsPLK/ps5-payload-manager/blob/main/src/app_installer.c).

## Unified-build verification on 2026-10-21

The combined `poldrive.elf` builds with the PS5 SDK. Local launcher tests cover
installer-only first launch, failed registration, existing shortcut launch,
registration-helper dispatch order, native success without browser launch,
native startup failure followed by web launch, helper-transfer failure without
blocking native startup, web failure propagation, exact helper embedding and
the fallback notice on new/reused HTTP services. These tests use controlled
native/browser functions; they do not establish real PS5 startup success.
The existing eight HTTP, two browser-helper and six installer tests also pass
(23 tests including the seven unified-launcher checks). Installer coverage now
includes repairing an already registered uppercase-folder launch target.
Lint, browser tests,
static generation and plain-HTTP preview validation pass. The fallback notice
was observed in the actual browser preview. Existing native full-suite blockers
listed below remain. **AI Studio deployment not verified; the combined ELF has
not been tested on a PS5 device.**

Manual device validation: replace only the saved poldrive.elf, run it through
Payload Manager with the loader active, check the three startup/shortcut logs,
confirm the Media tile and relaunch from it. Check whether native reached its
first frame or the explicit web-fallback notice appeared. Native Files must be
tested separately on disposable console/USB files. For AI Studio, host the
regenerated static-preview at the URL root and repeat Home, sidebar, Search,
category, details, Settings and Files checks. Neither SDK linking nor a fallback
browser is evidence that the native renderer works.

## Historical browser-only verification on 2026-10-20

Lint, web layout tests, regenerated static build and 15 HTTP/helper/installer
tests passed. Tests cover exact embedded bytes, HTTP errors, fallback ports,
singleton reuse, isolated browser dispatch, the two-argument ABI, ownership
checks and both registration paths/errors. The PS5 served HTML/JS/CSS over LAN,
launched its browser, registered the shortcut and reopened the catalogue from
that shortcut, with the latter two screen outcomes confirmed by the user.
These checks do not establish full controller navigation or native visual parity.

At that time the static-preview checker failed at "Missing/duplicated full index:
axekin". The current local checker passes, as noted above; direct AI Studio
deployment remains unverified. Static output paths and workflow are preserved.
Before release, serve
static-preview/ at a plain HTTP root and inspect startup, Home/sidebar, search,
categories, details, Settings and image fallbacks; check the deployed AI Studio
preview too. Compare the same catalogue/language/selection at 1920x1080 against
the native renderer and the actual console before declaring parity.

Native Linux compilation and snapshot tests previously passed. make test remains
blocked by existing regression-test declarations (DESCRIPTION_BUTTON and
load_additional_catalog). That Linux result does not validate PS5 SDL rendering.

The provider handoff and real download/install backend remain outstanding; see
[remote-install-workflow.md](remote-install-workflow.md). The shortcut and HTTP
service must not be advertised as a completed remote installer.

To roll back this additive browser build, remove its new targets, generators,
source and tests; keep the existing make ps5 and npm run build:static workflows.
Deleting PolDrive through the console UI removes its registered shortcut;
retain saved ELF files if it will be installed again.

Branding uses PolDrive in the native/web UI, metadata and the Media tile. The
existing WOLO00001 title identity and internal data/cache paths remain compatible
so this update keeps preferences and does not create a second dashboard app.
