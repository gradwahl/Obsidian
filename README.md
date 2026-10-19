# Wolodrive

SDL2 homebrew catalog interface for Linux and PS5.

### Artwork matching and box-art cache

Wolodrive's box-art enrichment uses three conservative stages: first-party
artwork URLs already present in the source, an exact **same-console and title**
match from another source, and [Libretro Named_Boxarts](https://github.com/libretro-thumbnails/libretro-thumbnails)
using exact or uniquely normalised filename matches. Unknown console
matches, ambiguous editions, and multiple regional covers are *not* guessed.
Libretro serves scans organised by console; those covers are not automatically
licensed for redistribution. Respect its service and copyright requirements.

The current results are in
[`docs/artwork_coverage.md`](docs/artwork_coverage.md). The report counts
links, **not verified images**. Artwork manifests are imported separately
in the React preview and read as an optional URL overlay by the native
catalogue loader. The existing asynchronous artwork cache loads images only
when a card is visible, and manual local art always wins.

To refresh matches after indexing titles:

```bash
python scripts/enrich_artwork.py --workers 4
python -m unittest discover -s tests -p test_enrich_artwork.py
npm run build:static
```

The importer downloads **directory listings only**, caches them in
`.cache/wolodrive-artwork/`, and does **not download cover images** by default.
To opt into caching at most ten unique matched images locally:

```bash
python scripts/enrich_artwork.py --offline --download 10
```

This uses the existing URL-keyed image cache at `assets/covers/`. Keep
downloaded cover bytes out of Git; the manifests consist only of source
IDs, image URLs and confidence. An optional **IGDB** importer can match modern games by exact cleaned
title **and verified console**. It runs locally with a Twitch client ID and
secret read from temporary process environment variables. No access token or
secret is saved or embedded in the web preview. It uses the IGDB API for
non-commercial projects under the provider's terms. Image URLs are saved, but
no image bytes are distributed.

**Security:** If a Twitch Client Secret was pasted into a chat, regenerate
it in the Twitch Developer Console before proceeding. Never add the new
secret to Git, a tracked configuration, a screenshot, or a command-line
argument.

From Windows PowerShell (in the Wolodrive repository):

```powershell
$env:IGDB_CLIENT_ID = Read-Host 'Twitch Client ID'
$private = Read-Host 'New Twitch Client Secret' -AsSecureString
$env:IGDB_CLIENT_SECRET = [System.Net.NetworkCredential]::new('', $private).Password
python scripts/igdb_artwork.py --dry-run
python scripts/igdb_artwork.py --max-queries 100
python scripts/enrich_artwork.py --offline
npm run build:static
Remove-Item Env:IGDB_CLIENT_SECRET
Remove-Item Env:IGDB_CLIENT_ID
```

From a Linux/WSL Bash terminal:

```bash
read -rp 'Twitch Client ID: ' IGDB_CLIENT_ID
read -rsp 'New Twitch Client Secret: ' IGDB_CLIENT_SECRET; echo
export IGDB_CLIENT_ID IGDB_CLIENT_SECRET
python3 scripts/igdb_artwork.py --max-queries 100
python3 scripts/enrich_artwork.py --offline
npm run build:static
unset IGDB_CLIENT_SECRET IGDB_CLIENT_ID
```

The script enforces a maximum of roughly **3 queries per second**, below
IGDB's 4-request-per-second limit, with retry/backoff. It searches missing
titles only, deduplicates by console/title and resumes via ignored local
cache `.cache/wolodrive-artwork/igdb-results.json`. IGDB queries now
remove **only trailing source distribution metadata**: `PS4`/`PS5` suffixes
for the matching console, region/language tags (such as `[EU] [EN]`),
bracketed update versions (such as `[1.05]`), and eShop/update markers.
Sequels, subtitles, `Deluxe Edition` and `Console Edition` remain part of
the actual game name. Exact cleaned-title AND matching-console checks are
still mandatory before any cover is accepted.

**If you already completed a batch before this title-cleanup improvement**,
rerun the *same* command. Previously cached `not-found` and `review` entries
are automatically retried **once when the cleaned query changes**. Previously
matched entries are retained, so do not delete the cache or use
`--retry-unmatched` just to apply this update. `--dry-run` shows how many
old searches qualify for retry without using credentials. Some entries may
still be missing if IGDB only lists a different edition or doesn't list that
title for its actual console.

The output
`assets/artwork/igdb-covers.json` contains approved cover image links, not
credentials. Repeat `--max-queries 500` batches or use
`--platforms nintendo-switch,sony-ps4,sony-ps5` to prioritise consoles.
Rerun `enrich_artwork.py --offline` after each batch; that merges IGDB into
the existing React and native manifests. Successful IGDB matches are
**cumulative across batches**, even after those titles have been merged into
the main artwork manifest and disappear from the missing-title queue.
If you need to regenerate the IGDB export from its locally saved results,
use `python3 scripts/igdb_artwork.py --max-queries 0`; this performs
**no network requests and requires no credentials**. Then rerun enrichment and
`npm run build:static`. Review uncertain titles rather than automatically
attaching unrelated artwork. The live API connection is not verified until
you configure fresh local credentials.

The native Linux/Windows/PS5 catalogue reads
`assets/artwork/cover-matches.tsv` alongside the snapshots. On PS5 copy it
to `/data/homebrew/wolodrive/artwork/cover-matches.tsv`. The web preview
loads `assets/artwork/cover-matches.json` as a separate deferred chunk;
it contains no image bytes. Native Windows runtime and PS5 deployment remain
unverified until tested on their respective platforms.

### Full public catalogue coverage

A source-by-source completeness audit is maintained in
[`docs/source_index_coverage.md`](docs/source_index_coverage.md).
In addition to Axekin, the static web preview now loads complete public
WordPress indexes (SuperPSX, DownloadGamePSP, Ziperto), TSV indexes
(NoPayStation, PSN-Content), Archive.org public collections (AlvRo and
maelly_pooh) and RomsTime's public game sitemap. GFS and RomHeaven are fully
indexed in the main `additional.json` snapshot using
`python scripts/import_sources.py --limit 64 --full-small`.

**Platform compatibility:** these new full indexes only retain Games/Roms
consoles, excluding Vita, PC games and unsupported categories. The website
sources use category/sitemap/platform labels, not guessed console names.
Full website indexes are deferred chunks in the React preview. The **native
app is still bounded** by a 26,000-entry in-memory catalogue and only uses the
smaller snapshots for these additional sources; bringing every new item into
the native app requires a compact or disk-backed catalogue, not merely a
larger fixed array. The audit lists six sources whose enumeration remains
unverified. No package downloads or license material are stored.

### Additional public website catalogs (metadata only)

The curated `config/additional_sources.json` file registers 17 website feeds and
indexes: the 15 requested external sites, plus the existing NoPayStation and
PSN-Content sources. Their public **game titles, platforms, artwork URLs and
source page links** are imported into `assets/catalogs/additional.json`.
No game archives, packages, license keys or direct download-host URLs are
downloaded or copied into that snapshot.

Update the snapshots from the repository root:

```bash
# Normal refresh: the 16 other sources are still capped per source.
python scripts/import_sources.py --limit 64
# Full Axekin rebuild: visit every /games page and filter to Games/Roms consoles.
python scripts/import_sources.py --limit 64 --full-axekin
python -m unittest discover -s tests -p test_import_sources.py
```

The normal importer fetches a bounded index per source (up to 400 entries each);
Axekin is the exception. With `--full-axekin`, all 840 pages are verified and
only titles explicitly tagged with a console in `src/libraryData.ts` and
`src/library.c` are kept. `config/axekin_platforms.json` maps Axekin's
platform slugs to the existing Games/Roms consoles; unsupported platforms
(including PlayStation Vita, which isn't in Games/Roms) are excluded. Games
with multiple supported consoles get one entry in each appropriate console.
As of the October 2026 crawl, 20,073 entries matched out of 20,993 games
across 840 pages; 920 had no supported console. The full catalogue is stored
in `assets/catalogs/axekin.json` (about 5.7 MB) rather than in the smaller
`additional.json` source snapshot. A failed or incomplete full crawl does not
overwrite the last successful full snapshot. A normal refresh preserves the
separate Axekin index and never reintroduces its first 64 titles as duplicates.

Other previously empty sites use specialized public metadata adapters:

- **Axekin** reads the paginated `/games` Inertia page-data attribute. Its
  publicly displayed titles, console associations and cover URLs are imported,
  never the `downloadLinks` field embedded alongside them.
- **RomHeaven** reads `/data/switch.csv`, the same public metadata feed its
  Switch page reads. Individual title pages use the site's filename-derived
  slugs; file-host payload URLs are excluded.
- **Retro Game Champion** reads the server-rendered console-gallery labels at
  `/games`. These are explicitly named **system collections**, not purported
  individual game dumps. Its first rendered gallery batch may omit more systems.
- **Archive.org / maelly_pooh** resolves the public uploader identifier from
  metadata of one known item, then queries that uploader's indexed items.
  The public uploader email is never written to the configuration or snapshot.

A failed refresh retains the last bounded snapshot and marks it stale instead
of silently clearing entries; inaccessible or unsupported indexes may remain
empty. Review each source's `status` and `note` fields. Importing metadata does
not validate that a package is available, safe or legally redistributable.

**Home** now displays games from all sources enabled in Settings, including
undated, website-only indexes. When several sources are enabled, the carousel
rotates among them instead of being filled by one dated Pegasus-DL feed.
Within each source, genuinely dated additions appear newest first; records
without a date retain the index's own order (not a claimed release date).
Disabling a source or the `catalog` format removes those entries from Home.
An Axekin-only source filter therefore displays Axekin listings only, once its
metadata has loaded. An empty selection shows the filtered-empty message.

The **React preview** bundles the small snapshot and loads the full Axekin
index as a **separate deferred chunk**. Rebuild with `npm run build:static`
after importing to update both. Only nearby cards are rendered in large console
carousels, and Search uses indexed title keys instead of pairwise comparisons.
Entries are searchable and selectable by source; platform metadata places each
matched entry under its Games/Roms console. Select the **Catalog metadata
(website only)** format filter to control these separately from installable
exFAT/FFPKG/PKG entries. Game details link to the source site's public listing
page; none of these metadata entries contain downloadable packages.

The **native Linux app** loads both local snapshots alongside Pegasus sources
when catalog synchronization runs. With a full Axekin index, its catalogue
capacity is 26,000 entries (including other sources), and console carousels can
navigate beyond 128 titles with Left/Right keyboard or D-pad navigation. The larger fixed
catalogue increases native memory usage; PS5 runtime behaviour is not verified.
Keep `assets/catalogs/additional.json` and `assets/catalogs/axekin.json`
in the repository. On **PS5**, copy *both* snapshots to:
`/data/homebrew/wolodrive/catalogs/additional.json` and
`/data/homebrew/wolodrive/catalogs/axekin.json` before synchronization.
This native source loader has been compiled and regression-tested on Linux;
PS5 SDK builds and on-device memory/performance remain unverified.

## React web preview

Catalog sources: DLPS Catalog, Pippo .exFAT, PFS Catalog, and ZER0GAME Catalog.
The broken evoX-CoreOS source is excluded from native catalog discovery, the
artwork downloader, and React's manifest source count, even if upstream lists it.
React still uses its bundled catalog; fetching the manifest does not import the
remote catalogs into the browser.

This branch also includes the React browser interface from `experimental`, alongside
the SDL2 app. The native application continues to use `make` and the C sources;
the browser preview uses the TypeScript/React sources and separate npm commands.
The browser UI is a separate preview with a bundled catalog, not a web connection
to the native app. Native renderer fixes and native-only features do not automatically
carry over to React. Package downloads/installations are unavailable in the web UI.

The React presentation follows `feature/game-information`: a 1920×1080 logical
stage scaled uniformly to the browser, a 272px sidebar, the full hero backdrop,
216×300 portrait artwork with PS4/PS5 strips, selected details footers, and a game
panel with neighboring covers and bottom tabs. Search uses the native three-column
grid with small portrait covers; the library uses four-column pages of eight tiles.
Non-16:9 windows have black margins to preserve these proportions. Browser fonts
use the native application's system-font families; glyph metrics can differ by OS.
This is a React recreation of the source layout; pixel-level appearance and input
behavior still need manual browser verification.
React card artwork fills the frame horizontally and vertically without cropping
or gaps. Images with different proportions stretch to fit the card. The missing-art
placeholder only appears when no image is available. This also applies to Search
and game-panel covers.

### Start without Node or Vite

The committed `static-preview/` folder contains compiled HTML, JavaScript, CSS,
the catalog, and versioned artwork. A host such as AI Studio can serve this folder
directly at the URL root, listening on `0.0.0.0:3000`.

From the repository in native Windows PowerShell:

```powershell
python -m http.server 3000 --bind 0.0.0.0 --directory static-preview
```

From Linux or WSL:

```bash
# For this checkout in WSL:
cd /mnt/c/Users/Callum/Documents/GitHub/wolodrive
python3 -m http.server 3000 --bind 0.0.0.0 --directory static-preview
```

Open `http://localhost:3000` in a browser, or the host's forwarded preview URL.
Stop the server with Ctrl+C. No Node installation is needed to serve this folder.
Use HTTP rather than opening `index.html` with `file://`. Serve at the URL root
because artwork uses `/assets/` URLs. The bundled catalog and versioned artwork
are local; remote covers, screenshots, and manifest requests require network access.
Six catalog cover paths have no versioned file: CUSA00900, CUSA18017, CUSA30410,
PPSA04610, PPSA07950, and PPSA10665. Local untracked artwork caches are excluded.

### Develop and rebuild React

Install Node.js 20+ (22 recommended), npm, and Git on Windows or Linux, then run:

```bash
npm ci
npm run dev
```

The Vite development server listens on `0.0.0.0:3000`. Stop any static server on
that port first. Keep Windows and Linux npm installations separate; native npm
dependencies in `node_modules/` must match the OS running the build.

```bash
npm run lint          # TypeScript checks
npm run test:web      # Geometry, scaling, and server-rendered component checks
npm run build         # Standalone production output in dist/
npm run build:static  # Regenerate the committed static-preview/ folder
python3 tests/check_static_preview.py  # Use python on Windows
```

Commit regenerated `static-preview/` files with React source changes. Both build
commands include versioned artwork, so their output can be hosted independently.
The preview duplicates about 36 MB of artwork for this purpose. Node/npm are
development dependencies of the React interface, not prerequisites for the native
SDL2 builds. This addition does not establish native Windows executable or PS5
build support; existing limitations below still apply. Browser appearance and
interactions require manual verification on Windows, native Linux, and WSL/WSLg;
builds and HTTP tests alone do not establish graphical correctness.

Integration validation (19 October 2026): React production/static builds,
TypeScript checks, and Node-free static HTTP delivery passed on native Windows
and Arch Linux under WSL. The native Linux build and full `make test` suite also
passed through WSL. Vite reports a JavaScript bundle-size warning. No manual
graphical checks, native Windows executable build, or PS5 SDK build were completed.

The layout update also passed React geometry/component tests on Windows and
Linux/WSL, including comparison with native layout constants, 16:9/4:3/ultrawide/
portrait viewport fitting, platform strips, selection, empty states, library page
size, and game-panel structure. These checks do not render a browser window.

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

Carousel motion accelerates and slows smoothly, preserving velocity across repeated
Left/Right input and reversals. Its timing is independent of frame rate, with
high-resolution elapsed time and bounds that keep cards inside the catalog.
The app requests VSync on the final renderer, including software rendering when
supported, and keeps a 60 FPS ceiling on all renderers. VSync synchronizes presentation
to the display; the frame cap also applies on displays above 60 Hz. Unsupported
backends use timed 60 FPS pacing. Missed frames do not trigger
catch-up bursts. Actual frame rates still depend on window size and rendering cost.
The WSL software-rendering default and renderer override remain unchanged.

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

The **Screenshots** tab shows up to six in-app image previews. Select one with
**Enter / Cross** or a click to open a full-screen viewer; **Left/Right / D-pad**,
mouse wheel, or the on-screen arrows change screenshots. **Escape / Circle** or
**X** closes the viewer and keeps the panel and selection. Images retain their
full aspect ratio and are never cropped like the box-cover cards. Loading and
unavailable images have explicit states in all nine languages.

Catalog `screenshots` / `screenshotUrls` arrays take priority, accepting unique
HTTP(S) URL strings or objects with a `url` field. When absent, a background worker
looks for local images, then [LaunchBox](https://gamesdb.launchbox-app.com/), then
an [official PlayStation game page](https://www.playstation.com/en-us/games/).
No API keys or accounts are required. LaunchBox discovery requires an exact
normalised title and console match in its search results (at most two search
requests, with a keyword fallback), then selects only
**Screenshot - Gameplay** images. Ambiguous matches, other platforms, covers,
title screens and logos are rejected. PlayStation discovery tries the game's
title slug and requires matching `searchtitle` metadata and screenshot-labelled
media. Explicitly age-gated blocks are skipped. These public HTML adapters depend
on source coverage and page structure; they do not guarantee screenshots for every
game. Named editions stay distinct, and DLC is not automatically matched to a base
game. Source name and matched page URL are shown in the gallery.

Optional `launchboxUrl` and `playstationUrl` catalog fields can identify a known
source page. A local `assets/screenshots/PPSA12345.sources` file can also contain
one source page URL per line: a LaunchBox `/games/images/...` URL and/or a
PlayStation `https://www.playstation.com/en-us/games/.../` URL. Supplied pages still
must match the game title (and console on LaunchBox). Only these page hosts and
their image hosts are accepted for provider discovery. Redirects stay within the
allowed page locations; failed requests and access challenges are not bypassed.

Local screenshots go in `assets/screenshots/PPSA12345/01.jpg` through `06.jpg`;
PNG and WebP are supported too (extension precedence: JPG, PNG, WebP). These files
are independent of covers. On PS5 use `/data/homebrew/wolodrive/screenshots` for
both local screenshots and `.sources` files. Runtime files are ignored by Git.

Discovery runs only when opening the Screenshots tab, with one worker, 32 memory
entries, a 2 MiB page limit, 5-second connection and 12-second request timeouts,
and verified HTTPS. LaunchBox page requests are spaced by at least one second;
PlayStation requests by at least five seconds, including redirects. Successful
metadata lists persist for seven days; empty/failed lookups are held for 30 minutes.
Image downloads reuse the existing bounded artwork worker/cache, including retries
and offline reuse. Remove a `.list` file and restart to force a metadata refresh;
changing a `.sources` file also changes its cache key. Workers join on shutdown.

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

To download all card artwork from the default catalog sources ahead of browsing,
install Python 3 and run from the repository on native Windows:

```powershell
python scripts/download_artwork.py
```

On Linux or WSL, use `python3 scripts/download_artwork.py`. The output defaults
to the repository's `assets/covers` folder, regardless of the launch directory.
For this Windows checkout that is
`C:\Users\Callum\Documents\GitHub\wolodrive\assets\covers`.
The app reuses these URL-keyed `.art` files without another image download;
they contain the original PNG, JPEG or WebP image bytes. For viewing individual
files in an external image viewer, use the image type recorded in
`assets/covers/artwork-index.json` to choose an extension for a copy.

The index maps files to game titles, title IDs and sources, and records failures.
The downloader fetches the catalog's supplied full box covers, cover fallbacks,
icons and card images, deduplicating shared URLs across packages. Sources with
only square icons retain those icons; this does not locate retail box art elsewhere.
Screenshots, game packages and separate background images are not bulk downloaded.
Files already cached are reused, invalid image signatures are retried, and
completed files remain available if a run is interrupted. Rerun the same command
to resume or fetch additions. There is no automatic disk size limit.

Downloads use four workers, verified HTTPS, a 12-second timeout, a 16 MiB image
limit and one retry. Failures leave existing files intact and return a nonzero
exit status; partial success remains usable. Use `--workers 1` for a slower
connection, `--catalog URL` for a particular catalog/source list, or `--output PATH`
to export elsewhere (the app reads its usual cache folder). No extra Python
packages are required. Local fixture tests passed on native Windows and Linux
through WSL with `python -m unittest discover -s tests -p test_download_artwork.py`
(use `python3` on Linux). This verifies the standalone downloader; native Windows
app builds/launch and a separate native Linux graphical session remain unverified.
The PS5 app/build is unchanged; transfer downloaded cache files to
`/data/homebrew/wolodrive/covers` for that target.
The initial bulk run cached 1,170 of 1,173 unique artwork URLs; SDL successfully
decoded all 1,170 files through WSL. Two source URLs returned HTTP 403 and one
provided an unsupported AVIF image. Those failures are recorded in the local index.

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

Cached images decode in a dedicated SDL worker, independent of the single network
download worker, so a slow/missing remote image cannot hold up other cached covers.
Texture creation stays on the UI thread. URL-keyed files in `assets/covers/*.art` share downloads across package
variants and change automatically when the URL changes. Existing title-ID `.cover`
files are left untouched and are no longer used as downloads, because they cannot
identify their source URL. Corrupt cached images are downloaded again and validated
by decoding before replacing the cache file. Failed images retry after 30 seconds
while requested. HTTPS verification remains enabled. Downloads are limited to
16 MiB and 12 seconds each. Decoded images fit within 1920×1080, pending decoded
surfaces plus in-flight loads are limited to three, and textures use a 48 MiB LRU budget across 64 slots.
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
`/data/homebrew/wolodrive/artwork`. Both artwork workers join before SDL/image shutdown.

Artwork location lookups, including missing local replacements, are cached in
128 memory slots. Visible/requested locations are rechecked after one second;
local file additions, removals and modification-time/size changes can therefore
take up to one second to appear. Catalog artwork URL changes take effect on the
next request. This avoids repeating filesystem probes for every card on every
frame, especially across WSL's Windows-drive filesystem. Missing/failed images
also recheck disk once per second while requested, so bulk-downloaded files can
become usable during the 30-second network retry cooldown.

The independent cached-image loader passed `make build-linux`, the full `make test`
suite and `git diff --check` through WSL. A local-server regression reproduced the
old queue blockage, then loaded a cached cover in 6 ms during a two-second network
stall after the fix. Another test loaded an imported cache file in 1,011 ms during
network retry cooldown without another HTTP request. The offline software navigation
benchmark measured 16.97 ms/frame (58.9 FPS) at 1280×720. Native Windows app builds
and launch, a separate native Linux graphical session and PS5 (SDK unavailable)
remain unverified; WSL's software renderer default and explicit override are preserved.
The updated WSLg preview was inspected fully visible, with readable text, loaded
covers, and live Right-key selection, carousel and background updates.

The subsequent scrolling fix expires artwork request priority after 100 ms without
a new request. This lets currently visible covers displace decoded backgrounds
from old selections. Discarding decoded work now requeues it without marking the
image as failed or imposing the former two-second cooldown. Network failure retry
delays, the three-load/surface limit and 48 MiB texture budget remain unchanged.
A headless rapid-scroll replay using 128 real catalog items and their local cache
reproduced 1,929–2,003 ms waits after stopping before the fix; the same eight stops
completed with all visible covers ready in 16–111 ms afterward. The offline UI
benchmark measured 15.95 ms/frame (62.7 FPS) during navigation at 1280×720.

This follow-up passed the Linux build and full automated suite through WSL,
including a regression that fills the decoded queue with old high-priority
backgrounds, requests a lower-priority visible cover, then revisits discarded
images. The standalone downloader tests also passed on native Windows. Native
Windows app build/launch still lacks a configured target/toolchain, a native Linux
graphical session is unavailable, and the PS5 SDK is missing. No live graphical
checks were performed for this follow-up: repository instructions prohibit
Computer Use and UI automation. Headless timing/tests do not verify live graphics.


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

UI text textures, including wrapped descriptions, use a per-app LRU cache with
128 slots and a 16 MiB texture budget. Text, font, color and wrapping width form
the key, so changing the selection or language produces the appropriate text.
Textures are released before fonts and the renderer during shutdown.

For a repeatable, offline navigation performance check on Linux or WSL, run
`make benchmark-ui` from the repository. It renders 128 fixture games at
1280×720 with SDL's software renderer and dummy video driver, reporting stationary
and moving-selection frame times. On the Windows-mounted repository through WSL,
the initial cache change reduced navigation from 206.25 ms/frame (4.8 FPS) to
15.44 ms/frame (64.8 FPS), with stationary frames falling from 184.84 to 10.82 ms.
These are fixture measurements; real artwork, screen sizes and machines affect
performance. WSL's software-rendering default and `SDL_RENDER_DRIVER` override
are unchanged.
This benchmark measures uncapped render cost; it does not run the main loop's
60 FPS pacing.

Startup prints `VSync: enabled; frame limit: 60 FPS` when the renderer reports
VSync enabled, or `VSync: unavailable (using frame pacing)` when unsupported.
SDL 2.0.18 and newer explicitly enable VSync after renderer creation; older SDL
builds retain the creation-time VSync request and the timed 60 FPS cap. WSL still
defaults to software rendering and honors the explicit renderer override.
The VSync follow-up passed the Linux build and full suite through WSL, including
supported/unsupported backend tests. The WSLg preview's software backend reported
VSync enabled at startup. This is backend/startup evidence, not a visual check of
presentation or tearing. Native Windows app, native Linux graphics, and PS5 remain
unverified because of the existing toolchain/session/SDK gaps; Computer Use and
UI automation were not used.

The smooth-scrolling change passed `make build-linux`, the full `make test` suite,
`make benchmark-ui` and `git diff --check` through WSL. Motion tests cover initial
acceleration, repeated selection changes, reversals, edges, slow frames, settling,
and matching motion at 30/60/120 FPS. The pacing test completed 12 frames in
0.200 seconds and checks deadline recovery after an overrun. Uncapped software
navigation measured 15.51 ms/frame at 1280×720. Live graphical checks were not
performed because repository instructions prohibit Computer Use/UI automation.
Native Windows app build/launch lacks a configured target/toolchain, a separate
native Linux graphical session is unavailable, and PS5 verification is blocked
by the missing SDK. These remain unverified.

The navigation cache change passed `make build-linux`, the full `make test`
suite and `git diff --check` through WSL. Tests cover text reuse, UTF-8, wrapping,
font/color/translation changes, eviction and cleanup, plus repeated positive and
negative artwork lookups, expiry and local override changes. The visible WSLg
preview displayed loaded covers and readable Home/game-panel text. Controlled
live navigation checks could not be completed while user input was active.
Native Windows still has no configured build target/toolchain, a separate native
Linux graphical session is unavailable, and `make ps5` is blocked by the missing
payload SDK. These platforms are not verified by the benchmark or WSLg preview.

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

Screenshot-source validation covers exact platform/title matching, ambiguity,
image-type and host filtering, entity decoding, explicit age gates, cached success
and failures, source changes, catalog/local priority, worker shutdown, and viewer
keyboard/controller/mouse navigation. Live discovery returned six PS5 gameplay
images for Stray from LaunchBox and six Tales of Arise images from PlayStation.
Hellblade II currently has no matching PS5 gallery in the tested LaunchBox search;
a missing or unrecognised official screenshot gallery is shown as an empty state.

The screenshot gallery change passed `make build-linux`, `make test` and
`git diff --check` through WSL. Its loading and empty states were visible and
readable in WSLg. Further Computer Use was stopped with the physical Escape key;
loaded gallery images and full-screen navigation remain visually unverified.
Native Windows has no configured build target/toolchain, a separate native Linux
graphical session is unavailable, and `make ps5` is blocked by the missing SDK.
