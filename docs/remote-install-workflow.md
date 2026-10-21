# Console catalogue and remote installation

The requested outcome is one catalogue usable on PS5, keyboard/pointer and LAN
browsers, with both interfaces controlling downloads onto the PS5 and real
PS4 PKG installation or PS5 image/folder preparation and mounting. The current
LAN ELF is an incomplete foundation: it serves the catalogue but implements no
download queue or installation action. No generated ELF should be advertised as
a completed remote installer.

## Confirmed source example

The user supplied [Axekin's Driveclub page](https://www.axekin.com/games/CUSA00093).
Browser inspection on 2026-10-20 showed title ID CUSA00093, two base-game links,
two update links and fourteen DLC links. The base-game section names DataVaults
and FileMirage. Plain HTTP requests to the Axekin game page received an anti-bot
document rather than the game data; the normal browser could display the page.
The existing importer expects Inertia data and cannot treat that challenge as a
valid game response.

The DataVaults selection is a file-host page for a `.pkg.rar` archive, not a raw
PKG response. Its free-download form advances to a countdown and CAPTCHA widget.
This interaction must remain a user step unless the provider offers an approved
direct-download/API workflow; the app must not implement CAPTCHA bypassing.
No package body was downloaded during investigation.

The FileMirage destination copied from the base-game section identified itself
as `DRIVECLUB [117 DLCs][CUSA00093](axekin.com).zip`. That is an observed mismatch
between source grouping and destination metadata, not proof of archive contents.
The app must flag it and validate the actual files before installation. A matching
title ID alone does not establish that an archive is the base game rather than DLC.

## Required implementation

1. Keep each entry's exact source/game-page identity. Preserve title/platform,
   region, version and package role; resolve the website's base/update/DLC links
   into distinct host choices, never interchangeable guessed URLs.
2. Use host adapters to distinguish HTML landing pages, unavailable files,
   direct downloads, multipart sets and interactive gates. Mark unsupported or
   interaction-required responses honestly on both native and browser screens.
3. Download in a PS5 worker with observed byte progress, cancellation and partial
   file handling. Closing the remote browser must not cancel the console job.
   Do not send arbitrary browser form posts through an unrestricted proxy.
4. Inspect/extract archives before installation, checking paths, contents, title
   identity, base/update/DLC role and expected formats. Do not pass RAR/ZIP HTML
   responses directly to the PS5 PKG installer.
5. Invoke and observe the actual PS5 PKG installation or image/folder mount
   backend. Queue acceptance, completed download, extracted archive, installer
   submission and confirmed installation/mount are separate states. No elapsed
   time or catalogue metadata can establish success.
6. Expose the same queue/action results to PS5 and LAN clients. LAN control needs
   an intentional pairing/authorization mechanism when mutation endpoints are
   added. The current server exposes only read-only catalogue routes.
7. Keep the standalone AI Studio preview working without this backend, with
   unavailable actions when no console service is connected. Rebuild its static
   artifacts when browser/shared-data inputs change and compare affected native
   and web screens at matching state/resolution.

## Confirmed host workflow and outstanding verification

The user has confirmed website-derived links, both PS4 and PS5 workflows, and
browser-assisted host interactions. A user completes the host page/countdown or
CAPTCHA, then Obsidian continues. This requires an explicit handoff of a usable
file URL or provider-supported session; opening a separate host tab alone does
not supply the download URL to the native worker. Browser cookies and successful
CAPTCHA sessions must not be assumed to exist in a PS5 HTTP client. The initial
handoff should therefore accept the host's actual downloadable URL and validate
its response before queuing it, or report that the host cannot yet be continued.
This handoff and the install/download backend are not implemented yet.

The PS5 LAN service has been checked directly: HTML, JavaScript and CSS returned
successfully, and the corrected isolated browser helper returned 0. The user
confirmed the catalogue visible and confirmed the new Obsidian Media shortcut
reopens it. The console's LAN address is used only for diagnostics, never as an
app default.

PKG Manager 1.5.1 status/version/storage APIs and ShadowMountPlus's loopback
version/capability API were checked without submitting an install or mount.
These checks establish service availability only. Download/extraction queues,
host-session handoff and observed install/mount completion remain outstanding.
