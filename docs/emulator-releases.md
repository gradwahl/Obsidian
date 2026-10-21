# PolDrive emulator releases

PolDrive's **Emulators** screen links to the public repository:
https://github.com/PolDrive/PolDrive-Emulators/releases

The Games/Roms library lists **58 console/system tiles**, but the Emulators
section now shows **56**, since PlayStation 4 and PlayStation 5 are excluded.
There are 56 visible emulator tiles and 43 explicit release mappings:
9 dedicated systems and 34 project-owner-designated RetroArch systems.
The remaining 13 have no specific mapped download.

The nine dedicated system mappings remain:

| System | Release | Verified main asset |
| --- | --- | --- |
| PlayStation 1 | v2 | PSXS5-v2.2.0.zip |
| PlayStation 2 | v3 | PS5SX2Installer.elf |
| PlayStation 3 (experimental) | V4 | PS5SX3-Installer.elf |
| Original Xbox | v1 | PPSA97358.zip |
| Xbox 360 (experimental) | v6 | PPSA50011.zip |
| Wii U | v5 | PS5-ECAM-v5.0.0.zip |
| Nintendo DS | v5 | PS5-ECAM-v5.0.0.zip |
| Nintendo 3DS | v5 | PS5-ECAM-v5.0.0.zip |
| Nintendo Switch | v5 | PS5-ECAM-v5.0.0.zip |

These are nine dedicated system-to-file mappings, not nine separate
releases. The repository has seven published releases and nine individual
assets.

## RetroArch v4 shared release

The following 34 distinct system entries are mapped to the SAME GitHub
release v4 file: PS5_RetroArch-v4.0.0-beta-4.zip.

- Sony: PlayStation Portable (PSP)
- Sega: SG-1000, Master System, Mega Drive / Genesis, Game Gear,
  Mega-CD / Sega CD, Sega 32X, Sega Saturn, Dreamcast
- Nintendo: NES / Famicom, SNES / Super Famicom, Nintendo 64, GameCube,
  Wii, Game Boy, Game Boy Color, Game Boy Advance, Virtual Boy, Pokémon Mini
- Atari: Atari 2600, Atari 5200, Atari 7800, Atari Lynx, Atari Jaguar
- SNK: Neo Geo AES, Neo Geo CD, Neo Geo Pocket, Neo Geo Pocket Color
- NEC: PC Engine / TurboGrafx-16, PC Engine CD / TurboGrafx-CD,
  SuperGrafx, PC-FX
- Bandai: WonderSwan, WonderSwan Color

Game Boy Color and Game Boy Advance were repeated in the request and have
been counted once each. The published RetroArch ZIP asset has a verified
GitHub URL, file size, and SHA-256 checksum. This mapping is based on the
project owner's stated coverage; we have NOT confirmed the presence of all
34 cores, gameplay compatibility, or PS5 firmware support. No emulator binary
is downloaded into the application bundle.

## React/web

Choosing a console under **Emulators** opens a release details view. A
verified mapping provides the exact GitHub release asset download URL, its
file size, SHA-256 digest, and any companion/helper files from the same
release. The emulator detail page has **one primary download action**, with
no separate "View release" or "Browse all PolDrive emulator releases" buttons.
The 13 unmatched consoles show an informational message, not an arbitrary ZIP.
The top-level emulator library still provides a general repository link.

## Native PS5 SDL2

Press X on a mapped emulator detail screen to save the selected GitHub
release asset into `/data/homebrew/wolodrive/emulators/`. The network task:

- Downloads asynchronously over HTTPS, with certificate checks and HTTPS-only
  redirects; downloads can reach 1.5 GB and are **not loaded into memory**.
- Limits the transfer to the published byte count.
- Verifies the entire downloaded file against the SHA-256 reported by GitHub.
- Saves via a temporary `.part` file and only renames the file after
  successful verification. Failed/incomplete files are removed.
- Cancels/joins its worker on app exit.
- Does **not automatically extract, install, launch or execute** ZIP/ELF files.

The native screen always displays the releases page URL, even for
unmatched systems and on PS5 installations without a working system browser.
Where an SDL system browser is available, pressing X on an unmatched system
attempts to open the releases hub.

The Linux/SDL2 build and SHA-256 verification test vectors pass, but **actual
PS5 downloads, storage permissions, HTTPS/TLS and compatibility with the
uploaded binaries have not yet been tested on PS5 hardware**. A release
filename alone is not proof that the emulator works on a specific firmware.

## Refresh verified release links

After uploading new releases, from the repository root run:

```sh
python scripts/generate_emulator_releases.py
npm run test:web
npm run build:static
make build-linux
```

The generator requests GitHub's **public** releases metadata only and never
downloads emulator binaries. It validates asset filenames, URLs and SHA-256
digests. The explicit platform-to-asset mappings are in
`scripts/generate_emulator_releases.py`; add a mapping there only after
confirming which uploaded release asset supports the platform. This protects
against incorrectly assuming every RetroArch archive contains every core.
All compiled mappings are in `src/emulatorReleases.json` and
`src/emulator_releases.h`. No GitHub credentials or ELF files are bundled.

To change the shared RetroArch list, edit RETROARCH_SYSTEMS in
scripts/generate_emulator_releases.py and regenerate src/emulatorReleases.json
and src/emulator_releases.h. The generated native and web records share the
same list. Do not confuse a mapped shared download with tested emulation.
