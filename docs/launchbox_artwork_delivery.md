# LaunchBox cover delivery — 20 October 2026

The public LaunchBox Metadata.zip was downloaded once and streamed locally.
Canonical and official alternate names were compared against unresolved source
entries on their exact console. No credentials, fuzzy matches, game downloads,
or browser/desktop automation were used.

| Catalogue entries | Before | After |
|---|---:|---:|
| Total | 85,602 | 85,602 |
| Direct artwork | 20,393 | 20,393 |
| Matched artwork | 44,225 | 47,945 |
| Missing | 19,814 | 16,094 |
| Awaiting review | 1,170 | 1,170 |

Added approved artwork to **3,720 entries**, using **3,252 distinct front covers**.
All 3,252 images downloaded successfully into `assets/covers` (about 1.63 GiB).
Metadata coverage is **68,338 / 85,602 (79.8%)**. All 62,005 existing manifest
rows, including URLs, providers, confidence and independent fallbacks, remained
unchanged. The native overlay gained 57 rows; full-source entries also use the
shared JSON overlay in the web preview.

The importer exported 4,807 candidate entry matches. Enrichment accepted 3,720
previously missing entries and withheld the other 1,087 because their earlier
source/Libretro matching was already ambiguous. None of the 1,170 existing
review entries was automatically approved. Multiple distinct LaunchBox games
with the same name/platform are also withheld, even if only one has a cover.
Only Box - Front images are accepted. Known regional tags require a matching
region; untagged titles use the documented deterministic region preference.

The remaining entries need other providers, authoritative title corrections,
or manual review. This run did not add an EmuMovies integration or refresh
Libretro indexes.

## Reproduction and verification

Run `python scripts/launchbox_artwork.py --offline --download` on Windows,
or use `python3` on Linux, after the initial metadata archive download.
Omit `--offline` for the initial run, or use `--refresh` to replace the archive.
The shared enrichment step retains the separate LaunchBox export on later runs.
The metadata export, overlays and coverage report were byte-identical after an
offline rerun.

- Seven new LaunchBox tests passed on native Windows Python and Linux Python
  under WSL, covering platform boundaries, aliases, editions, non-Latin titles,
  game collisions, regions, front-cover filtering, unsafe filenames, merge
  priority and failed archive refresh recovery.
- Existing enrichment (six), IGDB (40) and download (seven) tests passed on
  both Python environments.
- Windows web production/static builds, web tests and static HTTP delivery
  checks passed. Vite retains its existing large-chunk warning.
- Linux native build and the full `make test` suite passed under WSL.
- A saved twelve-cover contact sheet was visually inspected. This sample
  includes regional aliases; it does not verify every downloaded title match.
- All 3,252 downloaded images decoded. One 800-by-800 PNG exceeded Pillow's
  default 1 MiB text-metadata limit; it decoded after a bounded 8 MiB diagnostic
  allowance and also loaded through Linux SDL_image with its normal settings.
  The original image was retained without modification.
- Native Windows application build/launch, PS5 builds, and manual Windows,
  native Linux and WSL graphical checks were not performed. This change touches
  offline tooling and artwork metadata, not native rendering or input code.

Downloaded images and local validation reports stay in `assets/covers` and
`.cache/wolodrive-artwork`; they are not added to the Git artwork bundle.
The committed importer and metadata make the downloads reproducible.
