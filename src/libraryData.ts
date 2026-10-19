import { LibraryCompany } from './types';

export const COMPANIES: LibraryCompany[] = [
  {
    name: 'Sony',
    r: 0,
    g: 112,
    b: 204,
    artwork_path: '/assets/library/companies/sony.png',
    consoles: [
      { name: 'PlayStation 1', platform_id: 'sony-ps1', title_id_prefix: null, artwork_path: '/assets/library/consoles/sony-ps1.png', logo_path: '/assets/library/logos/sony-ps1.png' },
      { name: 'PlayStation 2', platform_id: 'sony-ps2', title_id_prefix: null, artwork_path: '/assets/library/consoles/sony-ps2.png', logo_path: '/assets/library/logos/sony-ps2.png' },
      { name: 'PlayStation 3 (experimental)', platform_id: 'sony-ps3', title_id_prefix: null, artwork_path: '/assets/library/consoles/sony-ps3.png', logo_path: '/assets/library/logos/sony-ps3.png' },
      { name: 'PlayStation 4', platform_id: 'sony-ps4', title_id_prefix: 'CUSA', artwork_path: '/assets/library/consoles/sony-ps4.png', logo_path: '/assets/library/logos/sony-ps4.png' },
      { name: 'PlayStation 5', platform_id: 'sony-ps5', title_id_prefix: 'PPSA', artwork_path: '/assets/library/consoles/sony-ps5.png', logo_path: '/assets/library/logos/sony-ps5.png' },
      { name: 'PlayStation Portable (PSP)', platform_id: 'sony-psp', title_id_prefix: null, artwork_path: '/assets/library/consoles/sony-psp.png', logo_path: '/assets/library/logos/sony-psp.png' }
    ]
  },
  {
    name: 'Microsoft',
    r: 16,
    g: 124,
    b: 16,
    artwork_path: '/assets/library/companies/microsoft.png',
    consoles: [
      { name: 'Original Xbox', platform_id: 'microsoft-xbox', title_id_prefix: null, artwork_path: '/assets/library/consoles/microsoft-xbox.png', logo_path: '/assets/library/logos/microsoft-xbox.png' },
      { name: 'Xbox 360 (experimental)', platform_id: 'microsoft-xbox360', title_id_prefix: null, artwork_path: '/assets/library/consoles/microsoft-xbox360.png', logo_path: '/assets/library/logos/microsoft-xbox360.png' }
    ]
  },
  {
    name: 'Sega',
    r: 0,
    g: 82,
    b: 170,
    artwork_path: '/assets/library/companies/sega.png',
    consoles: [
      { name: 'SG-1000', title_id_prefix: null, artwork_path: '/assets/library/consoles/sega-sg1000.png', logo_path: '/assets/library/logos/sega-sg1000.png' },
      { name: 'Master System', title_id_prefix: null, artwork_path: '/assets/library/consoles/sega-master.png', logo_path: '/assets/library/logos/sega-master.png' },
      { name: 'Mega Drive / Genesis', title_id_prefix: null, artwork_path: '/assets/library/consoles/sega-megadrive.png', logo_path: '/assets/library/logos/sega-megadrive.png' },
      { name: 'Game Gear', title_id_prefix: null, artwork_path: '/assets/library/consoles/sega-gamegear.png', logo_path: '/assets/library/logos/sega-gamegear.png' },
      { name: 'Mega-CD / Sega CD', title_id_prefix: null, artwork_path: '/assets/library/consoles/sega-cd.png', logo_path: '/assets/library/logos/sega-cd.png' },
      { name: 'Sega 32X', title_id_prefix: null, artwork_path: '/assets/library/consoles/sega-32x.png', logo_path: '/assets/library/logos/sega-32x.png' },
      { name: 'Sega Saturn', platform_id: 'sega-saturn', title_id_prefix: null, artwork_path: '/assets/library/consoles/sega-saturn.png', logo_path: '/assets/library/logos/sega-saturn.png' },
      { name: 'Dreamcast', platform_id: 'sega-dreamcast', title_id_prefix: null, artwork_path: '/assets/library/consoles/sega-dreamcast.png', logo_path: '/assets/library/logos/sega-dreamcast.png' }
    ]
  },
  {
    name: 'Nintendo',
    r: 220,
    g: 0,
    b: 35,
    artwork_path: '/assets/library/companies/nintendo.png',
    consoles: [
      { name: 'NES / Famicom', platform_id: 'nintendo-nes', title_id_prefix: null, artwork_path: '/assets/library/consoles/nintendo-nes.png', logo_path: '/assets/library/logos/nintendo-nes.png' },
      { name: 'SNES / Super Famicom', platform_id: 'nintendo-snes', title_id_prefix: null, artwork_path: '/assets/library/consoles/nintendo-snes.png', logo_path: '/assets/library/logos/nintendo-snes.png' },
      { name: 'Nintendo 64', platform_id: 'nintendo-n64', title_id_prefix: null, artwork_path: '/assets/library/consoles/nintendo-n64.png', logo_path: '/assets/library/logos/nintendo-n64.png' },
      { name: 'GameCube', platform_id: 'nintendo-gc', title_id_prefix: null, artwork_path: '/assets/library/consoles/nintendo-gamecube.png', logo_path: '/assets/library/logos/nintendo-gamecube.png' },
      { name: 'Wii', platform_id: 'nintendo-wii', title_id_prefix: null, artwork_path: '/assets/library/consoles/nintendo-wii.png', logo_path: '/assets/library/logos/nintendo-wii.png' },
      { name: 'Wii U', platform_id: 'nintendo-wiiu', title_id_prefix: null, artwork_path: '/assets/library/consoles/nintendo-wiiu.png', logo_path: '/assets/library/logos/nintendo-wiiu.png' },
      { name: 'Nintendo Switch', platform_id: 'nintendo-switch', title_id_prefix: null, artwork_path: '/assets/library/consoles/nintendo-switch.png', logo_path: '/assets/library/logos/nintendo-switch.png' },
      { name: 'Game Boy', platform_id: 'nintendo-gb', title_id_prefix: null, artwork_path: '/assets/library/consoles/nintendo-gameboy.png', logo_path: '/assets/library/logos/nintendo-gameboy.png' },
      { name: 'Game Boy Color', platform_id: 'nintendo-gbc', title_id_prefix: null, artwork_path: '/assets/library/consoles/nintendo-gbc.png', logo_path: '/assets/library/logos/nintendo-gbc.png' },
      { name: 'Game Boy Advance', platform_id: 'nintendo-gba', title_id_prefix: null, artwork_path: '/assets/library/consoles/nintendo-gba.png', logo_path: '/assets/library/logos/nintendo-gba.png' },
      { name: 'Nintendo DS', platform_id: 'nintendo-ds', title_id_prefix: null, artwork_path: '/assets/library/consoles/nintendo-ds.png', logo_path: '/assets/library/logos/nintendo-ds.png' },
      { name: 'Nintendo 3DS', platform_id: 'nintendo-3ds', title_id_prefix: null, artwork_path: '/assets/library/consoles/nintendo-3ds.png', logo_path: '/assets/library/logos/nintendo-3ds.png' },
      { name: 'Virtual Boy', title_id_prefix: null, artwork_path: '/assets/library/consoles/nintendo-virtualboy.png', logo_path: '/assets/library/logos/nintendo-virtualboy.png' },
      { name: 'Pokémon Mini', title_id_prefix: null, artwork_path: '/assets/library/consoles/nintendo-pokemini.png', logo_path: '/assets/library/logos/nintendo-pokemini.png' }
    ]
  },
  {
    name: 'Atari',
    r: 210,
    g: 32,
    b: 38,
    artwork_path: '/assets/library/companies/atari.png',
    consoles: [
      { name: 'Atari 2600', title_id_prefix: null, artwork_path: '/assets/library/consoles/atari-2600.png', logo_path: '/assets/library/logos/atari-2600.png' },
      { name: 'Atari 5200', title_id_prefix: null, artwork_path: '/assets/library/consoles/atari-5200.png', logo_path: '/assets/library/logos/atari-5200.png' },
      { name: 'Atari 7800', title_id_prefix: null, artwork_path: '/assets/library/consoles/atari-7800.png', logo_path: '/assets/library/logos/atari-7800.png' },
      { name: 'Atari Lynx', title_id_prefix: null, artwork_path: '/assets/library/consoles/atari-lynx.png', logo_path: '/assets/library/logos/atari-lynx.png' },
      { name: 'Atari Jaguar', title_id_prefix: null, artwork_path: '/assets/library/consoles/atari-jaguar.png', logo_path: '/assets/library/logos/atari-jaguar.png' }
    ]
  },
  {
    name: 'SNK',
    r: 0,
    g: 143,
    b: 208,
    artwork_path: '/assets/library/companies/snk.png',
    consoles: [
      { name: 'Neo Geo AES', title_id_prefix: null, artwork_path: '/assets/library/consoles/snk-neogeo.png', logo_path: '/assets/library/logos/snk-neogeo.png' },
      { name: 'Neo Geo CD', title_id_prefix: null, artwork_path: '/assets/library/consoles/snk-cd.png', logo_path: '/assets/library/logos/snk-cd.png' },
      { name: 'Neo Geo Pocket', title_id_prefix: null, artwork_path: '/assets/library/consoles/snk-pocket.png', logo_path: '/assets/library/logos/snk-pocket.png' },
      { name: 'Neo Geo Pocket Color', title_id_prefix: null, artwork_path: '/assets/library/consoles/snk-pocketcolor.png', logo_path: '/assets/library/logos/snk-pocketcolor.png' }
    ]
  },
  {
    name: 'NEC',
    r: 0,
    g: 75,
    b: 160,
    artwork_path: '/assets/library/companies/nec.png',
    consoles: [
      { name: 'PC Engine / TurboGrafx-16', title_id_prefix: null, artwork_path: '/assets/library/consoles/nec-pce.png', logo_path: '/assets/library/logos/nec-pce.png' },
      { name: 'PC Engine CD / TurboGrafx-CD', title_id_prefix: null, artwork_path: '/assets/library/consoles/nec-pcecd.png', logo_path: '/assets/library/logos/nec-pcecd.png' },
      { name: 'SuperGrafx', title_id_prefix: null, artwork_path: '/assets/library/consoles/nec-supergrafx.png', logo_path: '/assets/library/logos/nec-supergrafx.png' },
      { name: 'PC-FX', title_id_prefix: null, artwork_path: '/assets/library/consoles/nec-pcfx.png', logo_path: '/assets/library/logos/nec-pcfx.png' }
    ]
  },
  {
    name: 'Bandai',
    r: 220,
    g: 0,
    b: 35,
    artwork_path: '/assets/library/companies/bandai.png',
    consoles: [
      { name: 'WonderSwan', title_id_prefix: null, artwork_path: '/assets/library/consoles/bandai-ws.png', logo_path: '/assets/library/logos/bandai-ws.png' },
      { name: 'WonderSwan Color', title_id_prefix: null, artwork_path: '/assets/library/consoles/bandai-wscolor.png', logo_path: '/assets/library/logos/bandai-wscolor.png' }
    ]
  },
  {
    name: 'Other consoles',
    r: 140,
    g: 140,
    b: 140,
    artwork_path: null,
    consoles: [
      { name: 'Panasonic 3DO', title_id_prefix: null, artwork_path: '/assets/library/consoles/other-3do.png', logo_path: '/assets/library/logos/other-3do.png' },
      { name: 'Philips CD-i', title_id_prefix: null, artwork_path: '/assets/library/consoles/other-cdi.png', logo_path: '/assets/library/logos/other-cdi.png' },
      { name: 'ColecoVision', title_id_prefix: null, artwork_path: '/assets/library/consoles/other-coleco.png', logo_path: '/assets/library/logos/other-coleco.png' },
      { name: 'Intellivision', title_id_prefix: null, artwork_path: '/assets/library/consoles/other-intellivision.png', logo_path: '/assets/library/logos/other-intellivision.png' },
      { name: 'Magnavox Odyssey 2', title_id_prefix: null, artwork_path: '/assets/library/consoles/other-odyssey2.png', logo_path: '/assets/library/logos/other-odyssey2.png' },
      { name: 'Fairchild Channel F', title_id_prefix: null, artwork_path: '/assets/library/consoles/other-channelf.png', logo_path: '/assets/library/logos/other-channelf.png' },
      { name: 'Epoch Super Cassette Vision', title_id_prefix: null, artwork_path: '/assets/library/consoles/other-scv.png', logo_path: null },
      { name: 'Amstrad GX4000', title_id_prefix: null, artwork_path: '/assets/library/consoles/other-gx4000.png', logo_path: '/assets/library/logos/other-gx4000.png' }
    ]
  },
  {
    name: 'Arcade systems',
    r: 140,
    g: 140,
    b: 140,
    artwork_path: '/assets/library/companies/arcade.png',
    consoles: [
      { name: 'Sega NAOMI', title_id_prefix: null, artwork_path: null, logo_path: '/assets/library/logos/arcade-naomi.png' },
      { name: 'Sega NAOMI 2', title_id_prefix: null, artwork_path: null, logo_path: '/assets/library/logos/arcade-naomi2.png' },
      { name: 'Sega Atomiswave', title_id_prefix: null, artwork_path: null, logo_path: '/assets/library/logos/arcade-atomiswave.png' },
      { name: 'Neo Geo MVS', title_id_prefix: null, artwork_path: null, logo_path: '/assets/library/logos/snk-neogeo.png' },
      { name: 'Various classic arcade systems', title_id_prefix: null, artwork_path: null, logo_path: '/assets/library/logos/arcade-classic.png' }
    ]
  }
];
