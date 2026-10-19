# Wolodrive

SDL2 homebrew catalog interface for Linux and PS5.

## Languages

Click the **language button at the top right** to choose Polish, English, French,
Dutch, German, Spanish, Italian, Chinese (Simplified), or Russian. The list shows
each language's own name. Press **L** or **DualSense Square** to open or close the
menu, use **Up/Down** or the **D-pad** to navigate, and press **Enter/Space** or
**Cross** to select. **Escape/Backspace** or **Circle** cancels; clicking outside
also dismisses the menu. Switching updates the interface immediately and
preserves catalog selection and downloads. Mouse selection follows the window's
logical scaling when it is resized.

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
SDL_ttf UTF-8 APIs. Install **Noto Sans CJK** on Linux for full Latin, Cyrillic and
Chinese coverage (Arch: `noto-fonts-cjk`; Debian/Ubuntu: `fonts-noto-cjk`). The app
also finds Microsoft YaHei in Windows' font directory, including through WSL.
Alternatively, provide a font covering all three scripts as `assets/font_cjk.ttc`
(on PS5: `/data/homebrew/wolodrive/assets/font_cjk.ttc`). Custom `assets/font.ttf`
and `assets/font_bold.ttf` are also searched. Fonts covering all scripts are
preferred; a warning is printed if the selected font cannot display Chinese.
Fonts are loaded locally; no font downloads occur when a language is selected.

## Build and checks

On Linux, install GCC, Make, pkg-config, Python 3, SDL2, SDL2_ttf, SDL2_image,
libcurl and Noto Sans CJK fonts, then run:

```sh
make build-linux
make test
make linux
```

Tests validate translation completeness and printf placeholders, locale
precedence, fallback, preference persistence, mouse/keyboard/controller menu
selection, cancellation, glyph coverage for every translation, and SDL rendering
with a dummy video driver. They use a temporary preferences
directory. Set `WOLODRIVE_TEST_SCREENSHOTS=1` when running `make test` to save
screenshots for each language in `build/`.

For PS5, configure `PS5_PAYLOAD_SDK` and run `make ps5`.

Native Windows remains a project requirement, but the repository currently has
no native Windows build target. Existing platform gaps include POSIX storage
queries, pthread catalog workers, and GNU string functions. The Windows preview
below runs the Linux build through WSL; it does not verify a native Windows
executable. A separate native Linux graphical session is also required to verify
Linux display behavior independently of WSLg.

## Windows preview through WSL

Run `make linux` from your WSL distribution in this repository. WSL uses SDL's
software renderer by default because some WSLg accelerated drivers create a
window successfully but display only black. Other Linux environments and PS5
continue to prefer accelerated rendering. The selected video driver and renderer
are printed at startup.

To test a particular renderer, set `SDL_RENDER_DRIVER` explicitly. For example,
to force software rendering when troubleshooting an older build:

```sh
SDL_RENDER_DRIVER=software ./build/wolodrive
```

If the app reports missing fonts, install Noto Sans CJK as described above.
