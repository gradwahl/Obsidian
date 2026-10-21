# Streaming artwork from PolDrive-archive

The now-public PolDrive-archive repository serves PolDrive game covers, company
logos, console artwork and emulator logos **on demand**. No binary artwork or
GitHub credentials are embedded in the app.

## Verified public access

Anonymous HTTP requests and a headless Chrome decode test succeeded for:
- assets/library/companies/sony.png (company logo)
- assets/library/consoles/sony-ps5.png (console artwork)
- assets/library/logos/sony-ps5.png (console/emulator mark)
- assets/covers/Sony/PlayStation%205/63%20Days.art (JPEG cover)

Archive paths are served at:
https://raw.githubusercontent.com/PolDrive/PolDrive-archive/main/

The archive contains 9 company images, 53 console images and 56 console and
emulator logos. The Games/Roms and Emulators screens in React and SDL2 now use
these URLs instead of looking for local library image files.

## Game covers

- The original 711 verified bundled PS4/PS5 matches remain in
  src/archiveArtworkUrls.json and src/archive_artwork_index.h.
- The new src/archiveSourceCovers.json maps **48,906** additional unambiguous
  same-console, same-title matches from imported source catalogues.
- This file is metadata only (6.1 MB uncompressed; about 1 MB gzipped as a
  separate JS chunk). Paths are resolved to HTTPS URLs only when needed.
- React imports the metadata chunk asynchronously and displays covers as
  required, keeping source cover URLs as fallbacks.
- Native SDL2 compiles archive cover URLs only for its bounded catalogue
  snapshot; the huge source match index is not embedded in the PS5 binary.
- Ambiguous and unmatched covers retain their source providers or placeholders.

## Storage and fallbacks

- Native SDL2 streams images through its background download worker directly
  into memory. Each compressed transfer is capped at 16 MiB; decoded textures
  use the 48 MiB LRU budget shared with game covers.
- Remote company, emulator, and console art does NOT go into a persistent disk
  cache. Local user artwork overrides remain supported.
- React library images use lazy loading and decoding. Browsers may still use
  their own normal HTTP cache; PolDrive never fetches the entire artwork repo.
- Previously created cache files are not automatically removed. Old files under
  /data/homebrew/wolodrive/covers can be cleared after verifying they are
  unneeded. Do not remove custom /data/homebrew/wolodrive/artwork overrides.
- If the archive is offline, existing cover-provider URLs or placeholders are
  used; library tiles fall back to a text label.

## Regenerate metadata after archive changes

From the PolDrive repository root, run in this order:

    python scripts/generate_archive_artwork.py
    python scripts/generate_source_cover_matches.py
    python scripts/generate_native_sources.py
    npm run test:web
    npm run build:static

The generators perform a shallow partial Git clone *without checkout*,
inspecting filenames only. They never download the 50k+ image blobs.

The public Raw origin is defined in src/archiveAssets.ts (React) and
src/ui_netflix.c (native). No GitHub token is needed.

Windows Chrome browser delivery and native Linux/SDL2 compilation have been
verified. Actual PS5 network/TLS behaviour still requires hardware testing.
The game catalogue indexes themselves remain separate from the artwork
archive; see docs/source-catalogues.md.
