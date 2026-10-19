# Wolodrive

SDL2 homebrew catalog interface for Linux and PS5.

## Languages

The interface supports English, Polish and Spanish. Press **L** on a keyboard or
**Square** on a DualSense controller to cycle languages. The current language and
shortcut appear in the top bar. Switching updates the interface immediately and
preserves catalog selection and downloads.

On first launch, the app detects `LC_ALL`, `LC_MESSAGES`, then `LANG`. Unsupported
locales default to English. A saved choice takes priority on subsequent launches.
Linux stores the choice in SDL's user preference directory (normally
`~/.local/share/Wolodrive/Wolodrive/language`); PS5 uses
`/data/homebrew/wolodrive/language`. If saving fails, the language still changes
for the current session and a notification explains that it could not be saved.

Built-in descriptions, labels, notifications and catalog sync status are
translated. Game titles and descriptions received from remote catalogs are
displayed as supplied by their publishers. This setting changes the app's
interface language; it does not change the language inside games.

Translations live in `src/translations.def` as UTF-8 text with stable `TextId`
keys. Keep all printf placeholders identical across translations. Rendering uses
SDL_ttf UTF-8 APIs; custom PS5 fonts must include Latin accented characters.

## Build and checks

On Linux, install GCC, Make, pkg-config, Python 3, SDL2, SDL2_ttf, SDL2_image,
libcurl and DejaVu Sans fonts, then run:

```sh
make build-linux
make test
make linux
```

Tests validate translation completeness and printf placeholders, locale
precedence, fallback, preference persistence, keyboard/controller switching and
SDL rendering with a dummy video driver. They use a temporary preferences
directory. Set `WOLODRIVE_TEST_SCREENSHOTS=1` when running `make test` to save
screenshots for each language in `build/`.

For PS5, configure `PS5_PAYLOAD_SDK` and run `make ps5`.
