# Public source indexing audit — 2026-10-19

Obsidian indexes **metadata and public source-page links only**. An indexed
entry is not a guarantee that a game file can be downloaded, is legitimate, or
can be installed. One title may have multiple platform or region records.

## Complete publicly enumerable indexes

| Source | Indexed compatible entries | Evidence and exclusions | Runtime |
| --- | ---: | --- | --- |
| Axekin | 20,073 | 840/840 Inertia pages; 920 games lack supported console platforms | Native + web |
| GFS Catalog | 685 | All four pack API totals reconciled | Native + web |
| RomHeaven | 141 | All 141 CSV game rows | Native + web |
| SuperPSX | 3,892 | 4,253/4,253 public WordPress posts across 43 pages; excluded unsupported and non-game categories | Web full |
| DownloadGamePSP | 1,972 | 2,978/2,978 posts across 30 pages; PSP category only (Vita excluded) | Web full |
| Ziperto | 17,004 | 18,318/18,318 posts across 184 pages; Switch, 3DS, Wii U only | Web full |
| NoPayStation | 11,932 | All PS3/PSP/PS1/Vita TSV rows inspected; Vita excluded; identical content IDs de-duplicated | Web full |
| PSN-Content | 5,459 | All 5,515 PS3 feed rows; duplicate content IDs removed | Web full |
| Archive.org / AlvRo | 63 public files | Eight explicitly configured collections verified; 3,997 additional game files marked private by Archive.org and excluded | Web full public-only |
| Archive.org / maelly_pooh | 131 | All 30 indexed public uploader collections checked | Web full |
| RomsTime | 24,097 | All 26 sitemap files, 24,098 ROM page URLs; one Vita URL excluded | Web full |

**Web full** indexes load as separate JS data chunks in the React static preview.
The native Linux/PS5 app still uses the bounded initial snapshot of these
sources. Its existing `MAX_CATALOG_ITEMS=26000` fixed `GameItem` array
takes ~311 MB in `ObsidianApp` already. Loading tens of thousands more items
there requires a disk-backed/compact index or paged native search rather than
raising the fixed limit without evaluating PS5 memory safety.

## Incomplete — follow-up adapters required

| Source | Currently native snapshot | Public discovery | Next work |
| --- | ---: | --- | --- |
| Vimm's Lair | 62 | Alphabetical per-console vault indexes (A-Z, 30+ sections) | Crawl all letters for supported consoles, throttle requests, verify skipped/removed pages |
| PKGPS4 | 64 | Public home listings; conventional sitemap and WP API not found | Discover supported paginated index without inventing URLs |
| WowRoms | 3 | Public HTML; conventional sitemap not exposed | Find verified console listing routes and pagination |
| Retro Game Champion | 20 system collections | Wix sitemap lists 61 system pages, no verified game-by-game feed | Find game-level public listing; collections must not be presented as individual games |
| ROMsHQ | 64 | Public sitemap includes game-detail URLs but not console metadata | Determine reliable platform field from public game-detail metadata; exclude emulator pages |
| CDRomance | 25 | Site displays ~801 pages; WP public posts API returns news only | Locate a public game-specific feed or responsibly paginate its game archive |

The audit does not claim these six sources are complete. It is more accurate
to display an incomplete status than to silently cap or fabricate entries.

## Refresh the verified indexes

Run from the repository root:

```bash
python scripts/import_sources.py --limit 64 --full-small
python scripts/import_wordpress_sources.py --workers 3
python scripts/import_tsv_sources.py
python scripts/import_archive_sources.py
python scripts/import_romstime.py
# Axekin is large and handled separately:
python scripts/import_sources.py --limit 64 --full-axekin
npm run build:static
```

The full structured GFS and RomHeaven rows live inside
`assets/catalogs/additional.json`; other full indexed website sources have
individual `assets/catalogs/<source-id>.json` snapshots. Never run a
bounded refresh expecting it to cover all of a large website. All full index
scripts fail without replacing their previous snapshot when an API's counts
or pagination change unexpectedly.

## Archive.org / AlvRo: eight verified collections (22 October 2026)

AlvRo's source configuration now lists the exact eight public Archive.org collection identifiers, rather than relying on a `creator` search to discover them. The public metadata API reports these game-archive file counts; a `private` file is intentionally **not** represented as an available game entry. All collection URLs below point to directory listings, not game binaries:

| Collection directory | Public game files | Private game files |
| --- | ---: | ---: |
| https://archive.org/download/httpsarchive.orgdetailsps2japanredump4 | 0 | 835 |
| https://archive.org/download/ps2japanredump2 | 0 | 707 |
| https://archive.org/download/ps2japanredump3 | 0 | 504 |
| https://archive.org/download/ps2usaredump1_20200816_1458 | 0 | 565 |
| https://archive.org/download/ps2japanredump5 | 0 | 157 |
| https://archive.org/download/ps2japanredumpmissing | 63 | 0 |
| https://archive.org/download/ps2japanredump1 | 0 | 523 |
| https://archive.org/download/ps2usaredump1 | 0 | 706 |
| **Total** | **63** | **3,997** |

This is a live *metadata* inspection, not a verification that files are downloadable or usable. Each listed collection has `creator` AlvRo/Alvro; 63 public PS2 files currently come from the Missing Games collection. `assets/catalogs/alvro.json` retains all eight directory links and their counts in `coverage.collections`, while both the bounded and full game snapshots remain the same 63 verified public records. No private game files or download URLs were added to the catalogue.
