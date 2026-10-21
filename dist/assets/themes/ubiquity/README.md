# (u)biquity visual adaptation for PolDrive

Original theme: **(u)biquity v2.0.4**, by **Red Schism** (Playnite Fullscreen).
Original project: https://github.com/RedSchism/ubiquity

This optional theme is a native SDL2/React reimplementation of the supplied theme's
separate cinematic Home renderer, compact left icon navigation, a bottom game cover strip, and minimalist white focus accents. It does
not embed or execute Playnite WPF/XAML or require Playnite to be installed.

`fade-left.png` and `fade-bottom.png` are heavily downsampled fade profiles
extracted from the user-supplied .pthm theme package (not the sample game cover art).
Web CSS uses them for the fades. The native renderer reproduces the same look with
scanline alpha fills, without loading textures. This is a **style adaptation**, not
a pixel-for-pixel implementation of (u)biquity's layouts or extensions.

To use: Settings -> Appearance / Wygląd -> (u)biquity.
Switch back with Settings -> Appearance / Wygląd -> Netflix / Glass.

Theme selection persists independently of format/source filters:
- Native: `filters.theme` stored beside the existing `filters` settings file.
- Web: `localStorage['poldrive-ui-theme']`.

Legacy installations without a theme preference default to Netflix / Glass.

Visual fidelity note: Playnite's optional Extra MetaData Loader supplies transparent
per-game logo art and additional metadata. PolDrive currently uses game titles as
typographic fallbacks; when a game's backdrop already contains a logo it may also
appear within the background. Matching the sample image exactly requires a separate
logo-artwork pipeline and suitable images for each game.

## Full-screen game details

Selecting a game while (u)biquity is enabled opens the dedicated full-screen
cinematic detail page (`UbiquityGameDetails.tsx` and `ui_ubiquity_details.h`).
The separate Netflix/Glass detail modal remains unchanged. The primary button
uses PolDrive's existing action handler; the secondary `...` menu exposes
packages, DLC and screenshots. Escape/Circle closes overlays before leaving
this page.

The original theme's transparent per-game logos, real playtime, achievement
progress, genre classifications and audio controls cannot be populated
without corresponding catalogue/console data. PolDrive shows its available
source, year, platform, rating, size, version and status instead.

## Reference-sized square library tiles

The (u)biquity Home carousel now uses a **232×232** artwork tile with a
**20px** gap at the 1920×1080 design resolution. This is a measured
approximation of the theme's square-cover reference, not a hard-coded size
from the Playnite XAML package (actual Playnite cover sizes respond to layout
settings). The title is overlaid in the bottom 40px of the square; the
bottom-right arrow appears for available installable packages only. React and
SDL2 use matching card dimensions and scroll pitch. Netflix/Glass remains
unchanged.

## Selected-cover focus

The extra solid-white strip on a selected cover has been removed. The (u)biquity
selected card now uses a single 2px white outline and a soft red/white double
halo burst over a 2.6-second cycle, matched in React CSS and native SDL2.
The browser stops the animation under `prefers-reduced-motion: reduce` and
retains a steady glow. This is a local focus-only effect, not a full-screen flash.

## Games/Roms → Sony → PlayStation 5

The console game catalogue inherits the selected theme. React passes the
`uiTheme` preference from App → LibraryBrowser → HomeView. When (u)biquity
is active it reuses the same cinematic background, 232px square cover row,
20px spacing, white/red animated focus and download arrows as Home.
The Games/Roms breadcrumb and Back button remain accessible. SDL2 checks the
active library depth (`LIBRARY_GAMES`) and uses the same native Ubiquity
renderer and matching card hit targets; Netflix/Glass remains untouched.
