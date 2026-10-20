# DLPSGame PS3 artwork — 20 October 2026

The user supplied https://dlpsgame.com/category/ps3/. Its canonical category
page identifies WordPress category 64 as PS3. The public REST API returned
3,191 posts across 32 pages of at most 100 entries. All page totals and unique
post IDs were reconciled. Category IDs, source-page hosts and image hosts were
checked; six legitimate source URLs contain an encoded zero-width space and
are accepted without allowing encoded path traversal.

Only post ID, title, console, source URL and primary image URL are cached.
Post bodies are parsed in memory and discarded. No game files, download-link
payloads, license data or raw post HTML enter the index. This adds an artwork
provider, not a new game catalogue, and is separate from the existing DLPS
catalogue source. No browser or desktop UI automation was used.

## Results

| Catalogue entries | Before | After |
|---|---:|---:|
| Total | 85,601 | 85,601 |
| Direct artwork | 20,393 | 20,393 |
| Matched artwork | 48,039 | 48,286 |
| Missing | 15,999 | 15,752 |
| Awaiting review | 1,170 | 1,170 |

The first matching pass found 259 candidate entries and downloaded 134 distinct
images, all of which decoded successfully with Pillow. Saved contact sheets
were visually screened. Five source images were withheld, affecting nine
candidate entries: NFL Blitz, Family Feud, Deadly Premonition: The Director's
Cut, Floating Cloud God Saves the Pilgrims and Enchanted Cavern. Reasons and
exact post/image URLs are recorded in config/dlpsgame_artwork_exclusions.json.
Observed issues included legacy/platform-uncorroborated artwork, a missing
named edition, HD/Vita branding and a sequel cover for the first game.
These exclusions also filter cached exports on subsequent runs.

The final export retains 250 candidate entries using 129 images. Enrichment
approved **247 previously missing entries**, using **128 distinct images**;
the other three candidates supplement existing review entries. All 65,819
previous manifest rows remained unchanged. No earlier approved URL, provider,
confidence or independent fallback was replaced.

Examples include NormalTanks, Killzone 3 Multiplayer, Ratchet and Clank - Into
the Nexus, Rocksmith 2014 Edition, Assault Heroes, Crysis HD, Beyond Good and
Evil HD and Sniper Elite 3 Ultimate Edition. Source covers can be custom
composites or logos; they are not represented as uniformly official box scans.

Matching requires one exact cleaned title on a PS3-category post. The site's
trailing PSN storefront marker and zero-width formatting character are removed
for comparison; edition, HD, demo, DLC, mod and sequel words remain distinct.
Titles on another console and conflicting same-name posts are not accepted.
A successfully downloaded image is required before export. Existing review
entries remain withheld rather than being automatically approved by this
additional provider.

## Reproduction and verification

Run python scripts/dlpsgame_artwork.py on Windows, or python3 on Linux/WSL.
Use --refresh to fetch current PS3 category metadata. Use --offline to reuse
the saved index/export and valid cached images without making any requests.
The index is .cache/wolodrive-artwork/dlpsgame-ps3.json; download diagnostics
are in the same directory. Covers remain in assets/covers, using the existing
URL-hash filenames. Invalid/incomplete metadata does not replace a prior index.

- Four DLPSGame tests passed on native Windows and Linux/WSL Python: console
  and edition boundaries, duplicate-post ambiguity, sanitized metadata,
  encoded source URLs, malformed image URLs, source-specific exclusions,
  incomplete refresh preservation and enrichment priorities.
- Existing artwork enrichment, LaunchBox, GameTDB, IGDB, review-report and
  downloader tests passed in the applicable Windows and Linux test runs.
- Windows static/web build, web tests and static HTTP-delivery checks passed;
  the existing Vite large-chunk warning remains.
- Linux native build and full make-test suite passed under WSL. Focused
  DLPSGame/review tests passed after the saved-image exclusions were added.
- All 134 downloaded images decoded; saved sheets covering those candidates
  were inspected, and the final withheld-art gallery was refreshed to include
  the new evidence. Its 1,170 entries, 320 sheets and 97 unavailable tiles remain
  pending review. No challenge was bypassed for unavailable images.
- A repeat offline run preserved the export, overlays, review queue and
  coverage report byte-for-byte.
- Native Windows application build/launch, PS5 build, and manual graphical
  checks on Windows/native Linux/WSL remain unverified. Native rendering and
  input code were not modified.

Downloaded images, metadata caches, contact sheets and diagnostic logs remain
local. The importer, reviewed exclusions, metadata overlays, documentation and
rebuilt static preview are versioned.
