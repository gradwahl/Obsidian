# PS5 single-file installer and native recovery

For renderer, cataloguing and embedded-asset performance changes, see
[PS5 performance and device verification](ps5-performance.md).

`make ps5` now creates one installer with a bundled quiet service and direct
Media tile. The earlier manual two-file mode is documented separately in
[PS5 Web-first Media launch](ps5-webfirst.md).

The native SDL app remains the UI reference. `make ps5-native` requires the PS5 Payload
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

For native renderer investigation, use `make ps5-native` and run its
`build/obsidian-native-installer.elf` through Payload Manager once, then use the
restored native-first Media tile. This recovery route retains the intermediate
loader response. Retrieve `/data/wolodrive-startup.log` and
`/data/obsidian-launcher.log` for native startup errors. PS5 display, storage
access and parity remain unverified until device testing.

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

## One distributable: obsidian.elf

For the separate, opt-in fullscreen launch experiment, see
[PS5 fullscreen web-app test](ps5-fullscreen-test.md). The normal installer and
its confirmed browser-card launch remain the default.

Run `make ps5` (or `make ps5-shortcut`) with the SDK available. Distribute only
`build/obsidian.elf`. Embedded service/helper ELFs are generated in
`build/.internal/`; users do not copy or run them separately. Other top-level
payload variants are produced only by their explicitly requested targets.

1. Copy it to `/data/pldmgr/payloads/obsidian/obsidian.elf` and run it through
   Payload Manager with the ELF loader active.
2. The installer saves its bundled quiet service at
   `/data/pldmgr/payloads/obsidian/obsidian-web-service.elf`, starts it through
   the loopback loader, and checks its exact health identity/build checksum.
3. It preserves the existing autoload sequence and unrelated configuration,
   appends the service path once, enables `AUTOLOAD_ENABLED`, and registers the
   owned `WOLO00001` Media tile at `http://127.0.0.1:8098/`.
4. The installer exits without opening native UI or a browser. Open Obsidian
   from **Home / Media**. The tile goes directly to the existing React UI;
   it never visits `/loadpayload:` and needs no second browser window.

The service listens on all IPv4 interfaces, fixed to port 8098 with no browser
opener. On another device on the same LAN, open `http://<PS5-IP>:8098/`
(for example, `http://192.168.0.137:8098/`). The Media tile continues to use
`http://127.0.0.1:8098/` on the console. The
older 8097 server is left untouched. An unrelated listener or different build
on 8098 stops installation before configuration/dashboard changes; it is never
terminated automatically. Stop that service or restart the jailbroken
environment and rerun the installer. Matching services are reused quietly.
The installer checks title ownership before saving services or startup changes,
and explicit reinstallations retry Media registration even if a marker exists.

After each jailbreak, **Payload Manager must start** so its enabled autoload
sequence starts the service. No payload can run on an unjailbroken PS5 boot.
The installer preserves Payload Manager's own browser preference, delay,
favorites and other settings; its own UI never opens during installation or
autoload. Enabling autoload also enables the existing entries. Startup-file
backups are saved once as `/data/pldmgr/autoload.txt.obsidian-backup` and
`/data/pldmgr/pldmgr_config.txt.obsidian-backup`. Text, entry order and `!N`
millisecond delays are preserved; duplicate active service entries are not
added. Oversized/binary configuration or malformed delay entries cause failure
without changing the startup files or dashboard. Close configuration editors
while installing; these file edits do not participate in Payload Manager's
in-process configuration lock.

The generated browser UI/assets are unchanged. The quiet service uses the same
native file bridge with its existing same-origin/session checks. LAN clients
can access Files as well as the catalogue; use this service on a trusted LAN.
The v2 health identity prevents reusing the old loopback-only service even
when its embedded preview is identical. Before installing this update, stop
the old service or restart/re-jailbreak without autoloading the old service,
then run the new installer. Merely closing the Media tile does not stop the
background service. Keep the previous installer for rollback; stop the new
service before reinstalling the previous build. The original native renderer and its data remain the reference;
this packaging change does not claim renderer/controller parity or fix VideoOut.

### Logs and rollback

- `/data/obsidian-install.log`: service persistence, loader, health and startup configuration.
- `/data/obsidian-launcher.log`: fresh shortcut-helper dispatch/result.
- `/data/wolodrive-shortcut.log`: Media metadata and AppInstUtil result.
- `/data/wolodrive-web.log`: quiet service and filesystem-bridge startup.
- `/data/obsidian-service.endpoint`: current direct service endpoint and PID.

To undo startup configuration, remove only the service path from autoload and
restore the prior enabled value. The `.obsidian-backup` files record the original
contents; restore whole backups only if later unrelated edits can be discarded.
Use `make ps5-native` to build a separate native-first recovery installer; run it
once to restore its owned Media link and installed native app. This keeps the
historical `OK`/web-fallback handoff. Remove the quiet service's autoload entry
and stop it through Payload Manager if no longer needed. Never autoload the
installer itself; the installer adds only the quiet-service path.

`make ps5-web` remains browser-only diagnostics; the older manual
`make ps5-webfirst` flow uses port 8097 and is not required by this installer.
The AI Studio `npm run build:static` contract and HTTP-root output are preserved.

## Current single-file verification — 2026-10-21

The user confirmed the single-ELF setup and direct dashboard launch working on
their PS5 on this date. This confirms the reported installation/launch flow on
that console; fresh-jailbreak autoload, full controller/visual parity and other
firmware/loader combinations have not been separately verified.

The PS5 SDK built `build/obsidian.elf` (7,124,560 bytes). Its exact embedded
quiet-service and shortcut-helper ELF bytes, direct 8098 URL, and absence of
native/browser-opener payloads were checked. The output includes the current
autoload validation. The service uses the same generated static preview; no
React/CSS/rendering changes were made by this installation work.

Local C integration tests exercise real loopback loader/health sockets and
actual installer/shortcut/configuration code. They cover fresh installation,
matching-service reuse, backups and entry/delay preservation, first-time
configuration, write failures, invalid settings/delays, unrelated or stale
service rejection, title ownership and failed registration. PS5 app-service
registration and ELF execution are simulated in these tests. Separate HTTP
tests execute the quiet C service itself, verifying UI delivery, no browser
launch, singleton reuse and fixed-port validation. These do not prove PS5
lifecycle behavior.

`npm run lint`, `npm run test:web`, `npm run build:static` (through `make ps5`),
and `python tests/check_static_preview.py` pass. The static checker now verifies
the existing paged-catalog contract instead of obsolete per-source JS chunks:
85,690 records, 236 content-hashed JSON pages, root JS/CSS and 16 versioned
artwork files over plain HTTP without Vite/Node. Browser smoke checks of the
regenerated HTTP preview covered startup/Home, details/back, Search filtering,
company/console navigation, Settings, catalog images/fallbacks and keyboard/
mouse interaction; no captured console errors. No new renderer changes were
made; sliding-rail motion and full native/browser/controller visual parity
were not compared on device. **AI Studio deployment not verified.**

Manual PS5 validation: copy only the new installer and run it once with the
loader active. Confirm no Obsidian UI opens, and inspect the installation,
shortcut and service logs. Select Media/Obsidian and confirm a single direct
open without `OK`, closing or reopening. Close/reopen the tile, rerun the
installer, and verify both remain quiet until the tile is selected. After a
fresh jailbreak, start Payload Manager and confirm its autoload starts only
the quiet service for Obsidian; verify the tile works without reinstalling.
Existing Payload Manager entries/preferences must still match their backups.
For AI Studio, serve the refreshed `static-preview/` at the URL root and repeat
startup, Home/sidebar, search, categories, dialogs, artwork/fallbacks, keyboard/
mouse and Settings checks on the deployed host before claiming deployment.

## Earlier unified-build verification on 2026-10-21

These results describe the previous launcher lifecycle, before the installer/app
entry points were separated. See the installation fix verification below.

The combined `obsidian.elf` builds with the PS5 SDK. Local launcher tests cover
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

Manual device validation: replace only the saved obsidian.elf, run it through
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
Deleting Obsidian through the console UI removes its registered shortcut;
retain saved ELF files if it will be installed again.

Branding uses Obsidian in the native/web UI, metadata and the Media tile. The
existing WOLO00001 title identity and internal data/cache paths remain compatible
so this update keeps preferences and does not create a second dashboard app.

## Historical installer/app split verification — 2026-10-21

The current PS5 SDK build succeeded. The generated installer contains the exact
helper ELF, that helper contains the exact app ELF, and its dashboard deeplink
targets `/data/pldmgr/payloads/obsidian/obsidian-app.elf`. Nine launcher tests cover installer-only
invocations with existing/cached registration, stale-status cancellation,
registration failure and the independent dashboard native/web entry point.
Eleven shortcut tests cover runtime persistence, repeated registration repair,
write/registration failures, ownership and metadata migration. File bridge
(7), HTTP payload (11), and browser helper (2) tests pass. Lint and web tests
pass. The first web-test run overlapped catalog generation and failed; a
sequential repeat passed. No renderer or browser source was changed by this fix.

`npm run build:static` completed and all 257 emitted files were fetched and
byte-compared over a plain Python HTTP server. Browser checks confirmed Home,
Search, Settings and loaded catalogs, with no captured console errors.
`python tests/check_static_preview.py` still fails at
`Missing/duplicated full index: axekin`: the pre-existing paged-JSON catalog
changes no longer emit the JS chunks expected by that checker. The AI Studio
gate therefore did not pass. **AI Studio deployment not verified.** Full browser
interaction and native/browser visual comparison were not repeated for this
installer-only fix; PS5 registration and controller behavior remain unverified.

Manual console check: replace the saved installer ELF, run it in Payload Manager,
confirm it returns without opening Obsidian, and check the Media tile. Run the
installer again to verify repair also stays closed. Select the Media tile and
confirm it opens the app. If the icon is absent, inspect
`/data/wolodrive-shortcut.log` for the registration result and
`/data/obsidian-launcher.log` for fresh helper confirmation. On AI Studio, host
`static-preview/` at the URL root and check startup, Home, sidebar, Search,
category navigation, dialogs, artwork/fallbacks, keyboard/mouse and Settings.

## Historical follow-up: Invalid payload name — 2026-10-21

The first installer/app separation used `/data/obsidian-app.elf`. Payload
Manager's `slot_validate_payload_path` permits its `/data/pldmgr` root (and
configured USB roots), so that location is rejected by `/loadpayload:` with
`Invalid payload name`. The installer now saves the runtime at
`/data/pldmgr/payloads/obsidian/obsidian-app.elf`, creates the parent directories
if needed, and updates the dashboard deeplink to the same path. This keeps the
installer and runtime separate without placing the app outside managed storage.
The runtime may now also appear in Payload Manager's list; select `obsidian.elf`
to install/repair, and use the dashboard tile to open the app.

The PS5 SDK installer and legacy-shortcut builds succeeded; exact embedded
app/helper bytes and the corrected deeplink were verified in the output.
Twelve shortcut tests and nine launcher tests pass, including an explicit
managed-storage target regression check. Browser/runtime/static sources are
unchanged by this follow-up; the previous local static-preview results and
checker blocker still apply. AI Studio deployment not verified. Real console
validation is still required: replace the installer, run it once, confirm it
stays closed, then open the Media tile and confirm the invalid-name error is gone.

Upstream contract:
https://github.com/itsPLK/ps5-payload-manager/blob/main/src/slot.c
(`slot_resolve`, `slot_validate_payload_path`) and
https://github.com/itsPLK/ps5-payload-manager/blob/main/include/pldmgr.h
(`BASE_DATA_DIR`).


### LAN service update verification (2026-10-21)

The default installer, fullscreen test service, and recovery web-first service
now listen on all IPv4 interfaces. The console's direct Media URL is unchanged.
The service health identity advances to v2 so the installer rejects a running
v1 loopback-only service even with identical preview assets.

Validated: PS5 SDK `make ps5`; Linux `make build-linux` and
`make test-native-snapshot`; HTTP service tests (13), installer tests (23 plus
the new old-service rejection case), and filesystem bridge tests (7);
`npm run lint`, `npm run test:web`, `npm run build:static`, and
`python tests/check_static_preview.py`. The first web-test attempt overlapped
catalogue regeneration; the subsequent run with stable generated files passed.
The rebuilt preview loaded via Python's plain HTTP server: Home, search,
details open/close, Settings and library navigation worked with no browser
console errors observed. No UI geometry, catalog semantics, or native actions
were changed by the LAN update; side-by-side visual parity was not reverified.

The broader `make test` remains blocked by existing missing artwork/catalog
helper declarations in `tests/test_regressions.c`, including
`library_alpha_bounds` (invalid initializer). Actual updated PS5 runtime/LAN
access is unverified until this ELF is installed. AI Studio deployment not
verified. For manual validation, install with the old background service
stopped, open the Media tile, then open `http://<PS5-IP>:8098/` from a second
LAN device and compare Home, navigation, search, details and Files. In AI
Studio, serve the regenerated `static-preview/` at the URL root and check
startup, sidebar, search, categories, dialogs, artwork and Settings.
