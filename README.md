# Wolodrive

SDL2 homebrew catalog interface for Linux and PS5.

## TV catalog interface

The interface follows the supplied Netflix TV reference: a dark left navigation
rail, selected-game artwork across the background, a readable title and description,
and horizontal rows of portrait box-cover cards. The selected cover has a white border
and action footer. WoloDrive uses its own branding and the catalog's game artwork;
portrait covers and square fallback icons fill the case below its platform strip,
preserving proportions and cropping overflow centrally to avoid black gaps. PS5 cards have a white platform header and PS4 cards a blue one;
complete box-cover images keep their original branding. Titles sit below the art. Backgrounds prefer separate widescreen
artwork, with a softened, darkened cover fallback when none is supplied.

Use **Left/Right** or the controller **D-pad** to select a game. Press **Left**
again at the first card or leftmost grid column to focus the far-left sidebar.
Only while the sidebar has its white focus outline, **Up/Down**
switch between **Search**, **Home**, **Games/Roms**, **Emulators**, **Files**, **Payloads**,
**Downloads**, and **Settings**. Press **Right / Enter / Cross** to return to the content without
changing its selection. Up/Down within content stays in the current section;
at the top of a grid, Up keeps focus in the grid.
Each sidebar entry has a text label and an icon: magnifier, house, gamepad,
joystick, folder, chip, download arrow, and settings sliders. Click an entry to switch sections,
or click a cover to open its game panel. **Enter/Space** or controller **Cross**
also opens the focused game's panel. Download and installation availability is described
below. Each category remembers its selection; the row scrolls to bring that game
to the left edge. **Backspace / Circle** returns to the start or cancels an active
download. **R / F5 / Triangle** refreshes the catalog. Catalog sync and storage
information are shown along the bottom. Language controls are at the top left.

**Search**, above Home, opens an on-screen A–Z/0–9 keyboard beside a three-column
cover grid. Type with a physical keyboard (including Unicode text), click the
on-screen keys, or select keys with arrows / D-pad and **Enter / Cross**. **Space**
and **Delete** keys are above the letters; **Backspace / Triangle** also deletes
one character. Search matches title or title ID anywhere in the loaded catalog,
including entries outside the 128-item browsing rows. ASCII letters match without
case sensitivity; other scripts match their entered UTF-8 text. An empty query
shows the loaded catalog. Unconnected sources are not searched.

Search groups the same title across package formats and source typography
(format prefixes, punctuation and trademark symbols). PS4 and PS5, DLC and named
editions stay separate. The game panel's **Install** tab lists retained source
packages with their source, format, title ID and version. Catalog entries and
download choices are preserved; only the search cards are grouped.

Move Right past the keyboard's right edge to browse results. Up/Down pages through
all matches, nine covers at a time. Left from the first result column returns to
the keyboard; Left from the keyboard's first column reaches the sidebar.
**Enter / Cross**, or clicking a result, opens its description and package details.
**Escape / Backspace / Circle**, or the panel's **X** button, returns to the
same results. Package actions are inside the **Install** tab.
In the search grid, **Escape / Circle** focuses the sidebar. **F5** refreshes the
catalog. The search query persists when switching sections; typing letters there
does not trigger catalog shortcuts. The globe and **Square** still open languages.

### Settings and catalog filters

**Settings**, directly below Downloads, has independent checkboxes for each
download source and package format. Tick any combination of sources, using the
loaded catalog's website/source labels, and **.exfat**, **Folder (.ffpkg)**, or
**.pkg** formats. **All sources** and **All formats** select or clear their groups;
a dash indicates a partial selection. Clearing every source or format deliberately
shows no matching games. Source choices combine with OR, formats combine with OR,
and both groups must match. These filters apply together to Home,
Games/Roms and its console counts, Downloads, Search, and Install/DLC package cards.
Filtering happens before the 128-item list limits and Search's duplicate grouping.
All source and format packages are retained in the catalog; re-syncing updates
only the matching source/format/title-ID/title record. Home still shows the newest
dated copy of an identically named game per console family.

Focus Settings with **Right / Enter / Cross** from the sidebar. **Up/Down** moves
through checkboxes; **Left/Right** moves between Sources and Formats.
**Space / Enter / Cross**, or a mouse click, toggles the focused checkbox.
Long source lists page as focus moves; the mouse wheel also moves through sources.
**Left** from Sources or **Circle / Escape** returns to the sidebar.
**Reset filters** checks every source and format. Changes apply immediately and save automatically to
`filters` beside the saved language preference (SDL's user preference directory on
desktop, `/data/homebrew/wolodrive/filters` on PS5). Save failures are reported while
keeping the session's changes. Missing or invalid files default to all sources
and formats. Existing single-source/single-format preferences migrate to the same
checkbox selections. Saved sources missing from the current catalog remain in the
checkbox list so they can still be deselected; empty lists explain how to reset the filters. Source choices always come
from the full loaded catalog, including sources hidden by the current filters.

### Game panels

Opening a game from Home, Games/Roms, Search or Downloads expands a dark panel
with a cover strip, backdrop, title and four tabs: **Install**, **DLC**,
**Screenshots** and **Details**. **Left/Right** changes tabs; **Down / Enter / Cross**
focuses the content. Use **Left/Right** to browse package or DLC cards, then
**Enter / Cross** to select. **Up** returns focus to the tabs. Mouse clicks work
on tabs and cards. **Escape / Backspace / Circle** returns to the original list;
from a DLC panel it first returns to the parent game. **X** closes the panel.

Install lists source packages and invokes the existing package action. Installation
and downloads are still unavailable; the panel says so and never simulates success.
DLC includes entries with `category: "dlc"` whose `baseTitleId` or `parentTitleId`
matches the game, or whose own title ID matches when no parent ID is supplied.
Unrelated entries are excluded, and missing relationships show an empty state.

Screenshots accepts up to six unique HTTP(S) links from a catalog's `screenshots`
or `screenshotUrls` array, as URL strings or objects with a `url` field. Selecting
a link opens the source image in the desktop system's default browser/viewer.
These are external links, not an in-app screenshot gallery. PS5 cannot currently
open them and reports this explicitly. Sources without screenshot metadata show
an empty state; cover artwork is never presented as gameplay screenshots.
Details shows the catalog description and package metadata. **Page Up/Down**,
the mouse wheel or controller Down scrolls text; **I / right stick** toggles the
original description when online translation is configured.

Home shows the **128 latest catalog additions**, newest first across the loaded
sources. It includes explicit `category: "game"` entries with valid UTC `createdAt`
timestamps, excluding DLC, utilities, and entries without a usable date. The date
beside the focused game's metadata is its catalog addition date, not its original
retail release date. If multiple sources list a game, its newest addition timestamp
is used. Refreshing keeps a selected game when browsing beyond the first card;
the first card follows the newest addition. Without dated catalog data, Home
shows a refresh prompt instead of calling the built-in sample games new releases.

The catalog holds up to 4096 entries, accommodating the current default sources;
sources beyond that limit are not retained. Artwork loads independently of catalog
sync: selection first, visible cards next, then a small nearby window. Search and
game panels use the same cache, including games beyond the former prefetch limit.

## Game artwork

Catalog entries accept `boxArtUrl`, `boxartUrl` or `boxCoverUrl` for a complete
portrait front cover including its platform branding. They retain
`posterUrl` / `coverUrl` / `cover` / `image` as cover
fallbacks and `iconUrl` / `icon` as square icons. Optional `landscapeUrl`, `cardUrl`
or `bannerUrl` supplies card artwork; `backdropUrl`, `backgroundUrl` or `heroUrl`
supplies a separate backdrop. Screenshots are never substituted for these fields.
Cards prefer complete box art, then covers, icons and finally landscape artwork; backgrounds prefer their
own artwork, then landscape art, then a softened cover/icon. Failed preferred
artwork falls back to the next available artwork while retrying in the background.
This does not add a retail box-art database: sources that only supply square images
continue to use those images inside portrait cases, filling the artwork area
without stretching and cropping the sides where necessary.

Images download and decode in an SDL worker; texture creation stays on the UI
thread. URL-keyed files in `assets/covers/*.art` share downloads across package
variants and change automatically when the URL changes. Existing title-ID `.cover`
files are left untouched and are no longer used as downloads, because they cannot
identify their source URL. Corrupt cached images are downloaded again and validated
by decoding before replacing the cache file. Failed images retry after 30 seconds
while requested. HTTPS verification remains enabled. Downloads are limited to
16 MiB and 12 seconds each. Decoded images fit within 1920×1080, pending decoded
surfaces are limited to three, and textures use a 48 MiB LRU budget across 64 slots.
Disk files persist for offline reuse; they currently have no automatic size limit.

For local replacements, add PNG, JPG or WebP files under `assets/artwork/`:

- `PPSA12345.box.png` supplies a complete front cover including its platform header.
- `PPSA12345.card.png` replaces the card image inside the generated platform header.
- `PPSA12345.backdrop.jpg` replaces the backdrop.
- `PPSA12345.png` is a shared fallback for both roles.

Use the exact catalog title ID, including case on Linux. Role-specific files take
precedence over shared files; `.box` takes precedence over `.card` for cards,
and extension order is PNG, JPG, WebP. Adding/removing
files or changing their modification time/size updates the requested artwork.
Local replacements and downloaded cache files are ignored by Git. On PS5 the
cache is `/data/homebrew/wolodrive/covers` and replacements belong under
`/data/homebrew/wolodrive/artwork`. The worker joins before SDL/image shutdown.


**Games/Roms** opens large square company tiles for **Sony**, **Microsoft**,
**Sega**, **Nintendo**, **Atari**, **SNK**, **NEC**, **Bandai**, **Other consoles**, and
**Arcade systems**. These ten groups contain 58 systems: 6 PlayStation, 2 Xbox,
8 Sega, 14 Nintendo, 5 Atari, 4 SNK, 4 NEC, 2 Bandai, 8 other consoles, and 5 arcade
entries. PS3 and Xbox 360 are labeled experimental. PS4 and PS5 remain available;
Vita, Xbox One, Xbox Series and Switch 2 are excluded from this list.
Opening a company shows its consoles, including handhelds. Company tiles use
their logos; the mixed Other consoles category uses its name. Console tiles show
hardware photos, a console logo where available, and a small name label. Arcade
tiles use system logos. Nine group logos, 56 platform logos and 53 transparent
hardware images are bundled under `assets/library/`
and work offline. Cards use charcoal `#181818`; only the focused card has a white
selection border. Hardware images retain their source alpha and colors, with no
runtime background removal. Transparent padding is excluded from sizing so the
products fill a consistent image area without stretching or cutting off hardware.
Company logos retain their original brand colors, including blue SEGA and SNK,
red Nintendo and Atari, and Microsoft's four-color symbol. Sony's black wordmark
and the console logos are displayed in white for contrast, preserving their alpha.
Missing assets fall back to a readable name. Super Cassette Vision has a hardware
photo and name without a separate logo. The 32X and Super Cassette Vision use cutouts
prepared with imagegen from the attributed photos; other cutouts are sourced PNGs.
Sources, authors, individual image licenses,
and download hashes are recorded in [the artwork credits](assets/library/CREDITS.md)
and `assets/library/SOURCES.json`.

This list organizes the browser; it does not install emulators or assert that
emulation is configured. The current game catalog maps PS4/PS5 title IDs only;
other systems show an empty catalog until a corresponding source is connected.

**Emulators** uses the same company → system layout, all ten groups and 58 systems,
artwork, charcoal cards, selection border, paging and input controls. Headings and
breadcrumbs identify the Emulators section, and system cards show emulator counts.
No emulator source is currently connected: opening a system shows that explicitly,
without listing game packages or offering a simulated emulator download.

For PS5 deployment, copy `assets/library/` to
`/data/homebrew/wolodrive/assets/library/` alongside the payload. Desktop builds
look in the working directory and relative to the executable (including the
repository's `build/` layout).

Click a tile to open it, or press **Right / Enter / Cross**
from the sidebar to focus the grid, then use arrows / D-pad and **Enter / Cross**.
Long console lists page automatically as selection moves past the first eight
tiles. **Backspace / Escape / Circle**, or the on-screen **Back** button, returns
from games to consoles and then to companies. **Left** at the start of a grid row
or game list returns focus to the sidebar; Up/Down then switch sections. Clicking
Games/Roms in the sidebar resets it to the company grid.

Selecting **Sony → PlayStation 4** or **Sony → PlayStation 5** shows up to 128 games
in catalog order, identified by explicit game entries and their CUSA/PPSA title IDs.
Other consoles currently show that no catalog or ROM source is connected; adding
their navigation tiles does not implement emulation or ROM scanning. Home's latest
additions remain independent of the selected console.
Emulator management, the file browser, and
payload management are not implemented; the corresponding sections explicitly
show that they are unavailable. Downloads shows downloading and installed entries,
or an empty-state message. These navigation entries do not enable new backends.

## Languages

Click the **globe and current language at the top left** to choose Polish, English, French,
Dutch, German, Spanish, Italian, Chinese (Simplified), or Russian. The list shows
each language's own name. Press **L** or **DualSense Square** to open or close the
menu. The control matches the sidebar entries and has an arrow for its dropdown;
the dropdown stays inside the sidebar. Use **Up/Down** or the **D-pad** to navigate, and press **Enter/Space** or
**Cross** to select. **Escape/Backspace** or **Circle** cancels; clicking outside
also dismisses the menu. Switching updates the interface immediately and
preserves catalog selection and downloads. Mouse selection follows the window's
logical scaling when it is resized.

On first launch, the app detects `LC_ALL`, `LC_MESSAGES`, then `LANG`. Unsupported
locales default to English. A saved choice takes priority on subsequent launches.
Linux stores the choice in SDL's user preference directory (normally
`~/.local/share/Wolodrive/Wolodrive/language`); PS5 uses
`/data/homebrew/wolodrive/language`. If saving fails, the language still changes
for the current session and a notification explains that it could not be saved.

Built-in descriptions, labels, the window title, notifications and catalog sync
status are translated. Remote descriptions can also be translated using the
configurable service below. Game titles, publisher/source names, IDs, paths,
technical abbreviations and text embedded in cover images retain their original
wording. Description markup is converted to plain text:
HTML tags are removed, paragraphs are separated, and character entities and
JSON Unicode escapes are decoded. This setting changes the app's
interface language; it does not change the language inside games.

Translations live in `src/translations.def` as UTF-8 text with stable `TextId`
keys. Keep all printf placeholders identical across translations. Rendering uses
SDL_ttf UTF-8 APIs. Install **Noto Sans CJK** on Linux for full Latin, Cyrillic and
Chinese coverage (Arch: `noto-fonts-cjk`; Debian/Ubuntu: `fonts-noto-cjk`). The app
also finds Microsoft YaHei in Windows' font directory, including through WSL.
Alternatively, provide a font covering all three scripts as `assets/font_cjk.ttc`
(on PS5: `/data/homebrew/wolodrive/assets/font_cjk.ttc`). Custom `assets/font.ttf`
and `assets/font_bold.ttf` are also searched. Fonts covering all scripts are
preferred; a warning is printed if the selected font cannot display Chinese.
Fonts are loaded locally; no font downloads occur when a language is selected.

### Online catalog descriptions

Configure a [LibreTranslate-compatible `/translate` endpoint](https://docs.libretranslate.com/api/operations/translate/)
before launching the app. In a Linux or Windows **WSL terminal**:

```sh
export WOLODRIVE_TRANSLATE_URL="https://your-translator.example/translate"
# Optional, only if your provider requires an API key:
export WOLODRIVE_TRANSLATE_API_KEY="your-provider-key"
make linux
```

Replace the example URL with your own provider or self-hosted endpoint. There is
no bundled public translation server or API key. For a self-hosted local service,
an endpoint such as `http://127.0.0.1:5000/translate` is also accepted. The server
must support source-language detection and the selected target language; its
installed language models determine translation availability and quality.
The same environment variable names apply to a native Windows executable when a
native build becomes available; set them in that process's launch environment.

Only the focused game's plain-text description is sent to the configured
provider. Source-language detection is automatic. Requests run in an SDL worker
thread and up to 64 source-text/language results are cached in memory for the
current run. Language changes never display an old translation for a different
language or source description. Pending navigation requests are replaced by the
latest requested description. TLS verification stays enabled; API keys are sent
in the request body and are not logged. Provider charges or rate limits apply.

Press **I**, click **Original**, or press the **right stick** to view the
publisher's original description and switch back. Use **Page Up/Page Down**, or
the mouse wheel over the description, to read longer text. Up to 4 KiB of source
description and 32 KiB of service response are accepted. Changing the language or
focused game resets the original-text view and scroll position.

While a request is pending or unavailable, the app shows a message in the selected
language rather than silently displaying English. Without configuration, English
mode keeps showing the original, while other languages explain that a service
needs configuring. Built-in descriptions work offline in all nine languages.
Failed requests are held for 30 seconds before retrying. Restart the app after
changing service configuration.

## Build and checks

On Linux, install GCC, Make, pkg-config, Python 3, SDL2, SDL2_ttf, SDL2_image,
libcurl and Noto Sans CJK fonts, then run:

```sh
make build-linux
make test
make linux
```

Tests validate translation completeness and printf placeholders, locale
precedence, fallback, preference persistence, mouse/keyboard/controller menu
selection, cancellation, glyph coverage for every translation, and SDL rendering
with a dummy video driver. They use a temporary preferences
directory. Set `WOLODRIVE_TEST_SCREENSHOTS=1` when running `make test` to save
screenshots for each language in `build/`.

Search regression coverage includes full-catalog title/ID matching beyond the
carousel limit, UTF-8 deletion, live text input, keyboard/controller/mouse
navigation, result paging, refresh selection, empty states and result details.
It also reproduces duplicate source typography/format labels, verifies grouping
without merging DLC, named editions or PS4/PS5, and exercises retained package
selection with keyboard, controller and mouse events.
Game-panel regressions check Home/Games/Search activation, tab navigation,
package availability, explicit DLC relationships, screenshot-link parsing and
filtering, empty states and modal close/return behavior. Saved SDL renders were
inspected for layout, and the panel with real artwork and readable details was
inspected in WSLg. Live interaction checks were interrupted by Escape.
Settings regressions cover combined multi-source/multi-format filters before list
limits and grouping, all/partial/empty checkbox selections, source package retention,
paging, saved and malformed preferences, legacy preference migration, unavailable
sources, reset, and mouse/keyboard/controller input. The checkbox layout was
inspected in saved SDL renders; fresh WSLg checks were interrupted by Escape.
Native Windows and PS5
builds and a separate native Linux graphical session remain unverified.
The Linux build and automated search checks were run through WSL. Native Windows
and PS5 builds and a separate native Linux graphical session remain unverified;
the WSLg Search screen's visual and live interaction checks were interrupted.

Regression tests also check description cleanup and prevent false installation
claims. Gradient coverage is checked at four window sizes with opaque and
translucent drawing, including the fallback for older rendering backends.
Online translation tests use a local mock HTTP server to verify the provider
contract, Unicode, all nine target codes, caching, source changes, failure states,
request replacement and worker shutdown. Tests do not contact a real provider or
use your configured service credentials.

## Package availability

This build browses remote catalogs and caches covers. Package downloading,
installation and launching are not implemented yet. All catalog entries start
as available; sample installed states and timer-driven download progress have
been removed. Selecting a package action explains that it is unavailable and
does not claim a download or installation succeeded. Remote catalog metadata
and cached cover images are not evidence that a game is installed.

For PS5, configure `PS5_PAYLOAD_SDK` and run `make ps5`.

Native Windows remains a project requirement, but the repository currently has
no native Windows build target. Existing platform gaps include POSIX storage
queries and GNU string functions. Artwork and catalog workers use SDL threads;
this does not resolve the remaining native Windows build gaps. The Windows preview
below runs the Linux build through WSL; it does not verify a native Windows
executable. A separate native Linux graphical session is also required to verify
Linux display behavior independently of WSLg.

## Windows preview through WSL

Run `make linux` from your WSL distribution in this repository. WSL uses SDL's
software renderer by default because some WSLg accelerated drivers create a
window successfully but display only black. Other Linux environments and PS5
continue to prefer accelerated rendering. The selected video driver and renderer
are printed at startup.

To test a particular renderer, set `SDL_RENDER_DRIVER` explicitly. For example,
to force software rendering when troubleshooting an older build:

```sh
SDL_RENDER_DRIVER=software ./build/wolodrive
```

If the app reports missing fonts, install Noto Sans CJK as described above.

Artwork validation: the Linux build and automated suite run through WSL, including
local HTTP fixtures for cache corruption, retries, URL changes, package sharing,
separate/soft backgrounds, local overrides, fitting, GPU eviction and shutdown.
Native Windows builds/launch, PS5 builds (SDK unavailable), and a separate native
Linux graphical session remain unverified in this environment.
Visible WSLg covers, softened backgrounds and readable text were inspected during
this change. Final live navigation checks were stopped with the physical Escape
key; no further Computer Use actions were taken, so they remain incomplete.

Portrait-case validation uses the same artwork HTTP fixtures to check box-art
priority over covers/icons, complete-cover branding and local `.box` overrides,
plus the existing navigation and rendering regressions. Native Windows/native
Linux graphical checks and PS5 build verification retain the limitations above.
The portrait-case follow-up passed the Linux build and full automated suite through
WSL. The visible WSLg case cards, titles and selection/backdrop update after a
Right key were inspected. The preview window was partly off-screen, so full-window
layout and native Windows/native Linux graphics remain unverified.

The vertical-fill adjustment passed the Linux build and full automated suite via
WSL. A fully visible WSLg window confirmed artwork fills the cases below the
platform strip, readable text, and Right-key selection/backdrop updates. Native
Windows/native Linux graphical sessions and PS5 verification remain unavailable.
