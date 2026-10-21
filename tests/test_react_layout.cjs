// Source/SSR checks only. These do not establish browser rendering or input correctness.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const ts = require('typescript');
const React = require('react');
const { renderToStaticMarkup: render } = require('react-dom/server');

// Load the same TSX components as Vite without installing a second test runtime.
for (const extension of ['.ts', '.tsx']) {
  require.extensions[extension] = (module, filename) => {
    // Vite resolves .ts before .json; Node's extensionless require does the opposite.
    const source = fs.readFileSync(filename, 'utf8').replace(/(from\s+['"])(\.[^'"]+)(['"])/g,
      (match, prefix, name, quote) => {
        const extension = ['.ts', '.tsx'].find(ext => fs.existsSync(path.resolve(path.dirname(filename), name + ext)));
        return extension ? prefix + name + extension + quote : match;
      });
    const { outputText } = ts.transpileModule(source, {
      compilerOptions: { target: ts.ScriptTarget.ES2022, module: ts.ModuleKind.CommonJS,
        jsx: ts.JsxEmit.ReactJSX, esModuleInterop: true }
    });
    module._compile(outputText, filename);
  };
}
const root = path.resolve(__dirname, '..');
const { TV_LAYOUT, fitViewport } = require('../src/layout.ts');
const { GameCard, CoverArt } = require('../src/components/GameCard.tsx');
const { HomeView } = require('../src/components/HomeView.tsx');
const { NativeViewport } = require('../src/components/NativeViewport.tsx');
const { fittedFontSize } = require('../src/components/FittedText.tsx');
const { GameDetailModal } = require('../src/components/GameDetailModal.tsx');
const { UbiquityGameDetails } = require('../src/components/UbiquityGameDetails.tsx');
const { LibraryBrowser } = require('../src/components/LibraryBrowser.tsx');
require.extensions['.css'] = () => {};
const { FileBrowser } = require('../src/components/FileBrowser.tsx');
const fileMarkup = render(React.createElement(FileBrowser, { language: 'en', sidebarFocused: false, onSidebar() {} }));
for (const label of ['Open folder', 'Parent folder', 'Refresh', 'Copy', 'Move', 'Paste', 'Delete', 'Unzip'])
  assert.ok(fileMarkup.includes(label), `Files toolbar: ${label}`);
assert.ok(fileMarkup.includes('Folder access is unavailable'), 'SSR/no browser API must show truthful unavailable state');
const filesNative = fs.readFileSync(path.join(root, 'src/file_manager_ui.h'), 'utf8');
const filesCSS = fs.readFileSync(path.join(root, 'src/files.css'), 'utf8');
assert.match(filesNative, /130\+index\*210,252,198,52/);
assert.match(filesCSS, /top: 252px/);
assert.match(filesCSS, /width: 198px; height: 52px/);
assert.match(filesNative, /332\+\(index%8\)\*68,1668,64/);
assert.match(filesCSS, /top:992px; width:1668px; height:28px/,
  'PC controls must fit inside the 1020px native content area');
const fileActions = require('../src/fileManager.ts').FILE_ACTIONS;
assert.deepEqual([...filesNative.match(/file_labels\[\][\s\S]*?};/)[0].matchAll(/TXT_FILES_(\w+)/g)]
  .map(match => match[1]), fileActions, 'Native/web Files action ordering');
const { EmulatorReleaseView, getEmulatorRelease, EMULATOR_RELEASES_URL } =
  require('../src/components/EmulatorReleaseView.tsx');
const emulatorReleaseData = require('../src/emulatorReleases.json');

const { COMPANIES, consolesForSection } = require('../src/libraryData.ts');
const { archiveLibraryUrl } = require('../src/archiveAssets.ts');
const { Sidebar } = require('../src/components/Sidebar.tsx');
const { SettingsView } = require('../src/components/SettingsView.tsx');
const { t } = require('../src/translations.ts');
assert.match(t('en', 'TXT_NATIVE_FALLBACK'), /Native startup failed.*Web fallback is active/);
const bridgeSource = fs.readFileSync(path.join(root, 'src/fileBridge.ts'), 'utf8');
assert.match(bridgeSource, /initialTrail\(\).*handle\(''\), this.handle\('data'\)/,
  'Console browser opens /data with parent access to /, like native');
assert.match(fs.readFileSync(path.join(root, 'src/App.tsx'), 'utf8'), /window.location.hash === '#native-unavailable'/);
const { INITIAL_CATALOG, withArchiveArtwork } = require('../src/catalog.ts');
const { coverSources } = require('../src/artworkMatches.ts');
const native = fs.readFileSync(path.join(root, 'src/ui_netflix.c'), 'utf8');
const css = fs.readFileSync(path.join(root, 'src/native.css'), 'utf8');
// An artwork index is metadata, never the images themselves.
const archiveURLs = JSON.parse(fs.readFileSync(path.join(root, 'src/archiveArtworkUrls.json'), 'utf8'));
assert.ok(Object.keys(archiveURLs).length >= 700, 'Verified artwork match index should be generated');
assert.equal(INITIAL_CATALOG.length, 862);
for (const [gameKey, url] of Object.entries(archiveURLs)) {
  assert.match(url, /^https:\/\/raw\.githubusercontent\.com\/PolDrive\/PolDrive-archive\/main\/assets\/covers\/Sony\/PlayStation%20[45]\//);
  assert.ok(!url.includes('..'), 'Archive artwork URLs must not contain traversal paths');
  assert.ok(gameKey.includes('|'));
}
const archived = INITIAL_CATALOG.find(item => item.id_code === 'CUSA03041');
assert.ok(archived, 'Native and web should share this catalog game');
assert.equal(archived.boxart_url, archiveURLs['CUSA03041|Red Dead Redemption 2']);
assert.equal(coverSources(archived)[0], archived.boxart_url);
assert.ok(coverSources(archived).includes(archived.cover_url),
  'Original source cover is retained as network failure fallback');
assert.equal(withArchiveArtwork({ ...archived, cover_local_path: '/local/custom.png', boxart_url: undefined })
  .boxart_url, undefined, 'Local image overrides must take precedence');
const streamingSource = fs.readFileSync(path.join(root, 'src/artwork.c'), 'utf8');
assert.match(streamingSource, /IMG_Load_RW\(stream, 1\)/);
assert.match(streamingSource, /ART_TRANSFER_LIMIT \(16u \* 1024u \* 1024u\)/);
assert.match(streamingSource, /#define ART_BUDGET \(48u \* 1024u \* 1024u\)/);
assert.doesNotMatch(streamingSource, /fopen\(temporary|rename\(temporary|fwrite\(data/,
  'Native streamed images must not be copied into persistent disk cache');
assert.match(fs.readFileSync(path.join(root, 'src/sys_bridge.c'), 'utf8'),
  /const char \*archive_url = sys_archive_cover_url/);

const values = { rail: 'RAIL_WIDTH', contentX: 'CONTENT_X', cardsY: 'CARDS_Y',
  cardWidth: 'CARD_WIDTH', cardHeight: 'CARD_HEIGHT', cardGap: 'CARD_GAP',
  cardTitle: 'CARD_TITLE', cardFooter: 'CARD_FOOTER' };
for (const [key, name] of Object.entries(values)) {
  const expression = native.match(new RegExp(`\\b${name} = ([^,}]+)`))[1].trim();
  const expected = expression === 'RAIL_WIDTH + 38' ? TV_LAYOUT.rail + 38 : Number(expression);
  assert.equal(TV_LAYOUT[key], expected, `React ${key} must match native geometry`);
}
function declaration(selector, property) {
  const block = css.slice(css.indexOf(selector + ' {')).split('}')[0];
  return Number(block.match(new RegExp(`(?:[;{]\\s*)${property}: ([0-9]+)px`))[1]);
}
assert.equal(declaration('.tv-card', 'width'), TV_LAYOUT.cardWidth);
assert.equal(declaration('.tv-cover', 'height'), TV_LAYOUT.cardHeight);
assert.equal(declaration('.tv-card', 'height'), TV_LAYOUT.cardHeight + TV_LAYOUT.cardTitle + TV_LAYOUT.cardFooter);
assert.equal(declaration('.tv-carousel', 'left') + 4 + TV_LAYOUT.rail, TV_LAYOUT.contentX);
assert.equal(declaration('.tv-carousel', 'top') + 4, TV_LAYOUT.cardsY);
assert.equal(declaration('.tv-detail-panel', 'top') + declaration('.tv-detail-tabs', 'top'), 850);
assert.equal(fittedFontSize(150, 200, 20), 20);
assert.equal(fittedFontSize(400, 200, 20), 10);
assert.equal(fittedFontSize(0, 200, 20), 20);
for (const [width, height, scale, left, top] of [
  [1920, 1080, 1, 0, 0], [1280, 720, 2/3, 0, 0],
  [1024, 768, 1024/1920, 0, 96], [3440, 1440, 4/3, 440, 0],
  [1080, 1920, 1080/1920, 0, 656.25]
]) {
  assert.deepEqual(fitViewport(width, height), { scale, left, top });
}
const noop = () => {};
const game = { id_code: 'PPSA00001', title: 'Example <game>', version: '01.00', source_name: 'Fixture',
  is_game: true, is_dlc: false, screenshot_urls: [], catalog_added_at: 20261019120000,
  description: 'Full game description', install_path: '', cover_local_path: '/assets/covers/PPSA00001.cover',
  cover_r: 120, cover_g: 60, cover_b: 30, size_gb: 42, format: 'pkg', state: 'available', progress: 0 };
const cardProps = { game, language: 'en', onSelect: noop, onOpen: noop, isSelected: true };
const selected = render(React.createElement(GameCard, cardProps));
assert.match(selected, /tv-platform-ps5/);
assert.ok(selected.includes(`[X] ${t('en', 'TXT_GAME_DETAILS')}`));
assert.match(selected, /Example &lt;game&gt;/);
const unselected = render(React.createElement(GameCard, { ...cardProps, game: { ...game, id_code: 'CUSA00001' }, isSelected: false }));
assert.match(unselected, /tv-platform-ps4/);
assert.ok(unselected.includes('PKG / 42.0 GB'));
const completeBox = render(React.createElement(CoverArt, { game: { ...game, boxart_url: '/box.jpg' } }));
assert.match(completeBox, /src="\/box.jpg"/);
assert.ok(!completeBox.includes('tv-platform-ps5'), 'Complete box art must retain its original platform branding');
const missing = render(React.createElement(CoverArt, { game: { ...game, cover_local_path: '' } }));
assert.ok(!missing.includes('<img'));
assert.match(missing, /PolDrive/);
const games = Array.from({ length: 5 }, (_, index) => ({ ...game, id_code: `PPSA0000${index}`, title: `Game ${index}` }));
const homeProps = { games, language: 'en', selected: 1, sidebarFocused: false, onSelect: noop, onOpen: noop, onRefresh: noop, syncInProgress: false };
const home = render(React.createElement(NativeViewport, null, React.createElement(HomeView, homeProps)));
assert.match(home, /width:1920px;height:1080px/);
assert.match(home, /<h1 class="tv-hero-info-appear">Game 1<\/h1>/);
assert.match(home, /2026-10-19/);
assert.equal((home.match(/class="tv-meta-pill"/g) || []).length, 5,
  'Game id, version, format, size and date should render as separate pills');
const scoredHome = render(React.createElement(HomeView, {
  ...homeProps, games: games.map(g => ({ ...g, rating_score: 8.7, download_count: 140 }))
}));
assert.equal((scoredHome.match(/class="tv-meta-pill"/g) || []).length, 7);
const ubiquity = render(React.createElement(HomeView, {
  ...homeProps, uiTheme: 'ubiquity'
}));
assert.match(ubiquity, /data-layout="ubiquity"/);
assert.match(ubiquity, /ubq-cinematic-background/);
assert.match(ubiquity, /ubq-feature-title/);
assert.match(ubiquity, /ubq-shelf-track/);
assert.match(ubiquity, /ubq-focus-scroll-layer/,
  'Focus glow must have a separate layer so scrolling does not animate its offset');
assert.equal((ubiquity.match(/class="ubq-game-cover/g) || []).length, games.length);
assert.equal((ubiquity.match(/class="ubq-square-art"/g) || []).length, games.length,
  '(u)biquity artwork should be separate square images');
assert.equal((ubiquity.match(/class="ubq-cover-footer"/g) || []).length, games.length,
  '(u)biquity titles should overlay the bottom of the square artwork');
assert.equal((ubiquity.match(/class="ubq-cover-download"/g) || []).length, games.length,
  'Downloadable games should have a download-arrow indicator');
// Regression: the first/selected game still renders the original cover source.
const firstFocused = render(React.createElement(HomeView, {
  ...homeProps, uiTheme: 'ubiquity', selected: 0
}));
assert.match(firstFocused,
  /class="ubq-game-cover ubq-game-cover-selected"[^>]*>[\s\S]*?src="\/assets\/covers\/PPSA00001\.cover"/,
  'Selected index 0 must retain its cover <img>, not become an empty black tile');
const styleBody = selector => css.match(new RegExp('\\.' + selector + ' \\{([^}]+)\\}'))?.[1] || '';
const selectedZ = Number(styleBody('ubq-game-cover-selected').match(/z-index:\s*(\d+)/)?.[1]);
const frameZ = Number(styleBody('ubq-focus-pill').match(/z-index:\s*(\d+)/)?.[1]);
assert.ok(selectedZ > frameZ,
  'Crimson focus frame must paint behind the selected cover, not over its artwork');
assert.match(styleBody('ubq-focus-pill'), /background-image:\s*linear-gradient\(/,
  'The focus frame gradient must remain intact');
assert.match(styleBody('ubq-focus-pill'), /box-shadow:/,
  'The focus frame glow must remain intact');
assert.match(styleBody('ubq-shelf-track'), /z-index:\s*5/,
  'Every cover must paint above the glow while focus glides between cards');
assert.match(styleBody('ubq-focus-scroll-layer'), /z-index:\s*4/,
  'The glow wrapper must remain behind the carousel track');
assert.match(styleBody('ubq-game-cover-selected'), /transform:\s*scale\(1\.08\) translateY\(-4px\)/,
  'The selected-card scale animation must remain intact');
assert.match(styleBody('ubq-focus-pill'), /top:\s*42px/, 'Focus ring vertical placement follows the enlarged card');
const ubiquitySource = fs.readFileSync(path.join(root, 'src/components/UbiquityHomeView.tsx'), 'utf8');
assert.match(ubiquitySource, /index \* 252 - 15/,
  'Focus ring still uses the original 252px card stride');
assert.match(ubiquitySource, /layer\.style\.transform = `translate3d\(\$\{-track\.scrollLeft\}px, 0, 0\)`/,
  'Scroll compensation must follow actual scrollLeft, independently of React renders');
assert.match(ubiquitySource, /track\.addEventListener\('scroll', syncFocusScroll, \{ passive: true \}\)/,
  'Frame offset must update throughout smooth scrolling, not just when focus changes');
assert.match(ubiquitySource, /track\.removeEventListener\('scroll', syncFocusScroll\)/,
  'The scroll listener must be cleaned up when the carousel changes');
const nonDownloadable = render(React.createElement(HomeView, {
  ...homeProps, uiTheme: 'ubiquity',
  games: [ { ...game, state: 'installed' }, { ...game, state: 'downloading' },
           { ...game, format: 'catalog', state: 'available' } ]
}));
assert.doesNotMatch(nonDownloadable, /class="ubq-cover-download"/,
  'Installed, downloading and catalog-only entries have no download indicator');

const largeCatalog = Array.from({ length: 1000 }, (_, index) =>
  ({ ...game, id_code: 'EXT-' + index, title: 'Imported ' + index }));
const longUbq = render(React.createElement(HomeView,
  { ...homeProps, games: largeCatalog, selected: 500, uiTheme: 'ubiquity' }));
assert.ok((longUbq.match(/class="ubq-game-cover/g) || []).length <= 21,
  'Large imported catalogues must render only near-focus covers');
assert.match(longUbq, /501 \/ 1000/);
const longNetflix = render(React.createElement(HomeView,
  { ...homeProps, games: largeCatalog, selected: 500, uiTheme: 'netflix' }));
assert.ok((longNetflix.match(/class="tv-card /g) || []).length <= 21,
  'Netflix must also window long single-console catalogues');
assert.match(longNetflix, /501 \/ 1000/);

assert.doesNotMatch(ubiquity, /ubq-cover-line/,
  'The selected Ubiquity tile must not add a second solid white stripe at its bottom');
assert.match(css, /\.ubq-game-cover-selected::after \{/,
  'Selected artwork and title need a single inset white border');
assert.match(css, /@keyframes ubqFocusStrobe/,
  'The selected tile should have a glowing strobe animation');
assert.match(css, /\.ubq-game-cover-selected \{[^}]*animation: ubqFocusStrobe 2\.6s/);
assert.match(css, /@media \(prefers-reduced-motion: reduce\)/);
assert.doesNotMatch(ubiquity, /tv-hero-btn-primary|tv-card-footer/);
assert.equal((home.match(/class="tv-card /g) || []).length, games.length);
const nativeThemeSource = fs.readFileSync(path.join(root, 'src/ui_netflix.c'), 'utf8');
assert.match(nativeThemeSource, /static void render_ubiquity_home\(/);
assert.match(nativeThemeSource, /render_ubiquity_home\(app\)/);
assert.match(nativeThemeSource, /UBQ_CARD_W = 232, UBQ_CARD_GAP = 20, UBQ_CARD_Y = 755, UBQ_CARD_H = 232/);
assert.match(nativeThemeSource, /SDL_Rect cover_rect = \{card.x, card.y, card.w, card.h\}/);
// SDL already draws the pill before the texture; keep that ordering.
const nativeHome = nativeThemeSource.slice(nativeThemeSource.indexOf('static void render_ubiquity_home('));
assert.ok(nativeHome.indexOf('draw_rounded_border(r, pill_x') <
          nativeHome.indexOf('SDL_RenderCopy(r, cover, &src, &cover_rect)'),
  'Native SDL must draw the crimson focus ring behind the cover texture');
assert.match(nativeThemeSource, /SDL_Rect footer = \{card.x, card.y \+ card.h - 40, card.w, 40\}/);
assert.match(nativeThemeSource, /item->state == STATE_AVAILABLE && item->format != FMT_CATALOG/);
assert.match(nativeThemeSource, /SDL_RenderDrawLine\(r, ax, ay, ax, ay \+ 13\)/);
assert.match(nativeThemeSource, /SDL_GetTicks\(\) % 2600u/,
  'SDL2 focus strobe should have matching 2.6-second timing');
assert.match(nativeThemeSource, /float burst = fmaxf\(expf\(-first \* first\), expf\(-second \* second\)\)/);
assert.match(nativeThemeSource, /201,\s*24,\s*74/,
  'SDL2 should use silk crimson gradient border');
assert.match(css, /\.ubq-square-art \{[^}]*width: 232px; height: 232px/);
assert.match(css, /\.ubq-cover-footer \{[^}]*height: 40px/);
assert.match(css, /\.ubq-cover-footer \{[^}]*position: absolute;[^}]*bottom: 0/);
assert.match(css, /\.ubq-shelf-track \{[^}]*gap: 20px/);
assert.match(css, /\.ubq-cover-download \{[^}]*position: absolute;[^}]*bottom: 8px/);
assert.match(fs.readFileSync(path.join(root, 'src/components/UbiquityHomeView.tsx'), 'utf8'), /index \* 252/);
assert.match(css, /\.ubq-game-cover \{[^}]*width: 232px; height: 232px/);
assert.match(home, /tv-hero-btn-primary/);
assert.match(home, /tv-description-toggle-inline|tv-hero-btn-secondary/);
assert.equal((home.match(/class="tv-card tv-card-selected"/g) || []).length, 1);
assert.ok(!render(React.createElement(HomeView, { ...homeProps, sidebarFocused: true })).includes('tv-card-selected'));
assert.match(render(React.createElement(HomeView, { ...homeProps, selected: 99 })), /<h1 class="tv-hero-info-appear">Game 4<\/h1>/);
for (const language of ['en', 'pl', 'fr', 'nl', 'de', 'es', 'it', 'zh', 'ru']) {
  assert.ok(render(React.createElement(GameCard, { ...cardProps, language })).includes(t(language, 'TXT_GAME_DETAILS')));
}
const empty = render(React.createElement(HomeView, { ...homeProps, games: [] }));
assert.ok(!empty.includes('NaN') && !empty.includes('undefined'));
assert.ok(empty.includes(t('en', 'TXT_NO_LATEST_GAMES')));
const detail = render(React.createElement(GameDetailModal, { game: games[1], catalog: games, language: 'en', onClose: noop, onSelectGame: noop, onTriggerAction: noop }));
const ubiquityDetails = render(React.createElement(UbiquityGameDetails, {
  game, catalog: games, language: 'en', onClose: noop, onTriggerAction: noop,
  onSelectGame: noop, actionIndex: 0, onActionIndexChange: noop
}));
assert.match(ubiquityDetails, /data-layout="ubiquity-detail"/);
assert.match(ubiquityDetails, /class="ubq-detail-game-title"/);
assert.match(ubiquityDetails, /class="ubq-detail-stats"/);
assert.match(ubiquityDetails, /class="ubq-detail-actions"/);
assert.match(ubiquityDetails, /Install size/);
assert.doesNotMatch(ubiquityDetails, /tv-detail-tabs|tv-neighbors|Achievement Progress|Time Played/);
assert.match(detail, /tv-detail-tabs/);
const appDetailSource = fs.readFileSync(path.join(root, 'src/App.tsx'), 'utf8');
assert.match(appDetailSource, /uiTheme === 'ubiquity' && \(/);
assert.match(appDetailSource, /<UbiquityGameDetails/);
assert.match(appDetailSource, /uiTheme !== 'ubiquity' && \(/);
const nativeDetailSource = fs.readFileSync(path.join(root, 'src/ui_ubiquity_details.h'), 'utf8');
assert.match(nativeDetailSource, /static void render_ubiquity_game_details/);
assert.match(nativeDetailSource, /render_cover_or_placeholder\(app, item, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, true\)/);
assert.match(fs.readFileSync(path.join(root, 'src/ui_game_details.h'), 'utf8'), /detail_ubiquity_options/);
assert.match(detail, /role="dialog"/);
assert.equal((detail.match(/role="tab"/g) || []).length, 4);
assert.equal((detail.match(/class="tv-neighbor(?: tv-neighbor-selected)?"/g) || []).length, 4);
assert.ok(detail.indexOf('tv-detail-tabs') < detail.indexOf('tv-detail-body'));
assert.ok(detail.includes(t('en', 'TXT_ACTION_UNAVAILABLE')));
const library = render(React.createElement(LibraryBrowser, { isEmulatorsView: false, catalog: games,
  language: 'en', onOpenGame: noop, onRefresh: noop, syncInProgress: false,
  filters: { sources_limited: false, sources: [], disabled_formats: [] } }));
assert.equal((library.match(/class="tv-library-tile /g) || []).length, 8);
// Every library system links to the release hub, but only exact matches link
// to verified binaries. Do not fabricate RetroArch core compatibility.
const emulatorSystems = COMPANIES.flatMap(brand => brand.consoles);
const sonyCompany = COMPANIES.find(company => company.name === 'Sony');
assert.ok(sonyCompany);
const gamesSony = consolesForSection(sonyCompany, false);
const emulatorsSony = consolesForSection(sonyCompany, true);
assert.equal(gamesSony.length, 6, 'Games/Roms keeps every Sony console');
assert.equal(emulatorsSony.length, 4, 'Emulators removes PS4 and PS5');
assert.deepEqual(emulatorsSony.map(system => system.name), [
  'PlayStation 1', 'PlayStation 2', 'PlayStation 3 (experimental)',
  'PlayStation Portable (PSP)'
]);
assert.ok(gamesSony.some(system => system.platform_id === 'sony-ps4'));
assert.ok(gamesSony.some(system => system.platform_id === 'sony-ps5'));
const availableEmulators = COMPANIES.flatMap(brand => consolesForSection(brand, true));
assert.equal(availableEmulators.length, 56);
assert.ok(availableEmulators.every(system =>
  system.platform_id !== 'sony-ps4' && system.platform_id !== 'sony-ps5'));
assert.equal(emulatorSystems.length, 58,
  'The current library has 58 system tiles, including the requested emulator groups');
assert.equal(EMULATOR_RELEASES_URL, 'https://github.com/PolDrive/PolDrive-Emulators/releases');
assert.equal(Object.keys(emulatorReleaseData.platforms).length, 9);
assert.equal(Object.keys(emulatorReleaseData.systems).length, 34,
  'The owner-designated 34 RetroArch systems should share one v4 asset');
assert.equal(new Set(Object.values(emulatorReleaseData.systems)
  .map(match => match.asset.url)).size, 1);
assert.ok(Object.values(emulatorReleaseData.systems).every(match =>
  match.engine === 'retroarch' && match.release === 'v4'
  && match.asset.name === 'PS5_RetroArch-v4.0.0-beta-4.zip'));
for (const title of Object.keys(emulatorReleaseData.systems)) {
  assert.ok(availableEmulators.some(system => system.name === title),
    'No RetroArch title may be outside the existing Emulators library: ' + title);
}

assert.equal(emulatorReleaseData.releases.length, 7);
assert.equal(emulatorReleaseData.releases.reduce((n, rel) => n + rel.assets.length, 0), 9);
for (const system of emulatorSystems) {
  const rendered = render(React.createElement(EmulatorReleaseView, {
    system, onBack: () => {}
  }));
  assert.doesNotMatch(rendered, /View release|Browse all PolDrive emulator releases/,
    'Emulator detail pages should not offer release-browsing buttons');
  const match = getEmulatorRelease(system);
  if (match) {
    assert.match(rendered, /Download release asset/);
    assert.ok(rendered.includes(match.asset.sha256));
    assert.ok(match.asset.url.startsWith(EMULATOR_RELEASES_URL + '/download/'));
    assert.match(match.asset.sha256, /^[a-f0-9]{64}$/);
    if (match.engine === 'retroarch') {
      assert.match(rendered, /Core availability and gameplay compatibility/);
      assert.ok(rendered.includes('PS5_RetroArch-v4.0.0-beta-4.zip'));
    }
  } else {
    assert.doesNotMatch(rendered, /Download release asset/);
    assert.match(rendered, /No verified emulator download is linked/);
  }
}
assert.equal(availableEmulators.filter(system => getEmulatorRelease(system)).length, 43,
  '9 dedicated systems + 34 user-designated RetroArch systems');
assert.equal(availableEmulators.filter(system => !getEmulatorRelease(system)).length, 13);
assert.equal(getEmulatorRelease(sonyCompany.consoles[3]), null, 'PS4 is not an emulator');
assert.equal(getEmulatorRelease(sonyCompany.consoles[4]), null, 'PS5 is not an emulator');

const assetRoot = 'https://raw.githubusercontent.com/PolDrive/PolDrive-archive/main/';
assert.equal(archiveLibraryUrl('/assets/library/companies/sony.png'),
  assetRoot + 'assets/library/companies/sony.png');
assert.equal(archiveLibraryUrl('../secrets.txt'), null);
assert.equal(archiveLibraryUrl('assets/library/logos/sony-ps5.png'),
  assetRoot + 'assets/library/logos/sony-ps5.png');
const allLibraryAssets = COMPANIES.flatMap(company => [
  company.artwork_path, ...company.consoles.flatMap(console =>
    [console.artwork_path, console.logo_path])
]).filter(Boolean);
assert.equal(allLibraryAssets.length, 119,
  'All existing company, emulator and console image paths must stream');
assert.ok(allLibraryAssets.every(uri => uri.startsWith(assetRoot + 'assets/library/')));
assert.match(library, /raw\.githubusercontent\.com\/PolDrive\/PolDrive-archive\/main\/assets\/library\/companies\/sony\.png/);
assert.match(library, /loading="lazy"/);
const importedCoverMap = JSON.parse(
  fs.readFileSync(path.join(root, 'src/archiveSourceCovers.json'), 'utf8'));
assert.ok(Object.keys(importedCoverMap).length >= 40000,
  'The public archive should match imported sources beyond the bundled PS4/PS5 games');
for (const path of Object.values(importedCoverMap)) {
  assert.match(path, /^assets\/covers\/[^?#]+\.art$/);
  assert.ok(!path.split('/').includes('..'), 'Archive cover paths must remain under assets/covers/');
}
assert.match(fs.readFileSync(path.join(root, 'src/sourceCatalogues.ts'), 'utf8'),
  /import\('\.\/archiveSourceCovers\.json'\)/);
assert.match(fs.readFileSync(path.join(root, 'src/sys_bridge.c'), 'utf8'),
  /SDL_strlcpy\(game->boxart_url, entry->boxart/);
assert.match(fs.readFileSync(path.join(root, 'src/ui_netflix.c'), 'utf8'),
  /artwork_get_media\(app, url, 95/);

// Console game routes must forward the current theme instead of falling back to Netflix.
const librarySource = fs.readFileSync(path.join(root, 'src/components/LibraryBrowser.tsx'), 'utf8');
const appSourceForLibrary = fs.readFileSync(path.join(root, 'src/App.tsx'), 'utf8');
assert.match(librarySource, /<HomeView games=\{gamesFor\(console\)\} uiTheme=\{uiTheme\}/);
assert.match(librarySource, /tv-library-ubiquity-games/);
assert.equal((appSourceForLibrary.match(/uiTheme=\{uiTheme\}/g) || []).length >= 4, true,
  'The selected theme must be supplied to Home, Games/Roms and Emulators routes');
assert.match(css, /\.tv-theme-ubiquity \.tv-library-ubiquity-games \.tv-breadcrumb/);
const nativeLibraryPath = fs.readFileSync(path.join(root, 'src/ui_netflix.c'), 'utf8');
assert.match(nativeLibraryPath, /static bool ubiquity_carousel_enabled\(WolodriveApp \*app\)/);
assert.match(nativeLibraryPath, /app->active_row == VIEW_GAMES\)\s*return app->library_depth == LIBRARY_GAMES/);
assert.match(nativeLibraryPath, /if \(ubiquity_carousel_enabled\(app\)\) \{\s*render_ubiquity_home\(app\)/);
assert.match(nativeLibraryPath, /if \(ubiquity_carousel_enabled\(app\)\) \{\s*enum \{ UBQ_CARD_W = 232/);

const sidebarProps = {
  currentView: 'home', onSelectView: noop, language: 'pl',
  onSelectLanguage: noop, languageMenuOpen: false, setLanguageMenuOpen: noop,
  isSidebarFocused: false
};
const collapsedSidebar = render(React.createElement(Sidebar, sidebarProps));
assert.match(collapsedSidebar, /tv-sidebar-collapsed/);
assert.match(collapsedSidebar, /data-state="collapsed"/);
assert.ok(!collapsedSidebar.includes('tv-sidebar-dim-active'));
assert.ok(collapsedSidebar.includes('tv-brand-mark'));
assert.ok(collapsedSidebar.includes('olDrive'));
const focusedSidebar = render(React.createElement(Sidebar, { ...sidebarProps, isSidebarFocused: true }));
assert.match(focusedSidebar, /tv-sidebar-expanded/);
assert.match(focusedSidebar, /data-state="expanded"/);
assert.match(focusedSidebar, /tv-sidebar-dim-active/);
assert.ok(focusedSidebar.includes(t('pl', 'TXT_NAV_HOME')));
assert.ok(focusedSidebar.includes(t('pl', 'TXT_NAV_GAMES')));
assert.ok(focusedSidebar.includes(t('pl', 'TXT_NAV_SETTINGS')));
const settingsSidebar = render(React.createElement(Sidebar, { ...sidebarProps, currentView: 'settings' }));
assert.match(settingsSidebar, /tv-sidebar-settings/);
assert.match(settingsSidebar, /data-state="collapsed"/);
assert.ok(!settingsSidebar.includes('tv-sidebar-dim-active'), 'Settings keeps the navigation collapsed until focused, as in Backup-version-1');
const languageSidebar = render(React.createElement(Sidebar, { ...sidebarProps, languageMenuOpen: true }));
assert.match(languageSidebar, /tv-sidebar-expanded/);
assert.match(languageSidebar, /aria-expanded="true"/);
assert.match(languageSidebar, /id="tv-language-options"/);
assert.equal(declaration('.tv-nav-item', 'height'), 92, 'Web sidebar row height must match native');
assert.equal(declaration('.tv-navigation', 'top'), 148, 'Web navigation start must match native');
assert.equal(declaration('.tv-sidebar-collapsed', 'width'), 92, 'Native collapsed rail is 92px');
assert.equal(declaration('.tv-sidebar-expanded', 'width'), 272, 'Native expanded rail is 272px');
assert.match(css, /\.tv-sidebar \{[^}]*transition: width/);
assert.match(css, /\.tv-sidebar-dim-active \{[^}]*opacity: 1/);
// Motion contract: web carousel follows native damped motion; both rails slide
// their active marker instead of teleporting a static pseudo-element.
const homeSource = fs.readFileSync(path.join(root, 'src/components/HomeView.tsx'), 'utf8');
const appSource = fs.readFileSync(path.join(root, 'src/App.tsx'), 'utf8');
const heroSource = fs.readFileSync(path.join(root, 'src/components/HeroBanner.tsx'), 'utf8');
assert.match(native, /int row_h = 92;/);
assert.match(native, /sidebar_indicator_y \+=/);
assert.match(native, /home_grid_anim < 1\.0f/);
assert.match(css, /@keyframes tvGridEnter/);
assert.match(homeSource, /Math\.exp\(-speed \* dt\)/);
assert.match(homeSource, /prefers-reduced-motion: reduce/);
assert.match(appSource, /selectedHomeCols\[activeHomeCategory\]/);
assert.match(appSource, /lastGamepadPress\.current/);
assert.match(heroSource, /function ArtworkCrossfade/);
assert.match(css, /\.tv-hero-incoming-loaded/);
assert.match(css, /\.tv-nav-marker \{/);
assert.match(collapsedSidebar, /class="tv-nav-marker"/);
assert.match(collapsedSidebar, /translate3d\(0, 92px, 0\)/);
assert.match(settingsSidebar, /translate3d\(0, 644px, 0\)/);
// PS5 glass/pills: parallel native and web styles, no changes to focus semantics.
for (const helper of ['fill_rounded_rect', 'draw_rounded_border', 'draw_rounded_glow']) {
  assert.match(native, new RegExp('static void ' + helper + '\\('));
}
assert.match(native, /draw_hero_metadata_pill\(app, hero->id_code/);
assert.match(native, /int y = app->ui_theme == UI_THEME_UBIQUITY \? -500 : 470, h = 50;/);
assert.match(css, /\.tv-hero-btn-primary \{[^}]*linear-gradient\(135deg, #ff1e27/);
assert.equal(declaration('.tv-hero-btn', 'border-radius'), 25);
assert.equal(declaration('.tv-meta-pill', 'height'), 32);
assert.equal(declaration('.tv-card', 'border-radius'), 18);
assert.match(css, /\.tv-nav-marker \{[^}]*border-radius: 14px/);
assert.match(home, /<span>olDrive<\/span>/);
assert.doesNotMatch(home, /Poldrive/);
const nativeSettings = fs.readFileSync(path.join(root, 'src/ui_settings.h'), 'utf8');
const persistedTheme = fs.readFileSync(path.join(root, 'src/settings.c'), 'utf8');
const componentSettings = render(React.createElement(SettingsView, {
  filters: { sources_limited: false, sources: [], disabled_formats: [] },
  onChangeFilters: noop, availableSources: [], language: 'en',
  uiTheme: 'ubiquity', onChangeTheme: noop
}));
const componentDefault = render(React.createElement(SettingsView, {
  filters: { sources_limited: false, sources: [], disabled_formats: [] },
  onChangeFilters: noop, availableSources: [], language: 'en',
  uiTheme: 'netflix', onChangeTheme: noop
}));
assert.match(componentSettings, /PolDrive System/, 'Restored Settings dashboard must display the system summary');
assert.match(componentSettings, /Network|Connection|Połączenie/i,
  'Restored Settings dashboard must include network status');
assert.match(t('en', 'TXT_DOWNLOADS_HELP'), /download/i);
assert.match(t('en', 'TXT_SYS_VERSION'), /version/i);
const { DownloadsView } = require('../src/components/DownloadsView.tsx');
const restoredDownloads = render(React.createElement(DownloadsView,
  { catalog: [], language: 'en', uiTheme: 'ubiquity' }));
assert.match(restoredDownloads, /Active/i);
assert.match(restoredDownloads, /Completed/i);
assert.match(restoredDownloads, /Errors/i);
assert.match(appSource, /<DownloadsView[\s\S]*?catalog=\{catalog\}/);
assert.match(appSource, /<FileBrowser/, 'Restoring Downloads must not remove the File Manager');
assert.match(appSource, /catalogCount=\{catalog\.length\}/);
assert.match(css, /\.ubq-category-select button \{[^}]*border-radius:\s*16px/,
  'Bottom category buttons must retain the rounded Backup-version-1 appearance');
assert.match(fs.readFileSync(path.join(root, 'src/components/UbiquityHomeView.tsx'), 'utf8'),
  /<span className="tv-meta-pill">\{game\.id_code\}<\/span>/,
  'Ubiquity hero must show game metadata in oval pills');
assert.match(componentSettings, /\(u\)biquity/);
assert.match(componentSettings, /Netflix \/ Glass/);
assert.match(componentSettings, /aria-pressed="true"/);
assert.match(componentDefault, /tv-theme-option-active/);
assert.match(native, /render_ubiquity_fades\(r\)/);
assert.match(native, /app->ui_theme == UI_THEME_UBIQUITY/);
assert.match(nativeSettings, /theme_save\(app\)/);
assert.match(nativeSettings, /Appearance \/ Wygląd/);
assert.match(persistedTheme, /WOLODRIVE_THEME_1/);
assert.match(css, /\.tv-theme-ubiquity \.tv-hero-backdrop/);
assert.match(css, /\.tv-theme-ubiquity \.tv-nav-marker/);
assert.match(ubiquity, /ubq-logo-main/);
assert.match(ubiquity, /ubq-glass-capsule/);
assert.doesNotMatch(ubiquity, /PS5 LIBRARY/);
const ubiquityCollapsedSidebar = render(React.createElement(Sidebar, { ...sidebarProps, uiTheme: 'ubiquity', isSidebarFocused: false }));
const ubiquityExpandedSidebar = render(React.createElement(Sidebar, { ...sidebarProps, uiTheme: 'ubiquity', isSidebarFocused: true }));
assert.match(ubiquityCollapsedSidebar, /ubq-pd-bubble-collapsed/);
assert.match(ubiquityExpandedSidebar, /ubq-pd-bubble-expanded/);
assert.doesNotMatch(collapsedSidebar, /ubq-pd-bubble/, 'Netflix sidebar keeps its classic layout');
assert.match(native, /draw_poldrive_premium_logo/);
assert.match(native, /255,\s*51,\s*102,\s*255/);
console.log('PASS: layout, Netflix motion, Ubiquity switch, web SSR and native appearance contracts');

