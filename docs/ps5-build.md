# PS5 build and browser service

The native SDL app remains the UI reference. `make ps5` requires the PS5 Payload
SDK and its SDL2/curl/image/font dependencies. The SDK is installed in WSL at
`/opt/ps5-payload-sdk`; set PS5_PAYLOAD_SDK accordingly. A Linux build does not
prove PS5 rendering works. On the tested console the original native startup
failed at SDL_CreateRenderer, after accelerated and software renderer attempts.
Native startup checkpoints are written to /data/wolodrive-startup.log.

## Self-contained HTTP and dashboard shortcut

Run `make ps5-web` (or its `make ps5-shortcut` alias) with the SDK available.
The existing static build workflow is preserved. Distribute only
`build/Poldrive.elf`: it contains the HTTP assets, browser helper and
shortcut setup code. Helper objects/ELFs in build/ are internal intermediates.

Save this one ELF as
/data/pldmgr/payloads/Poldrive/Poldrive.elf and launch it through
Payload Manager. The embedded helper prepares the PolDrive red P icon under
Home / Media automatically. No separate shortcut-installer ELF needs copying.
Subsequent launches skip registration when the recorded shortcut metadata and
icon match. Failed updates invalidate the registration marker and retry later.
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

The server listens on all IPv4 interfaces, preferring port 8097 and selecting a
free port if occupied. /data/wolodrive-web.endpoint records its port and PID.
Relaunch validates the loopback health endpoint against the server protocol and
embedded asset SHA256, then reopens the matching running service. Different or
stale services are not reused. LAN clients open http://<PS5-IP>:<logged-port>/.
Different ports have separate browser localStorage preferences.

The listener serves only embedded GET/HEAD assets plus a health endpoint. It
has no filesystem routes, proxy, downloads, installation or mounting actions.
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

Link the installer with -lSceIpmi BEFORE -lSceAppInstUtil. Without that dependency
it did not reach main on this console. References:
[SDK dependency issue](https://github.com/ps5-payload-dev/sdk/issues/26) and
[Payload Manager registration](https://github.com/itsPLK/ps5-payload-manager/blob/main/src/app_installer.c).

## Verification on 2026-10-20

Lint, web layout tests, regenerated static build and 15 HTTP/helper/installer
tests passed. Tests cover exact embedded bytes, HTTP errors, fallback ports,
singleton reuse, isolated browser dispatch, the two-argument ABI, ownership
checks and both registration paths/errors. The PS5 served HTML/JS/CSS over LAN,
launched its browser, registered the shortcut and reopened the catalogue from
that shortcut, with the latter two screen outcomes confirmed by the user.
These checks do not establish full controller navigation or native visual parity.

The static-preview checker fails at "Missing/duplicated full index: axekin".
This existing catalogue/index issue blocks the full AI Studio gate; independent
asset HTTP checks do not override that failure. AI Studio deployment not verified.
Static output paths and workflow are preserved. Before release, serve
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
