# Source cleanup, GameTDB and artwork review — 20 October 2026

## Results

| Catalogue entries | Before | After |
|---|---:|---:|
| Total | 85,602 | 85,601 |
| Direct artwork | 20,393 | 20,393 |
| Matched artwork | 47,945 | 48,039 |
| Missing | 16,094 | 15,999 |
| Awaiting review | 1,170 | 1,170 |

Added 94 approved entry matches: 93 from GameTDB and one recovered by the
reviewed Joust title correction. Removed one confirmed navigation entry.
All 65,725 earlier manifest rows remained unchanged, including their providers,
URLs, confidence and independent fallbacks. Metadata coverage is now
68,432 / 85,601 (79.9%).

GameTDB metadata yielded 215 candidate entries. The saved export contains
96 entries backed by 94 distinct downloaded and decoded images. Enrichment
approved 93 previously missing entries; the other three remain in the review
queue with their additional GameTDB evidence. The export never credits a URL
that did not return an image. A saved twelve-cover sample was visually inspected;
this is not a visual verification of every title/image pair.

The GameTDB image host became unavailable during the run: a known cover that
had previously downloaded also timed out. Outstanding requests were stopped,
valid downloaded files were preserved, and offline recovery exported the
successful results. Remaining candidate entries were not credited. The importer
now stops scheduling work after three consecutive timeout/access-limit results
and retains completed checkpoints. HTTP 404 responses do not trigger the
provider-outage stop condition. Retry `python scripts/gametdb_artwork.py` when
the provider is available; `--offline` never makes network requests.

## Source evidence and preservation

- Added public product ID/region metadata to 11,927 NoPayStation and 5,453
  PSN-Content records using exact existing hashed identities, console and title.
  All original fields, row order and memberships in those snapshots were
  preserved. Contradictory title IDs/content identities and conflicting records
  are not used. No package URLs, RAP/license values or binary hashes were copied.
- Future TSV imports retain the same public metadata. The restoration command
  enriches existing snapshots without replacing titles or adding new releases.
- Corrected Axekin's exact `Joust[broken anchor]` PS3 record to `Joust`, preserving
  its ID and URL. The same-platform cached IGDB identity supplies the new cover.
  The correction is scoped by original title, source, console and source URL.
- Removed ROMsHQ's `Download Now` row for the Grand Theft Auto V source page.
  The heading-link importer now rejects the same navigation labels already
  rejected by its fallback link parser. The other four questionable/repeated
  ROMsHQ titles remain unchanged: there is insufficient authoritative evidence
  to turn them into verified Switch releases.
- The GameTDB pass uses exact product IDs on the same console. Without an ID,
  only a unique exact localised title is accepted. Identity-bearing demo, trial,
  DLC, bundle, edition and modification labels prevent base-game substitution.

## Review artifacts

`assets/artwork/review-queue.json` records every withheld identity, source URL,
reason and candidate image URL. The local report is:

`.cache/wolodrive-artwork/review/index.html`

It contains all 1,170 entries and links to 320 numbered contact sheets containing
5,105 candidate tiles. The candidate-download pass saved 3,508 additional images
and reused 779 existing images. All 92 download failures were Axekin routes
blocked by their known bot-check response, producing 96 unavailable tiles.
One additional Libretro PNG candidate could not decode, including after a fresh
diagnostic download, giving 97 unavailable tiles in total. These candidates
remain explicitly marked in both the sheets and HTML. No access challenge was
bypassed. Three already-cached
GameTDB candidates were added to the final report after enrichment finished.

The report does not approve artwork. Every image is loaded locally. Missing
images remain labelled, and the HTML retains complete source text and URLs.
Japanese labels use a Unicode font; caption wrapping measures rendered width
so full-width glyphs cannot spill into neighbouring columns. Windows sheets
and a Linux/WSL sample with an explicitly supplied Yu Gothic font were rendered
and visually inspected. No browser or desktop UI automation was used.

## Local availability audit

Separately from metadata coverage, 65,070 approved entries have at least one
local image with a valid supported-format signature; 3,362 approved entries
still lack such a file. This delivery gap is additional to the 15,999 missing
matches and 1,170 withheld entries. Signature checks do not establish complete
image decoding, correct visual identity or successful live UI rendering.
The full local audit is `.cache/wolodrive-artwork/availability-audit.json`.
All 94 new GameTDB images did pass full Pillow decoding.

## Verification and limits

- Native Windows and Linux/WSL Python: GameTDB/source-metadata tests (six),
  review-report tests (two), enrichment tests (six), LaunchBox tests (seven),
  source-import tests (12), full-source tests (two), IGDB tests (40), and
  downloader tests (seven) passed.
- An offline rerun preserved the GameTDB export, JSON/TSV overlays, review queue
  and coverage report byte-for-byte. Original source fields and catalogue
  membership were checked independently against Git.
- Windows web build, web tests, static preview build and static HTTP-delivery
  check passed. The existing Vite large-chunk warning remains.
- Linux native build and the full make-test suite passed under WSL. Focused
  tests were rerun after the outage/caption fixes.
- Saved reports rendered using Pillow on Windows and Linux/WSL. Native Windows
  app build/launch, PS5 build and manual Windows/native-Linux/WSL graphical
  checks remain unverified. Native rendering/input code was not changed.
- EmuMovies/ScreenScraper integration and full manual resolution of all review
  entries are not included in this pass. The provider outage prevents claiming
  a complete GameTDB image sweep.

Downloaded covers, comparison sheets, virtual environments and diagnostic
reports remain local; the importer, public metadata, review queue, documentation
and rebuilt static preview are versioned.
