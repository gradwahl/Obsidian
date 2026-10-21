# Text-only catalog — 22 October 2026

Game browsing now uses ten text rows per page on Home and console game lists. Search uses nine text rows per page. Rows show Console, Name, Format and Size in separate columns, with matching headings. Selected rows retain the crimson highlight. The main browsing screens, platform chooser, emulator release page, downloads and active game details no longer mount cover/background artwork. Screenshots are removed from the active details options. Existing artwork files and catalog metadata remain available to the build tooling; this change does not delete the asset library or change catalog inputs.

Home: Up/Down moves through games, Right switches between Newest Releases and Most Popular, Left returns to the rail, Enter/Cross opens details. Existing refresh and language controls remain. Console catalogs retain native batch paging beyond 128 records. No backend action is replaced with simulated success.

## Automated verification

- `npm run lint`: passed.
- `npm run test:web`: passed, including text-list rendering, large/empty/final pages, escaping, localization, unchanged category selection and mirrored native geometry.
- `npm run build:static`: passed; current compiled JS/CSS and root index included. Existing static output paths and build scripts are unchanged.
- `python tests/check_static_preview.py`: passed over plain Python HTTP, including HTML/JS/CSS, 85,690 catalog records in 472 JSON pages and all 16 versioned asset files.
- WSL Ubuntu 24.04 `make build-linux`, `make test-native-snapshot`, `make test-game-list`: passed. The new native test exercises console labels, known/unknown formats, unit-aware sizes and native live-size parsing, actual keyboard/controller/mouse events, ten-row page boundaries, gap hit testing and artwork-free rendering of Home, details, Search and the platform chooser.
- `make test`: **not passed**. It stops at the assertion in unchanged `tests/test_glass_ui.c:35` (`glass_cache[GLASS_PANEL*2+3].texture`). Continuing diagnostically with `make test -o test-glass` reaches existing compile errors in `tests/test_regressions.c`, including references to `DESCRIPTION_BUTTON`, `load_library_artwork` and `library_alpha_bounds` that are also absent from the original renderer. Those unrelated tests have not been repaired or declared passing.

## Visual and runtime verification

The rebuilt preview was served at the URL root using `python -m http.server 3011 --bind 127.0.0.1 --directory static-preview`, without Vite. Browser checks covered Home startup, keyboard selection across the ten-row boundary, category switching, game details, Search, platform/console navigation, and Settings/source options. The Home DOM contained ten game rows and zero image elements. No browser console errors were observed.

A native SDL software-renderer capture is generated with `OBSIDIAN_LIST_SCREENSHOT=build/game-list-native.png make test-game-list` in WSL. It starts on the English Newest Releases list, matching the browser default for the same catalog input. Native and browser list content and styling were visually inspected; automated tests check row geometry against the same 1920×1080 logical layout.

For the size/Format update, equivalent 16:9 Most Popular views were visually compared: browser 1280×720 and native SDL software rendering 1920×1080, English, first row selected. Both show the same ten games, formats and corrected sizes (Unknown 9 58 GB, NAIAD 1 GB, Lies of P 72 GB), with matching column placement. Captures are `build/corrected-sizes-formats.png` and `build/size-popular-native.png`. Browser Search/details also show Yatzi as 699 MB, matching its source description. No browser console errors were observed.

This is not a claim of whole-app or PS5 pixel parity. Font-engine differences and existing navigation-rail/footer geometry and keyboard-focus-outline differences remain; detail-option composition also differs. The browser streams the larger bundled source catalog, while native starts from its snapshot and supports its existing live sources, so complete catalog populations are not identical.

**AI Studio deployment not verified.** No direct AI Studio session or PS5 device was available. Local static checks and the native Linux build do not prove either deployment or PS5 runtime behavior.

## Remaining manual validation

1. Serve the rebuilt `static-preview/` at the HTTP root and open it at 1920×1080. Compare the initial English Newest Releases list against the native capture with the same catalog/filter selection. Check the first, second and final pages, focus borders, long titles, categories and detail options.
2. On PS5, check D-pad, Cross/Circle, language, refresh, console-list batch boundaries and truthful backend action results. Compare Search, platform navigation, dialogs and Settings with the browser.
3. Deploy the generated directory to AI Studio. Confirm root assets/catalog pages load, Home and the sliding sidebar work, Search and categories navigate, dialogs and Settings respond, and the browser console has no fatal errors. The UI should make no decorative artwork requests.
4. Repair the independent native test-suite blockers before calling the complete native suite green.

## Home tab simplification

Only **Newest Releases** followed by **Most Popular** is exposed in Home. Newest Releases is the initial selection. PS5/PS4/Top Rated are no longer Home tabs; platform-specific catalogs remain accessible through Games/Roms. Existing catalog ordering/filtering logic is reused. Native and browser tests cover the two-tab order, default, and controller/mouse switching.

## Game list metadata columns and corrected sizes

Home, Games/Roms game lists and Search share **Console | Name | Format | Size**. Console labels reuse library mappings and PS4/PS5 title-ID prefixes. Formats are the known EXFAT, FFPKG or PKG package format; metadata-only entries display `—` rather than claiming a downloadable format. Folder names are no longer displayed. Header labels are aligned in all nine languages; column weights remain 17/39/32/12. Narrow browser screens preserve separate columns with horizontal scrolling below 720px.

All 862 offline seed sizes were audited. The 852 Pippo entries now take their values from their explicit `Size:` description lines, respecting GB/MB units and ignoring separate `BackPort Size:` component lines. The ten original demonstration entries have no package-size evidence and now show unknown rather than retaining example numbers. `python scripts/repair_catalog_sizes.py --check` validates this contract; running without `--check` repairs regenerated seed data. Native seed headers were regenerated.

Source byte counts are converted using decimal GB (1 GB = 1,000,000,000 bytes) in both browser loading paths, native live parsing and generated native source metadata. The old native 42.5 GB fallback is removed. Native live catalogs accept positive `sizeBytes`, `sizeGB` or `size_gb`; the ambiguous unitless `size` field is now unavailable rather than guessed. Missing/invalid sizes display `—`. Positive small sizes display MB, KB or B, so they do not round down to 0.0 GB. Details and package choices use the same size formatter. These are source-reported sizes, not measurements of downloaded files or verified installed storage.

Regression checks cover all 852 description-backed sizes; Unknown 9 = 58 GB, NAIAD = 1 GB and Lies of P: Overture = 72 GB; formats replacing folders; decimal byte conversion and source-page hydration; small files; and missing/ambiguous native live sizes. Python size-repair tests run as part of `make test-game-list`.

No static-hosting paths, runtime dependencies or deployment workflow were changed. To roll back this size correction, revert the seed/size converters together, regenerate both native headers and rebuild `static-preview/`; do not restore only an older index or bundle.
