# Obsidian additional source catalogues

Obsidian stores public **game metadata only**: titles, platform, optional
version/cover, and a source-page URL. None of the indexes contain games,
license keys, direct package-download links or credentials.

## Refreshed public indexes (2026-10-20)

| Source | Entries | Coverage |
| --- | ---: | --- |
| Axekin | 20,091 | Full supported-console paginated games index |
| RomsTime | 24,097 | Full sitemap enumeration |
| Ziperto | 17,010 | Full supported-platform WordPress index |
| NoPayStation | 11,932 | Full supported TSV feeds |
| PSN-Content | 5,459 | Full PS3 TSV |
| SuperPSX | 3,898 | Full supported-platform WordPress index |
| DownloadGamePSP | 1,972 | Full PSP WordPress index |
| GFS Catalog | 686 | All published structured pack pages |
| RomHeaven | 141 | Full published Switch CSV |
| Archive.org / maelly_pooh | 901 | 131 public plus 770 restricted file listings; browse/download only through authorised Archive.org browser |
| Archive.org / AlvRo | 63 | Public files only |
| PKGPS4 | 64 | Partial public HTML |
| Vimm's Lair | 62 | Partial public index |
| ROMsHQ | 46 | Partial public index |
| Retro Game Champion | 20 | System collections, **not** verified individual games |
| CDRomance | 15 | Partial public HTML |
| WowRoms | 3 | Partial public HTML |

All 17 source adapters are represented. **The first 11 have verified full
coverage of the supported published feed**, while the last six do not. The
Retro Game Champion collections are not displayed as individual games.

There are **85,690 source records including 20 non-game system collections**;
the web app adds the real game entries to its original catalogue at startup.
Enabled-source preferences filter the loaded entries, rather than pretending
that enabling a source creates its index.

### Web

`src/sourceCatalogues.ts` uses Vite's lazy imports of
`assets/catalogs/*.json`. Vite builds a separate JS chunk per full snapshot.
The bounded `additional.json` is used for sites without full source files.
The Refresh action reloads those **built-in** snapshots, not live websites.
To include newer data, run the importer(s) below and rebuild/deploy the app.

Large Games/Roms carousels render a small cover window around the selected
game and maintain the total scroll width, in both Netflix and (u)biquity.

### Native PS5 SDL2

`scripts/generate_native_sources.py` creates
`src/native_source_index.h` containing a **bounded snapshot of 1,858
metadata-only entries**. Native startup imports this after the initial
Pegasus/Pippo snapshot and can still perform its existing online Pegasus
refresh. The fixed `MAX_CATALOG_ITEMS` and per-row memory limits are not
increased. Native does **not** expose the full 85k records yet; a compact
paged/on-demand native index is needed to do that safely.

### Refreshing indexes

Run from the repository root with network access:

```sh
python scripts/import_sources.py --limit 64 --full-small
python scripts/import_wordpress_sources.py --workers 3
python scripts/import_tsv_sources.py
python scripts/import_archive_sources.py
python scripts/import_romstime.py
python scripts/import_sources.py --limit 64 --full-axekin
python scripts/generate_native_sources.py
npm run build:static
python -m unittest discover -s tests -p test_catalogue_indexes.py
```

Importers reject unexpected/incomplete pagination and keep metadata only.
Never replace a verified full source index with a bounded sample without
explicitly updating its status. The source websites may change their APIs.

Art assets are separate: `Obsidian-archive` contains artwork, not source
catalogues, and anonymous access to that archive must be enabled before
remote cover streaming works on PS5.
