# Source-title cleanup — 20 October 2026

This pass used the user's completed IGDB canonical/alias caches and local
Libretro indexes. No new network requests or image downloads were made.
Counts represent artwork links, not visually inspected images.

## Result

| Catalogue entries | Before | After |
|---|---:|---:|
| Total | 85,608 | 85,602 |
| Direct artwork | 20,393 | 20,393 |
| Matched artwork | 42,635 | 44,225 |
| Missing | 21,410 | 19,814 |
| Awaiting review | 1,170 | 1,170 |

Added artwork to **1,590 entries**; removed **six comment-navigation entries**
that were not games. Coverage is now **64,618 / 85,602 (75.5%)**. The reduction
in missing entries includes those six removals. All earlier approved manifest
matches and IGDB export matches retained their URLs, providers and confidence.

## Changes and evidence

- Switch distribution suffixes now handle parenthesised NSP/XCI formats,
  combined region lists (`[US/JP]`, `(ASIA/JAPAN)`), RF tags and trailing bare
  eShop labels when preceded by a Switch packaging marker.
- 3DS suffixes now recognise `(Gateway/Sky3DS+)` and `ROM & 3DS` packaging.
- PS3 suffixes now recognise `full game`, `Downloadable Game` and bracketed
  region/language lists. PSP suffixes recognise the `(Minis)` service label.
- DLC, demos, trials, translations, hacks, bundles and named editions remain
  distinct. Exact canonical/official-alias matching on the actual console and
  existing collision checks still determine whether artwork is accepted.
- The HTML fallback importer previously concatenated title attributes, image
  alt text and visible link text. It now selects one title in that order:
  title attribute, visible text, image alt text. It rejects query/fragment
  navigation links and comment/download navigation labels.
- Two reviewed Axekin source corrections are stored in
  `config/source_title_corrections.json`: `Marble Saga: KororinpaMarbles!
  Balance ChallengePAL` becomes `Marble Saga: Kororinpa`; `My Word CoachMon
  Coach Personnel: J'enrichis Mon VocabulaireFR` becomes `My Word Coach`.
  In both cases, the two concatenated parts exactly correspond to canonical
  and official alternative names of one cached IGDB game. The correction
  requires the exact source, console, page URL and original title.
- The two existing source records kept their IDs and other fields. Future
  imports apply the same corrections after computing any generated ID from
  the original title. Six confirmed CDRomance `?xf-comments=1` records were
  removed from the local bounded snapshot.

The IGDB pass found **1,424 new unique title matches**: PS3 509, Switch 470,
3DS 243, PSP 197, PS1 four, Wii one. A title may cover several source entries;
the corrected `My Word Coach` record also gained same-console artwork through
the normal enrichment priorities.

## Remaining gaps

19,814 entries remain missing. Unproven concatenated names, modified releases,
unidentified source content, non-Latin normalisation limits and unavailable
provider artwork remain unresolved. For example, the concatenated Endless Ocean
title was left unchanged because both parts could not be established as exact
official names of the same game in the cached data. No substring or fuzzy
title matches were approved.

## Verification

IGDB tests (39), source-import tests (12), full-snapshot tests (two), and artwork
enrichment/download tests (23, overlapping the IGDB suite) passed on native
Windows and Linux under WSL. The existing-match preservation check passed;
source changes were limited to the two titles and six removals. The Windows
static-preview build and source/SSR checks passed, with the existing large-chunk
build warning. Linux web builds require a Linux Node runtime, which is unavailable
in this WSL environment. Native desktop and PS5 builds/launches and live graphical
verification remain unverified; no UI automation was used.
