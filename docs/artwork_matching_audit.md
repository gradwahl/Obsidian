# Remaining artwork audit — 20 October 2026

This audit used the local source catalogues, the user's completed IGDB bulk
catalogue caches, and cached Libretro indexes. No new API requests or image
downloads were made. Counts describe metadata links, not visually verified images.

## Applied improvements

Recognised whole distribution suffixes on their actual console, preserving
unknown tokens, editions, translations, UNDUBs and DLC labels. Also recognised
parenthesised PSP/PS3 product IDs and moved trailing English articles to the
front for comparison without dropping words.

| Console | Additional unique title matches |
|---|---:|
| Nintendo 3DS | 1,223 |
| Nintendo Wii U | 271 |
| Super Nintendo | 3 |
| PlayStation 3 | 2 |
| PSP | 2 |
| Total | 1,501 |

Examples include `Coaster Creator 3D 3DS (EUR/USA) CIA`, `Plantera WII U ISO
(Loadiine) (USA)`, `Addams Family, The`, and `After Burner: Black Falcon
(ULES-00753)`. Every match still requires the correct IGDB platform, an exact
cleaned name and one distinct cover image across all matching candidates.

The existing importer was run directly against complete caches. Legacy numeric
cache files were associated with consoles only where all earlier API-verified
successful game IDs for that console identified exactly one complete catalogue.
Two consoles without such anchors (Atari 7800 and Sega CD) were skipped in this
offline pass; no platform IDs were guessed. The public `--bulk` command continues
to resolve console identities through IGDB's platform endpoint.

## Coverage after merging

| Entries | Before | After |
|---|---:|---:|
| Direct artwork | 20,393 | 20,393 |
| Matched artwork | 39,424 | 40,923 |
| Missing | 24,621 | 23,122 |
| Awaiting review | 1,170 | 1,170 |

The 1,501 new title matches add 1,499 matched entries after enrichment's
provider priorities and review rules. All earlier manifest matches and IGDB
exported matches were checked for preservation, including their URLs, providers
and confidence. Artwork coverage is now 61,316 of 85,608 entries (71.6%).

## Remaining evidence and limitations

The importer still lists 19,076 unique uncovered supported-console titles,
including entries awaiting review. This differs from the missing-entry count
because several sources can list the same game, and review entries are not
exported as approved matches.

| Console | Unique uncovered titles, including review |
|---|---:|
| PlayStation 3 | 4,009 |
| Nintendo Switch | 3,680 |
| PSP | 2,081 |
| Nintendo 3DS | 1,816 |
| Nintendo DS | 1,198 |
| NES | 1,135 |
| Game Boy Advance | 920 |

Observed patterns, which overlap and are diagnostic labels rather than automatic
exclusion rules:

- 5,064 titles contain distribution markers such as CIA, Loadiine or NSP.
  Some still include DLC or other identity-bearing text; these were preserved.
- 579 contain demo/trial labels and 166 contain modification labels such as
  randomizer, hack or UNDUB. Base-game covers were not substituted for them.
- Nine unique titles exactly match sampled navigation/junk patterns such as
  `comments 20`, `Download Now`, `PSP Themes`, `disc2` or `game`.
- Source titles sometimes concatenate alternate regional titles, for example
  `Endless Ocean 2: Adventures of the DeepEndless Ocean: Blue WorldNA`.
- Regional names and abbreviations may differ from IGDB's canonical names,
  for example `Monster Hunter 3G` and `Lego Lord Of The Rings`. These need
  authoritative alias data or explicit review, rather than substring matching.
- Libretro's Switch index is unavailable. IGDB supplies Switch matches; its
  successful bulk fetch does not imply that Libretro has a corresponding index.
- A missing exact name does not prove that a provider lacks the game. Cached
  IGDB pages contain canonical names, not the `alternative_names` field.

The next useful work identified by this audit was source-specific cleanup of
concatenated/navigation titles and an IGDB alternative-name fetch with ambiguity
checks. The latter is now implemented as `--bulk --aliases`, using a separate
resumable cache and exact same-console comparisons. Live credentials are not
available to the agent process, so the fetch has not run and the counts above
remain unchanged. Increasing the title-search batch limit does not resolve
these naming problems. No fuzzy matches were applied.

## Validation

- IGDB tests, including the alias extension: 38 passed on native Windows
  Python and Linux Python under WSL.
- Artwork enrichment/download tests: 22 passed on each of those environments
  (this suite overlaps the IGDB suite).
- Existing approved manifest and IGDB export entries preserved byte-for-byte
  at the value level; offline enrichment completed successfully.
- Windows web production and static-preview builds passed, with the existing
  large-chunk warning. Source/SSR web tests passed.
- A Linux web build could not run because the WSL environment has no Linux
  `node` executable. Its available npm launcher points to Windows tooling.
- Native desktop builds/launches, PS5 builds and live graphical verification
  were not performed. No browser or desktop UI automation was used.
