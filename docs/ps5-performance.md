# PS5 performance changes and verification

## Current single-ELF browser memory changes — 21 October 2026

The default payload is now the quiet service plus browser Media shortcut, not
the older native-first installer discussed in the historical sections below.
It still has one distributable, `build/poldrive.elf`; internal helpers stay in
`build/.internal/`. These changes do not re-enable the failed fullscreen host.

- Catalogue schema 2 retains all 85,690 source records in compact browsing
  indexes. Identity, source, platform, DLC/game flags, version, size and dates
  remain available for the existing search, library, filters and ranking.
  Descriptions and per-record artwork/source URLs load only for visible cards,
  the selected hero or open details. Full metadata uses a four-page LRU (384
  records per page) with two requests in flight. Mounted consumers hold only
  their selected full records; summaries are never filled with full metadata.
  Generated index JSON totals 8,165,488 bytes versus 26,193,177 bytes for the
  full detail JSON: 68.8% less source metadata fetched during index startup,
  before visible-item details. This is a transfer/retention design measurement,
  not a measured percentage reduction in PS5 RAM.
- The loader exposes one cumulative collection instead of accumulating a
  second copy in App. React still takes immutable snapshots for updates.
  Home ranking uses stable bounded top-128 insertion instead of sorting three
  complete copies. Search retains only its current nine results plus a total
  count, avoiding an intermediate filtered array, and library
  tile counts no longer allocate a full results array for every console.
- Card artwork is resized to fit 320×480, and selected hero/detail artwork to
  fit 1280×720, preserving aspect ratio and existing source/fallback priority.
  Two resize jobs run at once, with a six-second remote-fetch timeout before
  using the ordinary image fallback. Idle thumbnail resources use an LRU with a
  24 MiB accounting target; live leases are protected until they leave the UI.
  Bitmap handles, temporary canvas buffers and unused blob URLs are released.
  This is not a hard bound on the browser's total image/GPU memory. Remote
  originals still have to be transferred. CORS or unsupported resize APIs can
  require the original-image fallback, whose decoded memory is browser-managed.
- Search retains its existing nine-item pages, library tiles their eight-item
  pages, and library/Home carousels mount a small focus window. Expanded grids
  now mount only visible rows plus overscan with matching spacer geometry.
  Offscreen Home categories no longer retain image components indefinitely.

The generated preview remains ordinary static files at the URL root; no new
runtime service, browser storage requirement, credential or Node dependency is
introduced. The native renderer/data model is unchanged: its existing artwork
cache and on-screen rendering were inspected, and browser labels, ordering,
navigation and artwork source choice are preserved. Resampling can produce
small sharpness differences; native/browser screenshot parity and actual PS5
memory savings are not established by the automated tests.

Local proof covers every generated record and index/detail locator, no full
metadata requests during index startup, selected-record hydration, cache
limits, shared requests/cancellation, thumbnail dimensions/cleanup and a
20,000-item virtual grid with bounded mounted cards. Local plain-HTTP browser
checks cover Home, details, Search and resized artwork. Actual PS5 memory and
launch behavior require a device test. **AI Studio deployment not verified.**

For the next console test, close PolDrive and restart the console/jailbroken
environment before running the updated installer so the previous service is
not still occupying port 8098. The installer refuses a different build at that
port. Use the restored browser-card tile; do not rerun the fullscreen experiment.

The final SDK output is 9,958,992 bytes (SHA256
`ef8adb958cce00254c61c28ba2ddce3e06647009cd4d7ce1e27968910a5992fe`).
It is larger than the previous 7,124,560-byte payload because both browsing
indexes and full details are packaged offline. Smaller retained browser data
does not imply a smaller ELF. The embedded service/helper bytes and ordinary
`deeplinkUri` metadata were checked; only `poldrive.elf` is at the build root.
Lint, web/memory tests, 23 installer tests, nine launcher tests, SDK build and
plain-HTTP preview checking passed. Browser checks included a catalogue-only
Search result, details, Settings, library counts, resized image dimensions and
Tab traversal from grid index 23 to 24 with 30 mounted cards. No fatal console
errors were observed during those checks. The final timeout-only adjustment
was verified locally with the memory tests and a rebuilt static/SDK output.

This branch reduces draw-call overhead, remote-catalogue lookup cost and the
embedded browser bundle. A successful SDK build is **not** a device runtime test.

## Changes

- `src/ui_netflix.c`: rounded rectangles and borders submit bounded groups
  of spans to SDL instead of one call per scanline. The primary red pill uses
  a cached texture, released with the renderer. Catalogue row rebuilds are
  coalesced during an active source sync (400 ms), with final updates immediate.
- `src/catalog_pegasus.c`: a bounded, open-addressed duplicate index replaces
  the per-record O(catalogue-size) scan. The index is rebuilt if seed counts
  change outside the remote-sync worker, and released after the sync stops.
- `src/sourceCatalogues.ts`: large dynamic catalogue modules load two at a
  time with an event-loop yield between batches. This is a peak-pressure
  reduction, not on-demand catalogue loading.
- `scripts/embed_web_preview.py`, `src/web_payload.c`: substantial JS,
  JSON and CSS assets are stored once as deterministic gzip in the ELF.
  Gzip-aware browsers receive those bytes directly; other HTTP clients receive
  streaming inflation with exact uncompressed Content-Length. The server
  caches embedded `/assets/` files for one day, while HTML and API responses
  remain uncached. The zlib SDK include path is explicit in the Makefile.
- `src/main.c`: the PS5 native loop records measured update, render/present
  average and peak times, and FPS, every five seconds to
  `/data/wolodrive-performance.log`. This file is created only after the
  native renderer and UI are initialized.

The built unified ELF was 48,298,216 bytes before compression and 25,884,992
bytes after the browser payload change on the local PS5 SDK. File size
reduction is not evidence of equivalent runtime frame-time improvement.

## Remaining issues that require device investigation

**Direct memory allocation:** earlier device logs reported
`sceKernelAllocateMainDirectMemory: Resource temporarily unavailable`.
This operation is implemented in the PS5 SDK SDL video backend, not by
PolDrive's C renderer. Shrinking the embedded payload may reduce general memory
pressure but does **not** guarantee a successful SDL/VideoOut allocation.
The SDK has to be checked for framebuffer allocation size, lifetime, and
the actual failing service on the console before changing its allocation.
Do not blindly retry the same allocation or change VideoOut buffer sizes.

**Catalogue capacity:** the in-memory native `GameItem` model retains a
26,000-element fixed array and multiple fixed-size strings. A safe arena or
sparse-metadata migration will require updating native callers and tests.
This branch avoids shrinking strings or catalogue capacity, which would
silently truncate data.

**Renderer:** the PS5 implementation still uses a software SDL backend and
1920x1080 logical coordinates. Batched drawing reduces call overhead but
does not add GPU acceleration or guarantee a 60 Hz display.

## Device verification

1. Build with `make ps5` in the configured PS5 SDK environment and install
   only `build/poldrive.elf`. Keep a backup of the previous known-working ELF.
2. Launch through Payload Manager, including an existing registered shortcut
   if applicable. Retrieve `/data/poldrive-launcher.log` and
   `/data/wolodrive-startup.log` to confirm native startup versus browser fallback.
3. If native displays, navigate Home, carousel and Files, then retrieve
   `/data/wolodrive-performance.log`. Compare stationary and navigation
   FPS, render averages and render peaks across multiple repetitions.
4. If native falls back, inspect the exact SDL renderer error first. A web
   fallback alone is not confirmation that VideoOut has been fixed.

Local checks: `npm run lint`, `npm run test:web`,
`make test-ps5-renderer`, `make test-native-snapshot`,
`python3 tests/test_web_payload.py`, `python3 tests/test_unified_launcher.py`,
and `make ps5` in the configured SDK/WSL environment. The desktop
`make benchmark-ui` test can be used for relative software-renderer comparisons
but is not a PS5 frame-rate measurement.

## PS5 browser shows a white "OK" page (2026-10-21 device log)

A device log from the previous deployed build showed server ID
`wolodrive-server-v9-files-lan`, four `index.html` responses, followed by
hundreds of thousands of `accept: error_code=0x000000a3` messages. The
browser helper reported successful dispatch, but that does **not** establish
that JavaScript or CSS finished loading. That errno value has not been mapped
to a confirmed console-side root cause.

The current diagnostic build identifies itself as
`wolodrive-server-v11-files-lan-open`. It now records up to 96 requests for
static bundled assets (including MIME, size, and selected gzip encoding).
Repeated errors in `accept()` are rate-limited and terminate the listener
after eight consecutive failures rather than saturating a log and CPU. A
simple HTML loading placeholder is displayed until React replaces the root;
it can distinguish a JavaScript bootstrap failure from a completely different
page. These safeguards do **not** prove the console's networking fault is fixed.

When retrying, use the newly rebuilt `build/poldrive.elf`, not an earlier
copy. Confirm v11 in `/data/wolodrive-web.log` and inspect the first few
`[POLDRIVE WEB] Static GET` lines. Requests for `/index.html` but no
`/assets/index-*.js` suggest browser loading or HTML parsing problems.
Successful JS/CSS requests with an unmounted UI suggest browser compatibility
or JavaScript runtime errors. Compare to the LAN URL printed in the log by
opening `http://<PS5-LAN-IP>:<PORT>/` on a **trusted** PC or phone. If the
LAN browser works but the console does not, isolate the console browser/
process networking environment rather than altering the catalogue.
Do not share the LAN file bridge on an untrusted network.

Also retrieve `/data/wolodrive-startup.log` on the same launch to establish
the **native** startup reason. `/data/poldrive-launcher.log` can be
overwritten by a separate installer-only invocation; do not infer the native
reason from a log generated on a different launch.

## Media shortcut displays only "OK": launcher/installer status mismatch

The Media shortcut's configured `deeplinkUri` calls Payload Manager's
`http://127.0.0.1:8084/loadpayload:...` endpoint. The console's plain
white `OK` page is consistent with Payload Manager acknowledging that HTTP
request; it is **not** evidence that the native app initialized.

In the reported session `poldrive-launcher.log` said the shortcut registration
was unconfirmed while the helper's `wolodrive-shortcut.log` said
`Shortcut already current; no registration required`. The launcher checked
`/user/app/WOLO00001/sce_sys/wolodrive-shortcut.registered`, a path which
may not appear identically to two different payload processes.

The helper now writes an atomic `/data/wolodrive-shortcut.status` response
with either `CURRENT` (pre-existing verified shortcut) or `INSTALLED`
(successful first registration). The launcher clears the previous reply before
sending a helper and will only accept a fresh response; it starts native if
the helper says `CURRENT`, even if the marker is invisible to the launcher.
`INSTALLED` retains the one-time installer-only exit. Missing/failed
confirmation does not bypass registration checks.

To test, replace the Payload Manager copy at
`/data/pldmgr/payloads/poldrive/poldrive.elf` with the newly built
`build/poldrive.elf`, reboot to clear old payload/browser processes, and
launch from Media. On the new attempt collect the logs
`/data/poldrive-launcher.log`, `/data/wolodrive-shortcut.log`,
`/data/wolodrive-startup.log`, and, if fallback is used,
`/data/wolodrive-web.log`. Specifically look for
`Shortcut helper confirmed CURRENT via /data; starting native UI.`.
If the native startup then fails, investigate the SDL error on the console
rather than treating the Payload Manager `OK` page as a successful UI launch.

## Faster Media launch and early loading splash

For already registered shortcuts, the unified launcher no longer sends the
installer helper payload to port 9021 on **every** launch. It accepts the
previous successful `CURRENT` response in
`/data/wolodrive-shortcut.status` as a cached verification (or the
`/user/app` marker if visible). `INSTALLED` is not accepted as a cached
fast-path: a first-time registration still exits as installer-only, and an
unknown status still dispatches the helper. To force revalidation after
manually altering the shortcut metadata, remove
`/data/wolodrive-shortcut.status` (and the installer will check it again).
This avoids the helper transfer and its confirmation wait during normal
subsequent Media launches. It cannot eliminate the PS5 dashboard's Payload
Manager loading time or the native SDL/VideoOut initialization time.

The native executable now creates SDL video and its renderer before
initializing curl, images, fonts and controllers; after the renderer starts,
it immediately renders a minimalist **POLDRIVE** startup screen with a red
12-segment spinner and progress bar. Progress frames are rendered between
initialization stages, on the main SDL thread. A blocking SDK call can still
hold the display on the last frame; no unsupported background SDL rendering
thread has been added. The HTML fallback also displays a CSS-animated red
spinner until React mounts.

Startup logs report elapsed milliseconds relative to entering native main
for each stage, in `/data/wolodrive-startup.log`. Use this to distinguish
time spent before SDL, on VideoOut creation, and in catalogue/UI
initialization. The spinner cannot appear during the browser-based Payload
Manager `/loadpayload:` request **before** the ELF starts or while SDL
VideoOut itself is still initializing.

Tests: `make test-startup-splash` verifies a no-font/no-assets startup
frame on a dummy SDL software renderer; unified-launcher tests verify no
helper dispatch for already-verified shortcuts, including `CURRENT` cached
responses.

## PS5 browser watchdog / stuck loading screen (October 2026)

Symptoms observed on console: browser loader renders but stops animating, then
the browser reports "This page isn't responding" even though the app
eventually appears. The previous source loader evaluated multiple multi-MB
JSON-as-JavaScript modules on the browser's main thread and converted
~85,000 records before publishing one UI update. The native SDL loading
screen is separate: the screenshot shows the *browser* fallback.

The HTML bootstrap now contains only the PolDrive name, a vendor-prefixed
CSS spinner (compositor transform) and "Starting PolDrive...". The
diagnostic "If this message stays..." paragraph has been removed.

Browser startup now has these changes:

- `scripts/generate_browser_catalog_pages.cjs` converts verified source
  files into 384-item JSON pages at **build time**. The optional 6MB
  artwork-match map is joined by platform/title at build time, so no giant
  cover-index JS module is evaluated in the browser.
- `src/sourceCatalogues.ts` asynchronously fetches one small page at a
  time. It converts 96 entries per task then yields using a real timer,
  giving CSS animations, controller input and the browser watchdog a chance
  to run. The pages are read only after the seed Home UI has appeared.
- `src/App.tsx` publishes incremental catalogue batches with at most one
  expensive React catalogue commit per ~900ms, then a final update.
- `src/catalog.ts` computes all five Home categories from one normalized
  deduplication pass instead of five repeated full scans.
- `src/components/HomeView.tsx` keeps only ~15-21 card components per
  category mounted near the focused cover. Empty spacer elements preserve
  scrolling distances, selection and the full dataset.
- `src/web_payload.c` caches content-addressed JSON pages, but never caches
  the manifest. Original source catalogues remain unchanged and all
  catalogue entries remain available over time. The app does not require a
  network call to a remote catalogue on boot: these pages ship inside
  the ELF and are requested over local loopback.

The build runs the page generator automatically before `npm run dev`,
`npm run build` and `npm run build:static`. `public/poldrive-pages/`
is generated and ignored by Git; do not copy that directory manually.
Native snapshot generation still consumes the original source indexes.
The web tests verify source counts, source precedence, archive artwork
matches, bounded page size, partial fetch/yield correctness, retained
Home ranking and DOM virtualization.

This substantially reduces startup event-loop blocking but cannot
*guarantee* the PS5 browser's watchdog never fires on all firmware builds.
On-console validation is required. If the alert persists, compare the
updated `/data/wolodrive-web.log` and startup log, and see whether the
problem occurs before or after the React Home view appears. Slower CSS
animations can still freeze if WebKit is blocked during the initial main
JS parse, or while another system task monopolizes the browser process.
