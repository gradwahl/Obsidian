# PS5 fullscreen web-app experiment — v2

**WITHDRAWN FROM DEVICE TESTING:** The reporting console subsequently displayed
repeated insufficient-system-memory notifications. The user clarified that
the console did not crash. The cause is not established. Do not run the
fullscreen test or reopen its Media tile. Use the recovery ELF described below to restore the working
browser-card metadata and remove the owned experimental system scaffold.
Recovery takes priority over collecting logs; do not reproduce the failure for
diagnostics. The recovery ELF has local tests but recovery following this
failure has not been verified on device.

The ordinary `poldrive.elf` installer is confirmed working on the reporting
console, but its `deeplinkUri` opens a PS5 browser card with Sony's title/URL
banner. This experiment tests registration as a web application instead.
**Version 1 failed on the reporting PS5 with CE-105773-3 ("Can't start the
game or app").** Its logs confirmed an exact healthy local service, successful
AppInstUtil initialization and registration scan (`0x0`), followed by a fresh
`INSTALLED` status. The TitleDir entry point was unavailable, so it used the
existing scan fallback. This does not identify the cause of launch failure.

The first test changed only `/user/app` metadata. The upstream SVT Play installer
also creates `/system_ex/app/<title>/eboot.bin` (empty) and a system metadata
copy. **Version 2 adds that missing launch layout.** This is a concrete packaging
difference to test, not a confirmed diagnosis. **The reported v2 test also failed,
with CE-108255-1 (Game or App Error).** Chiaki separately displayed that the
screen could not be shown using Remote Play. Neither screenshot establishes
that the page loaded or fullscreen rendering worked. Fullscreen, successful
launch and plain loopback HTTP support remain unverified.

The initial three supplied local log files were from v1 (21 October 2026,
16:37:11; the shortcut log says `EXPERIMENT:`, not `EXPERIMENT v2:`). New install
and launcher logs at 16:52:27–28 and an `INSTALLED` status confirm service health
and registration, but do not include the shortcut or web-service diagnostics.
The subsequently supplied 16:58:24 v2 shortcut log confirms that `/system_ex`
was already writable (no mount change), the system scaffold was prepared,
AppInstUtil initialized with `0x0`, and the fallback registration scan returned
`0x0` with `INSTALLED`. This confirms the scaffold step completed; registration
success does not establish successful web-app launch. The supplied files still
do not include `/data/wolodrive-web.log` or diagnose the memory warnings.
The user subsequently confirmed that `/data/wolodrive-web.log` is absent.
The service opens that file only on fresh startup, after checking for an
existing server. The installer can also reuse an already healthy service
without starting another process. A missing file therefore does not prove
that the service was absent or the page was never requested; log creation
failure, reuse, or later removal cannot be distinguished from these logs.
Do not reopen the failing app merely to produce this file.
If already available, the web service log can show
whether the host requested HTML, JavaScript or other static resources; absence
of requests alone does not identify the cause, particularly if its bounded
request tracing has already reached its limit. No further launch change is
justified by the screenshots alone.

## Install the test

Build with the configured PS5 Payload SDK:

```bash
make ps5-fullscreen-test
```

1. Keep `poldrive.elf` and the generated `build/poldrive-fullscreen-restore.elf`
   available. The restore ELF is recovery-only; it is not part of normal setup.
   Copy **only**
   `build/poldrive-fullscreen-v2.elf` to the console, for example
   `/data/pldmgr/payloads/poldrive/poldrive-fullscreen-v2.elf`.
2. Close the currently open PolDrive/browser window. Run the test ELF once
   through Payload Manager with the ELF loader active. It contains the same
   quiet service and installer lifecycle; no helper ELF needs copying. It also
   creates its owned system launch scaffold, as described below.
3. Select the existing **PolDrive** icon under **Home / Media**. The installer
   changes that owned tile, without creating another title or opening the UI.
4. Check whether it fills the display without the browser title/URL banner.
   Verify the complete navigation rail, Home/cards, details/back, Search,
   Settings and Files are visible and usable. Check controller input as well
   as the pointer; a different web-app host may report inputs differently.

If it displays an error, a blank page or the original browser card, record the
exact behavior/error code. This would not prove that fullscreen is impossible;
it would mean this specific registration/local-HTTP combination needs more work.

## What changes

The test uses the same `WOLO00001` title and
`http://127.0.0.1:8098/` service, but its metadata has:

```json
{
  "applicationCategoryType": 66048,
  "downloadDataSize": 256,
  "webAppUri": "http://127.0.0.1:8098/"
}
```

`deeplinkUri` is omitted in this variant. These app-mode fields follow the
upstream [SVT Play metadata](https://raw.githubusercontent.com/ps5-payload-dev/svtplay/master/sce_sys/param.json)
and [TV4 Play metadata](https://raw.githubusercontent.com/ps5-payload-dev/tv4play/master/sce_sys/param.json).
The system scaffold follows the [upstream installer source](https://raw.githubusercontent.com/ps5-payload-dev/svtplay/master/main.c).
Unlike upstream, this test does not uninstall the app or add an `ignoreacl`
mount option. It verifies `/system_ex` is the expected exfatfs mount/device,
temporarily makes it writable only if currently read-only, and restores that
read-only state after both successful and failed file operations.

Only `/system_ex/app/WOLO00001/` is written. A new `poldrive-webapp.owner` marker
identifies files created by this test; an existing unowned directory, symlink or
unexpected executable is refused before changing user metadata. The system
`eboot.bin` intentionally has zero bytes, and both metadata copies match. The
mount/file operations require real-device verification even when the SDK links.

Their examples use HTTPS entry URLs, so PolDrive's direct local HTTP URL remains
an explicit part of this experiment. No unsupported fullscreen JS API, new
runtime service, external redirect or renderer rewrite is introduced.

The test has its own helper/service build headers and output files. It does not
overwrite `build/poldrive.elf`. Service readiness, title ownership, configuration
preservation and quiet installation retain the existing single-file checks.
Its shared service/autoload paths are the same as the working installer.

## Logs and rollback

- `/data/poldrive-fullscreen-install.log`: service and startup setup.
- `/data/poldrive-fullscreen-launcher.log`: helper dispatch and confirmation.
- `/data/poldrive-fullscreen-shortcut.log`: experimental metadata and AppInstUtil result.
- `/data/wolodrive-web.log`: shared quiet service and HTTP requests.

To restore the working browser-card launch after **v2**, close PolDrive and run
**`poldrive-fullscreen-restore.elf` through Payload Manager**. It removes only
its owned, expected system scaffold, then restores category 65536 and
`deeplinkUri` on the same user tile. It does not uninstall the app, delete
preferences, remove the quiet service or change autoload. It refuses cleanup
if extra files, unowned objects or links appear in the system title directory.
Select PolDrive under Media afterward. The existing service must still be running.

The original `poldrive.elf` can restore the **v1 metadata-only test**, but it
predates the system scaffold and cannot clean up v2's system files by itself.
Use the new restore ELF for v2. Do not use the native-first or port-8097 rollback
installers. Recovery logs are `/data/poldrive-fullscreen-restore.log` and
`/data/poldrive-fullscreen-restore-launcher.log`.

Local tests validate metadata, fresh/repeated installation, service reuse,
failure paths and restoring the original metadata. They cannot verify the PS5
window presentation. The native SDL renderer, React UI and AI Studio static
hosting contract are unchanged; native/browser visual and input parity in this
new host must be checked on the device. **AI Studio deployment not verified.**

## Earlier v1 local verification — 21 October 2026

The SDK built `poldrive-fullscreen-test.elf` (7,124,560 bytes). Its embedded
service/helper bytes and app-mode JSON were checked. The working installer's
SHA256 remained `cc96d16c816596e2025553609edbe64772c12421b6f16df0693162d7753a74d2`.
The installer suite exercises both variants through real loopback sockets;
shortcut tests cover repeated app-mode registration and byte-for-byte metadata
rollback to the working direct browser tile. Lint, web tests, static generation
and plain-HTTP static checking pass. These local results preceded the failed v1 PS5 launch reported above; no AI
Studio deployment verification was performed. The required device checks are listed above.

## V2 local verification — 21 October 2026

The SDK built both the v2 test and its restore ELF. Exact embedded service/helper
bytes, the system paths and each metadata variant were checked. The original
working installer hash remains unchanged. All 23 installer integration/embedding
tests and nine launcher tests pass. All 19 shortcut tests pass (18 in the suite
run plus the added symlink test run separately), including system ownership,
matching system/user metadata, empty eboot, mount failures, post-write mount
restoration, extra-file refusal, symlink refusal and restore to the original
browser metadata. A separate C mount-scope test covers six injected statfs/nmount
scenarios: read-only, already writable, failed open/close and wrong/missing mount.
These verify requests/call ordering without mounting a real filesystem.

Lint, web tests, static generation and the plain-HTTP preview checker pass.
No React/CSS/native rendering change was made. The subsequent device report
shows v2 launch failure with CE-108255-1; successful launch/fullscreen, real
mount restoration, new-host input and filesystem parity are not verified.
AI Studio deployment not verified. Following the repeated memory warnings
(without a console crash), restore the working setup before further investigation. Use
existing logs only; do not launch the fullscreen test again to obtain them.
Do not infer a specific cause from the error codes or memory notification alone.
