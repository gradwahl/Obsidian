# PS1 cover delivery fix — 20 October 2026

The supplied screenshot showed catalogue entries with placeholders, including
Axekin's `Triple Play 2001` (`EXT-5fe80be2c82547bd45cc`). All seven visible games
already had Axekin cover URLs. They were counted as supplied artwork and were
therefore excluded from the missing-title search and independent fallback merge.

Command-line requests for those seven URLs returned HTTP 200 with
`text/html; charset=utf-8`, and the response title was
`Making sure you're not a bot!`. A URL being present was not evidence that an
image would render. No challenge bypass was attempted.

## Result

- Added independent provider fallbacks for 1,209 PS1 entries with supplied
  covers and 571 PS1 entries inheriting source covers through same-title matching.
- 2,603 of the 3,559 PS1 entries now have an independent IGDB/Libretro artwork
  path, including existing independent primary matches. This does not establish
  live availability of every image; other titles can still lack safe matches.
- All seven screenshot examples received a fallback, and all seven alternative
  image responses downloaded and decoded successfully: six IGDB JPEGs and the
  Libretro PNG for `Trickshot`. A saved contact sheet was visually inspected;
  it shows the correct titles' artwork.
- The complete catalogue has 30,065 independent backup paths, including direct
  and inherited source-art rows. Existing primary URLs, providers and confidence
  were preserved. The original metadata coverage totals are unchanged.

## Implementation

`--bulk --aliases --fallbacks` also considers entries that have supplied art.
This pass used complete local IGDB alias catalogues, without new API requests.
Enrichment uses an already unambiguous IGDB identity first for backups, then
unique exact/normalised Libretro artwork. It never echoes the source image as
its own independent backup or guesses between conflicting regional covers.

Supplied art remains primary. The manifest supplies independent alternatives;
React cards, hero and details advance through candidates after image errors.
The native loader attaches the same alternatives from the TSV and the SDL
renderer advances past failed loads/decodes, including more than one failure.
The source URL, source record ID, user local artwork and console identity are
preserved. Additional backup paths do not increase the metadata coverage count.

## Verification

The native decoder fixture returns HTML for two primary candidates and a real
image for the third. The test confirms the independent image is rendered into
an SDL texture. Source/SSR tests exercise the web selection path, and regression
checks cover supplied-versus-inherited alternatives, ambiguity and the real
Triple Play 2001 native catalogue record. Seven live independent images were
decoded and the saved contact sheet inspected.

The Linux native build and full `make test` suite passed under WSL, including
the SDL decoder and real catalogue regressions. Windows Python tests,
source/SSR web tests and the Windows static-preview production build passed.
The existing large-chunk warning remains.

Live browser, Windows, native Linux and WSLg graphical checks remain unverified;
repository instructions prohibit UI automation. Automated/headless tests and
saved images do not establish that the running graphical interface works.
Native Windows build/launch is blocked because no Windows C compiler is
available in the configured shell;
the Linux web build lacks a Linux Node runtime. PS5 compilation requires the
unavailable PS5 SDK. The WSL renderer default is unchanged.
