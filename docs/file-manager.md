# Files manager

Files now browses real storage and provides Copy, Move, Paste, Delete and Unzip.
Select an entry, choose Copy or Move, navigate to a destination, then choose
Paste. Move copies the contents first and removes the source only after a
successful copy. Delete is permanent and opens a confirmation with Cancel
selected. Single click selects; double click or Enter/Cross opens a directory.
Arrow keys/D-pad navigate. Escape/Circle goes to the parent, then the rail at
the storage root. R/Triangle refreshes the directory.

Native opens `/data` on PS5, or the current working directory on Linux. Parent
folder reaches `/`, allowing navigation to mounted USB volumes under `/mnt`.
Actual availability depends on the payload's filesystem permissions. Native
operations run on an SDL worker thread and block further Files operations until
they finish. The UI remains responsive; there is no cancellation or byte-level
progress in this version.

The web app uses a user-granted local directory through the optional
[File System Access directory picker](https://developer.mozilla.org/en-US/docs/Web/API/Window/showDirectoryPicker).
Open folder can choose another destination without clearing the clipboard.
Browser permission prompts require direct user interaction. The picker requires
a supporting browser and secure context (HTTPS or localhost), and embedded
hosts may prohibit it. Unsupported browsers show an explicit unavailable state.
Permissioned browser folders stay on that browser's device. Ordinary static hosting has no filesystem HTTP endpoint,
Node runtime, localhost bridge, or credential requirement.

The PS5 **web fallback** includes an optional filesystem bridge with console-local
access and paired LAN access.
Open Files, then Open folder to list `/data`; Parent folder reaches `/`, including
accessible USB mounts under `/mnt`. Its Copy, Move, Delete and Unzip actions call
the same `fm_*` backend as the native UI. The browser does not need a directory
picker for console storage. Operations are serialized in the payload HTTP loop;
the browser remains responsive with file controls disabled until the response.
Other HTTP requests wait during a long operation. There is no cancellation or
progress beyond the existing busy state. Closing a page does not cancel an
operation already executing in the payload.

This bridge is compiled into PS5 ELF targets only (`WOLO_FILE_BRIDGE`). It accepts
IPv4 loopback/private/link-local peers, an exact numeric destination-IP-and-port
Host, matching Origin when present, a custom request marker and a random
per-process session token for listings/operations. Cross-origin fetch metadata
is rejected and no CORS permission is emitted. The console-local Files page
displays a separate 16-hex-digit LAN access code. Unpaired LAN clients get no
listing, token or code; entering that code exchanges it for an in-memory session.
Eight failed pairing attempts block pairing for one minute without blocking local
console access. Restarting the payload's server changes code/token and revokes
sessions; ordinary relaunches that reuse the same server preserve its session.
Token/code creation uses the console random device; failure disables the bridge
and is logged. Credentials are not bundled into assets or written to URLs/logs.
This is ordinary HTTP on a trusted LAN, not an encrypted remote-access service.
Backend permissions, symlink refusal and copy/move overwrite protection are shared
with native Files. Pairing grants read/write file access within those permissions.
Path references are not filesystem transactions; replacing a file concurrently
can still change what a later operation addresses, as in native Files.

Local bridge discovery uses the launcher's `#ps5-files` or `#native-unavailable`
URL marker. LAN browsers visiting the root URL probe root HEAD for the payload's
`X-PolDrive-Files: 2` response header before attempting pairing. A normal static
host has no such header; AI Studio needs no bridge and retains its permissioned
picker/unavailable state. Use the PS5's numeric IPv4 address and actual server port.
Existing servers without the current protocol are not reused by the new ELF.

## Editing PS5 files from a PC

1. Launch the updated ELF through Payload Manager. In Files on the PS5, read the
   **LAN access code** at the bottom of the screen.
2. On the PC, visit `http://<PS5-IP>:<port>/`, open Files, enter that code and
   choose Connect to PS5. The actual port is recorded in `/data/wolodrive-web.log`
   and `/data/wolodrive-web.endpoint` (8097 preferred). Pair again after a page reload.
3. Open folder starts at `/data`; Parent folder reaches `/` and accessible system
   directories/mounts. Select a regular file, choose Edit text and Save to PS5.
   Files on read-only mounts or blocked by payload permissions report errors.
4. Download file saves the selected file to the PC. Upload new file creates a new
   filename in the current console folder and refuses an existing destination.
   Replace file uploads over the selected console file only after a confirmation
   with Cancel selected. Binary files use Download/Replace rather than the editor.

Text editing is limited to 1 MiB of UTF-8 text; file transfers are limited to 8 MiB.
The text editor preserves UTF-8 BOM and consistent LF/CRLF line endings. Editing
a file with mixed line endings normalizes them to LF. A revision combines file
identity, size/times and content CRC; a changed file causes HTTP 409 instead of a
stale overwrite. Unsaved text stays visible on error. Existing saves stage a
temporary file in the destination directory, preserve mode/owner, fsync and rename
after a second revision check. This changes inode identity and does not preserve
extended metadata or hardlink relationships. Concurrent changes remain subject to
the native backend's path-race limits; this is not a cross-application transaction.
New uploads use exclusive creation and remove an incomplete file on write failure.
The console Files controls retain the native layout; pairing and PC editing/
transfer controls are the user-requested desktop workflow, not an SDL text editor.

The two interfaces use the same toolbar order, list geometry, eight-row pages,
selection, delete dialog, input meanings and translation keys in nine languages.
Storage-selection mechanisms, path prefixes and operating-system error details
necessarily differ. Browser permissions and native permissions are independent.
Linux SDL screenshot review does not establish PS5 rendering or device behavior.

## Operation limits

- Existing destinations are refused; copies do not merge folders. Copying into
  the same location or into a descendant is refused. Native links and special
  files are skipped in listings and refused during recursive copy/delete.
- Copies preserve file contents and directory hierarchy, not permissions,
  ownership, timestamps, extended attributes or sparse allocation. Moves use
  exclusive copy creation even on the same native volume to avoid portable
  `rename()` silently replacing a destination.
- Operations are not transactions across other applications. Avoid concurrent
  edits to source/destination folders. Browser handle creation has no portable
  exclusive-create option. Removal failures may leave a source partly removed;
  the completed destination is retained and an error is reported. Failed-copy
  or failed-extraction cleanup is best effort. Review both locations after an
  error. There is no automatic overwrite, undo or recycle bin.
- ZIP store and deflate are supported. Input is limited to 128 MiB, declared and
  actual output to 512 MiB, entries to 4,096, nesting to 64 levels. Listings are
  limited to 4,096 entries; oversized listings fail explicitly rather than
  silently hiding entries. Files over 128 MiB can still be copied/moved.
- Unzip creates `<archive filename>.extracted`, refuses an existing output,
  checks CRC/output sizes and rejects unsafe paths, symlinks, encryption,
  split archives, ZIP64 and unsupported compression. Names must be UTF-8 or
  ASCII. Limits are intentionally conservative because compressed input is held
  in memory; browser decoded output is also buffered before writing.

## Verification and deployment

Run `npm run lint`, `npm run test:web`, `npm run build:static`, and
`python tests/check_static_preview.py`. The static hosting contract is unchanged:
root `static-preview/index.html`, root-relative assets and ordinary HTTP serving.
`fflate` is bundled into browser JavaScript; native ZIP decoding uses zlib (`-lz`).
Rollback removes the Files integration, new backend/UI files, translation keys,
fflate dependency/lock entry, and Makefile source/zlib entries, then rebuilds the
preview. No user data schema or storage migration is involved.

Native checks: `make build-linux`, `make test-native-snapshot`, `make test-files`.
`make test-files` exercises real temporary directories, corrupt/traversing ZIPs,
copy/move conflicts, symlink refusal and the real SDL input handler. It also
writes `build/files-native-{empty,list,delete}.png` for visual comparison.
`npm run test:web` includes browser service tests with handle doubles and SSR
layout/translation contracts.

For a real browser filesystem integration test, run
`node tests/build_file_fixture.cjs`, then serve `build/files-fixture/` using plain
HTTP. The fixture's Prepare button creates only a disposable, origin-private
test folder; it never accesses user-selected files. Its FileBrowser is the
production component with a fixture-only picker returning real OPFS handles.
Verify copy/paste, move/source removal, conflict errors, ZIP extraction,
delete/cancel and directory navigation. Do not package the fixture in the
production preview.

Checked locally: lint/web tests, static build and HTTP resource checker; Linux
build, native snapshot and Files backend/input tests; actual browser OPFS
copy/paste, move, ZIP extraction, refused overwrite and delete/default cancel.
Reviewed SDL and browser list/modal composition at equivalent 16:9 geometry;
font rasterization differs between SDL and browser engines. Browser smoke checks
covered startup, Home/details, Search filtering, company navigation, Settings and
Files. Missing optional local cover-cache URLs return 404 and use existing
fallbacks; they do not stop startup.

The existing full `make test` is blocked compiling `tests/test_regressions.c`:
it references `DESCRIPTION_BUTTON`, `load_library_artwork`,
`library_alpha_bounds`, `prepare_library_logo`, and `load_additional_catalog`,
which were already removed by other working-tree changes. These unrelated tests
were not rewritten as part of Files. The full native test gate has not passed.

The unified `make ps5` now produces `build/poldrive.elf`: it dispatches the Media
shortcut helper, starts the native UI, and falls back to the embedded browser on
a reported native startup failure. See [ps5-build.md](ps5-build.md). Both interfaces
now call the native file backend, with LAN access requiring pairing.
Standalone static previews retain their explicit unavailable state
when the browser does not support permissioned folder access.

**PS5 SDK build verified; device verification not performed. AI Studio deployment not verified.**
On a PS5 with SDK/device access, compile `make ps5` and repeat the temporary-file
workflow on `/data` and USB, including permission-denied/full-volume failures,
controller navigation and leaving/re-entering Files. In AI Studio, host the
rebuilt `static-preview/` at the URL root, check Home/search/categories/dialogs,
Settings and Files, then confirm either permissioned folder access or the explicit
unavailable state. These device/host gates remain required before release.

## Console bridge verification (2026-10-21)

The updated `build/poldrive.elf` includes the bridge and a freshly embedded static
preview. Local checks passed: lint, web/bridge client tests, `build:static`, the
plain-HTTP static checker, Linux build, native catalog snapshot, native Files
backend/input tests, and `make test-web-payload` (including real HTTP bridge tests,
embedded assets, browser helper, shortcut and unified-launcher checks).
The existing full native regression-suite blocker described above remains.

Browser integration used `node tests/build_bridge_fixture.cjs`, then
`python3 tests/run_bridge_fixture.py` in WSL. This runs the production FileBrowser
against the actual C HTTP bridge with a root confined to
`build/bridge-fixture/storage`; it never maps to real console/user storage.
At `http://127.0.0.1:3077/#ps5-files`, verified listing `/data`, navigating to `/`,
copy/paste, overwrite refusal, move, delete/default Cancel and ZIP extraction.
The copied bytes, removed move source and extracted contents were also checked
by native HTTP tests. Reviewed the equivalent four-entry 16:9 Files composition
against the SDL screenshot; font rendering and fixture root names differ.
This is not proof of actual PS5 rendering or controller event delivery.

Standalone preview browser checks over plain HTTP covered Home/details, search
filtering, category navigation, Settings and Files, with no captured console
errors. The HTTP checker confirmed the root entry, JS/CSS and 16 artwork assets.
Existing optional cover-cache misses still use the existing artwork fallbacks.
**AI Studio deployment not verified. PS5 device filesystem access not verified.**

On PS5, replace the saved ELF, launch it once through Payload Manager, then use
Files > Open folder. Expect `/data`; Parent folder should expose `/` and permitted
mounts. Use a new disposable test directory for copy/move/unzip/delete checks,
including Cancel, keyboard/controller navigation and leaving/re-entering Files.
For a bridge error, retrieve `/data/wolodrive-web.log`; startup should include
`Filesystem bridge ready; LAN pairing enabled; initial path /data`. Browser errors must
report genuine payload permissions/failures rather than successful fake actions.
In AI Studio, host the generated directory at the URL root and check the same core
screens plus the permissioned picker/unavailable state; no bridge is required.

Bridge rollback: remove `WOLO_FILE_BRIDGE` from PS5 compilation, remove console
bridge discovery/transport from the React Files service, restore the prior notice,
and rebuild both static preview and ELF. No user data migration, SDK replacement,
external credential, new dependency or output-path change is involved.

## Paired LAN editing verification (2026-10-21)

The updated ELF builds with the PS5 SDK. Linux build/native snapshot/Files input
checks, nine native backend tests, seven bridge HTTP tests, embedded-server and
launcher/helper tests, lint, web client/parity tests, static build and the plain
HTTP resource checker passed. The previously documented full native regression
suite blocker remains; these focused checks do not establish a full-suite pass.

Browser integration used the fixture through its real private IPv4 interface,
not loopback auto-authorization. Verified: unpaired denial, code entry, listing,
UTF-8 text edits, preserved BOM/CRLF bytes, a stale save retaining unsaved text
without overwriting the newer file, byte-exact PC upload/download, confirmed
binary replacement with Cancel initially focused, and refusal to edit binary
files as text. All test storage stayed under `build/bridge-fixture/storage`.
Reviewed the editor/conflict message and the final transfer-control placement
inside the 1020px content height. The new PC workflow was explicitly requested;
the SDL UI retains its existing file-manager controls and shares the backend and
updated description. Native/PS5 and PC editor screens are not claimed identical.

Fresh standalone preview checks covered Home, details, search filtering, category
navigation, Settings and Files with no captured console errors or pairing
requirement. Optional cover-cache misses continue to use existing fallbacks.
**AI Studio deployment not verified. Actual PS5 LAN access/editing not verified.**
On the console, replace the saved ELF and launch through Payload Manager once.
Read the code in Files; pair the PC at the logged numeric IPv4 URL, then test
with a disposable `/data` file before inspecting the required system directory.
Verify read-only mounts/permission errors, code rejection, file transfers and a
concurrent-edit conflict. Direct AI Studio validation still requires hosting the
fresh generated `static-preview/` at the URL root and checking the core screens.

LAN-edit rollback restores the previous console-only request guard/client and
server protocol ID, removes pairing/editor/transfer controls and the shared
read/write entry points, restores translations, then rebuilds static preview and
ELF. User file contents are not automatically rolled back by reverting code.


## Code-free LAN access (current configuration)

This installation intentionally does not require an access code for PCs on the same
trusted private LAN. Earlier pairing instructions above describe the previous
protocol and are superseded by this section. The client obtains a random
per-server session token via a same-origin request; that token is checked on
read/write actions. Host, Origin, fetch-site and private-address checks still
apply, but **any device on this LAN can use the file manager**, including destructive
operations permitted by the payload's filesystem privileges. Do not use this
on shared/public Wi-Fi; keep the PS5 off untrusted networks. The standalone static
AI Studio preview still has no PS5 filesystem access.

Rebuild `static-preview/` and `build/poldrive.elf`, copy the new ELF to the
console, and start it again through Payload Manager. Port assignment may change;
read `/data/wolodrive-web.endpoint`. Rebuild/test locally and validate on device
before relying on it.
