<div align="center">

![PolDrive banner](docs/poldrive-banner.svg)

**A console-first game catalogue, built for PlayStation 5, Linux and the web.**

Browse games and ROM listings in a TV-style interface with cover art, search, platform libraries and detailed game pages.

![Version](https://img.shields.io/badge/version-2.0.0-e50914?style=flat-square)
![Web](https://img.shields.io/badge/Web-React%20%2B%20Vite-20232a?style=flat-square)
![Linux](https://img.shields.io/badge/Linux-SDL2-20232a?style=flat-square)
![PS5](https://img.shields.io/badge/PS5-Single%20ELF%20installer-20232a?style=flat-square)

[What is PolDrive?](#what-is-poldrive) · [Features](#features) · [Get started](#get-started) · [Supported systems](#supported-systems) · [Documentation](#documentation)

</div>

> [!IMPORTANT]
> **PolDrive browses catalogues and manages accessible files; game installation is still in development.** It does **not** yet download game packages, install PKGs, mount games or launch games. The single-ELF installer installs **PolDrive itself**, not the games listed in its catalogue.

## What is PolDrive?

**PolDrive** is an in-development, controller-friendly interface for exploring game and homebrew catalogue metadata. It presents games, ROM listings, artwork and available source/package information through a dark, console-inspired UI.

The project contains three related experiences:

- **PS5 app** — one installer ELF that sets up PolDrive, configures its quiet background service and creates a dashboard icon under **Home / Media**. Installation stays closed; selecting the icon opens the app directly.
- **Native app** — the C/SDL2 PS5 implementation is the UI reference, with a Linux build for development and comparison. Native PS5 rendering remains experimental; the default installer uses the working browser UI.
- **Web app** — a React/TypeScript preview that runs in a desktop browser or from static hosting, including AI Studio.

The native app and browser use related catalogue data, but **do not yet have identical catalogue coverage or fully verified UI/input parity**.

## Features

| | Feature | What it does |
| :-- | :-- | :-- |
| 🎮 | **TV-style browsing** | Artwork-first Home screen, horizontal carousels and console-style navigation. |
| 🔎 | **Search and discovery** | Search loaded entries by name or title ID; browse games by console and category. |
| 🗂️ | **Platform libraries** | Explore PlayStation, Nintendo, Sega, Xbox and other console families. Availability depends on the catalogue source. |
| 🖼️ | **Artwork and screenshots** | Cover art with fallback handling, backdrops and screenshot panels where images are available. |
| 📋 | **Game details** | Descriptions, IDs, versions, source/package formats, related DLC and available metadata. |
| 🎛️ | **Filters and preferences** | Filter by catalogue source and package format; save language and filter preferences. |
| 🎨 | **Appearance choices** | Switch between Netflix / Glass and (u)biquity themes. |
| 🌐 | **Nine UI languages** | English, Polish, French, Dutch, German, Spanish, Italian, Simplified Chinese and Russian. |
| 🕹️ | **Multiple input methods** | Keyboard and mouse in the web preview; controller-oriented navigation in the native interface. Input parity is still being checked. |
| 📁 | **PS5 file manager** | Browse accessible console/USB folders and use file operations through the native filesystem bridge. The default PS5 service supports console and LAN access. |
| 📺 | **Direct dashboard launch** | Install PolDrive from one ELF, then open its Media icon without the interim `OK` page or a second browser window. |

Bundled source snapshots load progressively as small catalogue pages. They are not a live, complete mirror of every source website; artwork and source links may require internet access. **Not implemented yet:** game-package downloads, download queues, game installation, mounting, game launching, and remote control of those actions. See [remote-install-workflow.md](docs/remote-install-workflow.md) for the outstanding work.

## Get started

Choose how you want to run PolDrive.

### 1. Web preview (Windows, Linux or macOS)

The easiest way to explore the interface is in a browser.

**Requirements:** [Node.js](https://nodejs.org/) 20 or newer (22 recommended) and npm.

```bash
git clone https://github.com/PolDrive/PolDrive.git
cd PolDrive
npm ci
npm run dev
```

Open [localhost:3000](http://localhost:3000). The Vite development server uses port `3000` by default.

To create and serve the **standalone static preview** without keeping Vite or Node running:

```bash
npm run build:static
python3 -m http.server 3000 --directory static-preview
```

On Windows, use `python` instead of `python3`. Open [localhost:3000](http://localhost:3000) again. The generated `static-preview/` folder can be hosted as ordinary static files at the URL root (including AI Studio). **Node is only needed to build it**; a *prebuilt* `static-preview/` can be served with any static HTTP server. Do not open the page using `file://`, because assets use root-relative paths. Standalone static hosting does not provide the PS5 filesystem bridge. **AI Studio deployment not verified.**

### 2. Native Linux app

**Requirements:** GCC, Make, pkg-config, Python 3, SDL2, SDL2_ttf, SDL2_image and libcurl development libraries. Noto Sans CJK fonts are recommended for full multilingual character coverage.

On Debian/Ubuntu-based distributions, install the dependencies:

```bash
sudo apt update
sudo apt install build-essential pkg-config python3 \
  libsdl2-dev libsdl2-ttf-dev libsdl2-image-dev libcurl4-openssl-dev fonts-noto-cjk
```

Then, from the project root:

```bash
make build-linux
make linux
```

The executable is generated at `build/wolodrive`. Windows users can run this Linux build using **WSL/WSLg** with a suitable graphical environment; that is *not* a native Windows build.

### 3. PlayStation 5 (single-ELF installer)

**Requirements:** a PS5 environment capable of loading homebrew ELFs, [PS5 Payload Manager](https://github.com/itsPLK/ps5-payload-manager), and a compatible loader. Building from source additionally requires the [PS5 Payload SDK](https://github.com/ps5-payload-dev/sdk) with the required dependencies.

With the SDK installed and `PS5_PAYLOAD_SDK` configured, build the installer:

```bash
make ps5
```

The normal build produces one payload to copy: `build/poldrive.elf`. Embedded
service/helper files are generated inside `build/.internal/`; leave them there.

The browser uses compact catalogue indexes, loads full metadata for visible
items, resizes cached artwork and renders bounded card windows. All catalogue
entries remain searchable. See [memory behavior and verification](docs/ps5-performance.md)
for cache limits and image compatibility fallbacks. PS5 memory savings still
require a device test.

1. Copy **only** `build/poldrive.elf` to `/data/pldmgr/payloads/poldrive/poldrive.elf` on the console.
2. Run it once through Payload Manager. It installs PolDrive, configures background startup and creates/repairs the **PolDrive** icon under **Home / Media**, then exits without opening the app.
3. Select **PolDrive** under Media to open it directly. No second ELF needs copying, no manual service setup is required, and the old `OK` → close → reopen sequence is removed.

**Confirmed working on one PS5 setup on 21 October 2026:** the single-ELF setup and direct dashboard launch. Fresh-jailbreak autoload and full controller/native-browser parity have not been separately confirmed; firmware and loader compatibility may vary.

The service listens on all IPv4 interfaces. From another device on the same LAN, open `http://<PS5-IP>:8098/` (for example, `http://192.168.0.137:8098/`). The Media tile still uses the console loopback URL. Files uses the same native bridge with existing same-origin/session checks; LAN clients can use it too, so use a trusted LAN. To upgrade from the loopback-only build, stop the old service before running the new installer; closing the app tile alone does not stop it. After each jailbreak, start Payload Manager so its autoload sequence starts the service. Autoload does not run on an unjailbroken PS5 boot. Existing entries, their ordering/delays and unrelated settings are preserved; enabling autoload also enables the existing sequence. Original startup files are backed up with `.poldrive-backup` suffixes before editing.

An ELF loader is required during installation. The installer checks the exact service/build before changing the Media icon; failure details are in `/data/poldrive-install.log`, `/data/poldrive-launcher.log` and `/data/wolodrive-shortcut.log`. The service writes `/data/wolodrive-web.log`. [Setup, verification and rollback](docs/ps5-build.md).

`make ps5-shortcut` builds the same single-file installer. Native PS5 SDL rendering remains **experimental** and available through `make ps5-native`, which produces `build/poldrive-native-installer.elf`. The existing renderer failure is not fixed by this packaging change. Browser-only diagnostics (`make ps5-web`) and the earlier manual two-file mode (`make ps5-webfirst`) remain available for recovery; neither is needed for normal installation.

## Supported systems

There is a difference between **devices that run PolDrive** and **consoles represented in its catalogue**.

### Where PolDrive runs

| Environment | Experience | Status |
| :-- | :-- | :-- |
| **PS5** (homebrew-capable) | Single-ELF installation, console/LAN browser UI and direct Home / Media icon | Installation and dashboard launch confirmed on one console; broader compatibility and controller parity remain unverified |
| **Linux** | Native SDL2 app | Build path available; graphical behaviour depends on drivers and environment |
| **Windows** | Browser preview, or native Linux app via WSL/WSLg | Browser path available; native Windows executable not verified |
| **macOS** | Browser preview | Expected with a compatible modern browser; not separately verified |
| **Other devices on the PS5 LAN** | Default HTTP service on port 8098 | LAN listener tested locally; updated PS5 deployment remains unverified |

### Consoles shown in the library

The library UI includes platform groupings such as **PS1, PS2, PS3, PS4, PS5, PSP; Xbox and Xbox 360; Nintendo NES through Switch and handhelds; Sega consoles; Atari; SNK; NEC; and select arcade and other systems**.

These are **catalogue categories, not a claim that PolDrive can emulate or run those systems**. Some console tiles have no connected catalogue data. Games and ROMs appear only when relevant source metadata is available; the Emulators section is not a bundled emulator.

## Controls

| Action | Keyboard / mouse | Native PS5-style controller |
| :-- | :-- | :-- |
| Move through the catalogue | Arrow keys / click | D-pad |
| Open a selected game | Enter / click | Cross |
| Go back / close a panel | Escape / Backspace | Circle |
| Change language | Language menu / `L` | Square |
| Refresh catalogue/source status | `R` or `F5` where supported | Triangle |

Controller input is part of the native UX; **the browser implements only a subset of the same controller interactions**. Keyboard and mouse are recommended for the standalone web preview.

## Development

The frontend uses **React 19, TypeScript and Vite**. The native implementation uses **C, SDL2 and libcurl**. The PS5 installer bundles a quiet C HTTP service with the statically built browser UI and a same-origin filesystem bridge accessible from the console and LAN. It configures Payload Manager autoload and registers a direct Media tile. Ordinary static hosting, including AI Studio, remains independent of the console and its bridge.

```bash
npm run lint              # TypeScript checks
npm run test:web          # Web UI and layout checks
npm run build:static      # Rebuild static-preview/
python3 tests/check_static_preview.py
make test-web-payload     # Installer, launch, HTTP and file-bridge tests (Linux/WSL + GCC)
make test-native-snapshot # Linux native catalogue test (SDL2 toolchain required)
```

Use `python` rather than `python3` on Windows. The full native test suite is `make test` on a configured Linux environment. A successful static build does not by itself validate AI Studio deployment or native PS5 behaviour.

## Documentation

- [PS5 single-ELF installation, troubleshooting and rollback](docs/ps5-build.md)
- [PS5 performance notes and device verification](docs/ps5-performance.md)
- [Planned download and remote installation workflow](docs/remote-install-workflow.md)
- [Catalogue source coverage](docs/source_index_coverage.md)
- [Artwork coverage](docs/artwork_coverage.md)
- [Full technical reference and original development notes](docs/technical-reference.md)

---

<div align="center">
  <sub>Built around a PS5-first experience · Catalogue browsing today, deeper console integration in progress.</sub>
</div>
