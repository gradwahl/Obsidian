# Streaming artwork from PolDrive-archive

PolDrive streams verified artwork from `PolDrive/PolDrive-archive` on demand. The
runtime never clones the archive or downloads its contents in bulk.

## Before enabling public image delivery

At the time of implementation, Git can read the archive **using the configured
Windows credentials**, but unauthenticated HTTPS requests to
`https://raw.githubusercontent.com/PolDrive/PolDrive-archive/main/README.md`
return **404**. A PS5 does not have your Windows Git credentials. Therefore,
archive covers **will not load remotely until they are served anonymously**.

Either publish an appropriately licensed/authorized artwork repository, or
serve only the artwork through a public CDN/mirror that preserves the same
`assets/covers/<company>/<console>/<filename>.art` paths. Do not put a GitHub
PAT or private-repository credentials in the PS5 application. If you use a
different public origin, regenerate the JSON/header mapping with that origin.

## What is shipped to PS5

- `src/archiveArtworkUrls.json` and `src/archive_artwork_index.h` hold a
  **small filename/URL map** generated from the Git tree. They contain no images
  or authentication material.
- At the time of generation, **711/862 current catalog records** had unique
  same-console, same-title matches; 142 were ambiguous and nine absent. No
  potentially incorrect matches are guessed.
- The existing source cover URLs remain available as fallback if the archive
  URL does not load. Custom local artwork overrides take priority.
- The React UI uses normal `<img>` requests, including lazy loading in the
  (u)biquity carousel. The browser may maintain its **own** HTTP cache.
- The PS5 SDL2 app now fetches one image at a time over HTTPS, decodes directly
  in memory, caps each compressed transfer at **16 MiB** and retains a bounded
  **48 MiB** texture LRU. It **does not create or write** a persistent image
  cache directory. This also applies to fallback cover sources.
- Existing cached covers are **not automatically deleted**. For an older PS5
  installation, the generated files under
  `/data/homebrew/wolodrive/covers` may be cleared *after verifying you no
  longer need them*. Do not delete `/data/homebrew/wolodrive/artwork` if you
  use hand-selected local overrides.

## Refresh the filename-only index

From the repository root:

```sh
python scripts/generate_archive_artwork.py
# For a licensed/public mirror with the same file paths:
# python scripts/generate_archive_artwork.py --base-url https://your-artwork-cdn.example/
npm run test:web
npm run build:static
```

The generator performs a shallow partial Git clone *without checkout* and
reads filename metadata only. It does **not** fetch the approximately 50,000
binary cover files. The git clone may authenticate on the development PC;
authentication never becomes part of the output.

The archive contains artwork, not the individual game-list indexes for the
17 additional catalogue source adapters. Enabling a source in Settings
controls filtering; it does not magically populate a missing game index.
Those separate imports still require functioning, verified metadata feeds.

## Known limitation

Until the archive is anonymously reachable, the app will fall back to
existing cover providers or placeholders. A successful build cannot verify
GitHub artwork delivery while anonymous requests return HTTP 404.
