# Wolodrive

SDL2 homebrew catalog interface for Linux and PS5.

## TV catalog interface

The interface follows the supplied Netflix TV reference: a dark left navigation
rail, selected-game artwork across the background, a readable title and description,
and one horizontal row of landscape covers. The selected cover has a white border
and action footer. WoloDrive uses its own branding and the catalog's game artwork;
portrait covers are cropped to fill the landscape backdrop and cards, so their
composition depends on the available source image.

Use **Left/Right** or the controller **D-pad** to select a game. **Up/Down**
switch between **Home**, **Games/Roms**, **Emulators**, **Files**, **Payloads**, and
**Downloads**. Each sidebar entry has a text label and an icon: house, gamepad,
joystick, folder, chip, and download arrow. Click an entry to switch sections,
or click a cover to select it. Click
the selected card's white action footer, press **Enter/Space**, or press controller
**Cross** to request its action. Download and installation availability is described
below. Each category remembers its selection; the row scrolls to bring that game
to the left edge. **Backspace / Circle** returns to the start or cancels an active
download. **R / F5 / Triangle** refreshes the catalog. Catalog sync and storage
information are shown along the bottom. Language controls are at the top left.

Home shows the **128 latest catalog additions**, newest first across the loaded
sources. It includes explicit `category: "game"` entries with valid UTC `createdAt`
timestamps, excluding DLC, utilities, and entries without a usable date. The date
beside the focused game's metadata is its catalog addition date, not its original
retail release date. If multiple sources list a game, its newest addition timestamp
is used. Refreshing keeps a selected game when browsing beyond the first card;
the first card follows the newest addition. Without dated catalog data, Home
shows a refresh prompt instead of calling the built-in sample games new releases.

The catalog holds up to 4096 entries, accommodating the current default sources;
sources beyond that limit are not retained. Cover prefetch prioritizes Home's
latest games and Games/Roms' first 128 entries, downloading at most 256 distinct
covers per sync. This avoids downloading the enlarged catalog's entire cover set.
Games/Roms keeps the existing catalog order and includes PS4 and PS5 formats.
ROM browsing, emulator management, the file browser, and
payload management are not implemented; the corresponding sections explicitly
show that they are unavailable. Downloads shows downloading and installed entries,
or an empty-state message. These navigation entries do not enable new backends.

## Languages

Click the **globe and current language at the top left** to choose Polish, English, French,
Dutch, German, Spanish, Italian, Chinese (Simplified), or Russian. The list shows
each language's own name. Press **L** or **DualSense Square** to open or close the
menu. The control matches the sidebar entries and has an arrow for its dropdown;
the dropdown stays inside the sidebar. Use **Up/Down** or the **D-pad** to navigate, and press **Enter/Space** or
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

Built-in descriptions, labels, the window title, notifications and catalog sync
status are translated. Remote descriptions can also be translated using the
configurable service below. Game titles, publisher/source names, IDs, paths,
technical abbreviations and text embedded in cover images retain their original
wording. Description markup is converted to plain text:
HTML tags are removed, paragraphs are separated, and character entities and
JSON Unicode escapes are decoded. This setting changes the app's
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

### Online catalog descriptions

Configure a [LibreTranslate-compatible `/translate` endpoint](https://docs.libretranslate.com/api/operations/translate/)
before launching the app. In a Linux or Windows **WSL terminal**:

```sh
export WOLODRIVE_TRANSLATE_URL="https://your-translator.example/translate"
# Optional, only if your provider requires an API key:
export WOLODRIVE_TRANSLATE_API_KEY="your-provider-key"
make linux
```

Replace the example URL with your own provider or self-hosted endpoint. There is
no bundled public translation server or API key. For a self-hosted local service,
an endpoint such as `http://127.0.0.1:5000/translate` is also accepted. The server
must support source-language detection and the selected target language; its
installed language models determine translation availability and quality.
The same environment variable names apply to a native Windows executable when a
native build becomes available; set them in that process's launch environment.

Only the focused game's plain-text description is sent to the configured
provider. Source-language detection is automatic. Requests run in an SDL worker
thread and up to 64 source-text/language results are cached in memory for the
current run. Language changes never display an old translation for a different
language or source description. Pending navigation requests are replaced by the
latest requested description. TLS verification stays enabled; API keys are sent
in the request body and are not logged. Provider charges or rate limits apply.

Press **I**, click **Original**, or press the **right stick** to view the
publisher's original description and switch back. Use **Page Up/Page Down**, or
the mouse wheel over the description, to read longer text. Up to 4 KiB of source
description and 32 KiB of service response are accepted. Changing the language or
focused game resets the original-text view and scroll position.

While a request is pending or unavailable, the app shows a message in the selected
language rather than silently displaying English. Without configuration, English
mode keeps showing the original, while other languages explain that a service
needs configuring. Built-in descriptions work offline in all nine languages.
Failed requests are held for 30 seconds before retrying. Restart the app after
changing service configuration.

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

Regression tests also check description cleanup and prevent false installation
claims. Gradient coverage is checked at four window sizes with opaque and
translucent drawing, including the fallback for older rendering backends.
Online translation tests use a local mock HTTP server to verify the provider
contract, Unicode, all nine target codes, caching, source changes, failure states,
request replacement and worker shutdown. Tests do not contact a real provider or
use your configured service credentials.

## Package availability

This build browses remote catalogs and caches covers. Package downloading,
installation and launching are not implemented yet. All catalog entries start
as available; sample installed states and timer-driven download progress have
been removed. Selecting a package action explains that it is unavailable and
does not claim a download or installation succeeded. Remote catalog metadata
and cached cover images are not evidence that a game is installed.

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
