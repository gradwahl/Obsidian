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
const { TV_LAYOUT, fitViewport, fitEdgeToEdgeViewport } = require('../src/layout.ts');
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
  assert.match(url, /^https:\/\/raw\.githubusercontent\.com\/gradwahl\/Obsidian-archive\/main\/assets\/covers\/Sony\/PlayStation%20[45]\//);
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
  const edge = fitEdgeToEdgeViewport(width, height);
  assert.equal(edge.left, 0, 'The sidebar must sit against the left screen edge');
  assert.equal(edge.top, top, 'The existing TV vertical fitting must be preserved');
  assert.equal(edge.scale, scale, 'Game cards must retain their aspect ratio');
  assert.ok(Math.abs(edge.stageWidth * scale - Math.max(width, 1920 * scale)) < .01,
    'The fitted stage must reach the full right screen edge without black gutters');
}
const widescreen = fitEdgeToEdgeViewport(2048, 1016);
assert.equal(widescreen.left, 0);
assert.ok(Math.abs(widescreen.stageWidth * widescreen.scale - 2048) < .01);
assert.match(fs.readFileSync(path.join(root, 'src/glass.css'), 'utf8'),
  /width: var\(--tv-bleed-width, 1920px\)/,
  'The game wallpaper must extend across the fitted widescreen stage');
const glassLiquidCSS = fs.readFileSync(path.join(root, 'src/glass-liquid.css'), 'utf8');
const glassLiquidComponent = fs.readFileSync(path.join(root, 'src/components/GlassLiquid.tsx'), 'utf8');
const nativeGlass = fs.readFileSync(path.join(root, 'src/ui_glass.h'), 'utf8');
assert.match(glassLiquidCSS, /blur\(12px\) saturate\(138%\)/,
  'The shared bubble must let artwork show through real 12px backdrop blur');
assert.match(glassLiquidCSS, /rgba\(92,98,116,\.27\)/,
  'Neutral capsules should use a translucent graphite-purple fill');
assert.match(glassLiquidCSS, /\.glass-pill\.sliding-highlight \{\s*position:absolute;\s*top:0;\s*left:0;/,
  'The glass highlight must remain absolutely positioned so its translate does not double the flex offset');
assert.match(glassLiquidCSS, /@keyframes gel-drift-a/);
assert.match(glassLiquidCSS, /@keyframes gel-drift-b/);
assert.match(glassLiquidCSS, /@keyframes gel-drift-c/);
assert.match(glassLiquidCSS, /@keyframes gel-rise/);
assert.match(glassLiquidCSS, /@keyframes gel-press/);
assert.match(glassLiquidCSS, /@media \(prefers-reduced-motion:reduce\)/,
  'Reduced motion must keep static depth without movement');
assert.match(glassLiquidComponent, /IntersectionObserver/,
  'Off-screen pills should suspend moving interior layers');
assert.match(nativeGlass, /SDL_CreateTextureFromSurface/,
  'Native SDL2 must reuse cached pre-softened liquid light textures');
assert.match(nativeGlass, /SDL_GetTicks\(\)/,
  'Native liquid movement must follow elapsed real time');
assert.match(fs.readFileSync(path.join(root, 'src/main.tsx'), 'utf8'),
  /import '\.\/glass-liquid\.css'/,
  'All real pills should share the glass surface stylesheet');
const mobileCSS = fs.readFileSync(path.join(root, 'src/mobile.css'), 'utf8');
const mobileViewport = fs.readFileSync(path.join(root, 'src/components/NativeViewport.tsx'), 'utf8');
assert.match(mobileViewport, /viewport\.width <= 1024/, 'Small screens need an unscaled layout');
assert.match(mobileViewport, /mobile \? 'none' : `scale/, 'Phone buttons must not shrink with TV-scale');
assert.match(fs.readFileSync(path.join(root, 'src/main.tsx'), 'utf8'), /import '\.\/mobile\.css'/);
assert.match(mobileCSS, /\.tv-stage-mobile \.tv-sidebar \.tv-nav-item[\s\S]*?min-width:48px/,
  'Mobile navigation needs 48px-wide touch targets');
assert.match(mobileCSS, /\.tv-stage-mobile \.tv-theme-ubiquity \.ubq-feature-title/,
  'Game title should reflow rather than shrink to phone-scale');
assert.match(mobileCSS, /\.tv-stage-mobile \.ubq-category-select \{[\s\S]*?overflow-x:auto/,
  'All game categories must be horizontally swipeable');
assert.match(mobileCSS, /\.tv-stage-mobile \.tv-theme-ubiquity \.tv-sidebar\.glass-surface \{[\s\S]*?backdrop-filter:none!important/,
  'Fixed-position language and logo buttons must not anchor to the bottom navbar');
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
assert.match(missing, /Obsidian/);
const games = Array.from({ length: 5 }, (_, index) => ({ ...game, id_code: `PPSA0000${index}`, title: `Game ${index}` }));
const homeProps = { games, language: 'en', selected: 1, sidebarFocused: false, onSelect: noop, onOpen: noop, onRefresh: noop, syncInProgress: false };
const home = render(React.createElement(NativeViewport, null, React.createElement(HomeView, homeProps)));
assert.match(home, /width:1920px;height:1080px/);
assert.match(home, /<h1 class="ubq-feature-title">Game 1<\/h1>/);
assert.match(home, /2026/);
assert.equal((home.match(/class="tv-meta-pill glass-pill glass-pill--sm"/g) || []).length, 6,
  'Source, game id, version, format, size and year render as separate pills');
const catalogueModule = require('../src/catalog.ts');
const exampleFilters = { sources_limited: false, sources: [], disabled_formats: [] };
const categoriesOnce = catalogueModule.getHomeCategoryLists(INITIAL_CATALOG, exampleFilters, 128);
for (const id of ['ps5', 'ps4', 'popular', 'newest', 'top_rated']) {
  assert.deepEqual(
    categoriesOnce[id].map(item => item.id_code),
    catalogueModule.getHomeCategoryGames(INITIAL_CATALOG, exampleFilters, id, 128)
      .map(item => item.id_code),
    'Fast shared category selection must preserve ' + id
  );
}
// Home rows must not mount 128 covers times five categories when browsing on PS5.
const largeRows = Array.from({ length: 128 }, (_, index) =>
  ({ ...game, id_code: 'PPSA' + String(index).padStart(5, '0'), title: 'Row ' + index }));
const multiRows = ['ps5','ps4','popular','newest','top_rated'].map(id =>
  ({ id, titleKey: 'TXT_CAT_PS5', games: largeRows }));
const reducedHome = render(React.createElement(HomeView, {
  ...homeProps, categories: multiRows, activeCategoryIndex: 0, selected: 70
}));
const visibleCards = (reducedHome.match(/<button type="button" class="ubq-game-cover/g) || []).length;
assert.ok(visibleCards > 0 && visibleCards < 120,
  'PS5 category view should virtualize offscreen covers (got ' + visibleCards + ')');
assert.match(reducedHome, /Row 70/, 'Focused cover must stay in the rendered slice');

// The same expanded-grid geometry must remain bounded for very large libraries.
const { VirtualGameGrid, gridWindow } = require('../src/components/VirtualGameGrid.tsx');
const hugeLibrary = Array.from({ length: 20000 }, (_, i) => ({ ...game, title: 'Virtual ' + i, id_code: 'TEST' + i }));
const virtualGrid = render(React.createElement(VirtualGameGrid, {
  games: hugeLibrary, selected: 12000, language: 'en', sidebarFocused: false, onSelect: noop, onOpen: noop
}));
assert.ok((virtualGrid.match(/class="tv-card /g) || []).length <= 30);
assert.match(virtualGrid, /Virtual 12000/);
const range = gridWindow(20000, 392 * 500, 800);
assert.equal(range.start, 499 * 6);
assert.ok(range.end - range.start <= 30);
assert.ok(range.before > 0 && range.after > 0);
// Empty and final windows retain all valid records without phantom cards.
assert.deepEqual(gridWindow(0, 0, 800), { start: 0, end: 0, before: 0, after: 0 });
assert.equal(gridWindow(20000, Math.floor(19999 / 6) * 392, 800).end, 20000);
assert.deepEqual(gridWindow(0, 999999, 800), { start: 0, end: 0, before: 0, after: 0 });

const scoredHome = render(React.createElement(HomeView, {
  ...homeProps, games: games.map(g => ({ ...g, rating_score: 8.7, download_count: 140 }))
}));
assert.equal((scoredHome.match(/class="tv-meta-pill glass-pill glass-pill--sm"/g) || []).length, 6);
const ubiquity = render(React.createElement(HomeView, {
  ...homeProps, uiTheme: 'ubiquity'
}));
assert.match(ubiquity, /data-layout="ubiquity"/);
assert.match(ubiquity, /ubq-cinematic-background/);
const verifiedBackdrop = render(React.createElement(HomeView, {
  ...homeProps, games: [{ ...game, boxart_url: '/verified-art.jpg',
    cover_local_path: '/assets/covers/PPSA00001.cover' }], selected: 0
}));
assert.match(verifiedBackdrop, /class="ubq-backdrop-current"[^>]*src="\/verified-art\.jpg"/,
  'Cinematic background must choose verified artwork before missing legacy .cover files');
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
const longDefault = render(React.createElement(HomeView,
  { ...homeProps, games: largeCatalog, selected: 500, uiTheme: 'ubiquity' }));
assert.ok((longDefault.match(/class="ubq-game-cover/g) || []).length <= 21,
  'Default interface must window long single-console catalogues');
assert.match(longDefault, /501 \/ 1000/);

assert.doesNotMatch(ubiquity, /ubq-cover-line/,
  'The selected Ubiquity tile must not add a second solid white stripe at its bottom');
assert.match(css, /\.ubq-game-cover-selected::after \{/,
  'Selected artwork and title need a single inset white border');
assert.match(css, /@keyframes ubqFocusStrobe/,
  'The selected tile should have a glowing strobe animation');
assert.match(css, /\.ubq-game-cover-selected \{[^}]*animation: ubqFocusStrobe 2\.6s/);
assert.match(css, /@media \(prefers-reduced-motion: reduce\)/);
assert.doesNotMatch(ubiquity, /tv-hero-btn-primary|tv-card-footer/);
assert.equal((home.match(/class="ubq-game-cover/g) || []).length, games.length);
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
assert.match(home, /DOWNLOAD \/ INSTALL/);
assert.match(home, /More info/);
assert.equal((home.match(/class="ubq-game-cover ubq-game-cover-selected"/g) || []).length, 1);
assert.ok(!render(React.createElement(HomeView, { ...homeProps, sidebarFocused: true })).includes('ubq-game-cover-selected'));
assert.match(render(React.createElement(HomeView, { ...homeProps, selected: 99 })), /<h1 class="ubq-feature-title">Game 4<\/h1>/);
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
assert.match(appDetailSource, /\{detailGame && \(/);
assert.match(appDetailSource, /<UbiquityGameDetails/);
assert.doesNotMatch(appDetailSource, /GameDetailModal|obsidian-ui-theme|changeUiTheme/);
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
assert.equal(EMULATOR_RELEASES_URL, 'https://github.com/gradwahl/Obsidian-Emulators/releases');
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
  assert.doesNotMatch(rendered, /View release|Browse all Obsidian emulator releases/,
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

const assetRoot = 'https://raw.githubusercontent.com/gradwahl/Obsidian-archive/main/';
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
assert.match(library, /raw\.githubusercontent\.com\/gradwahl\/Obsidian-archive\/main\/assets\/library\/companies\/sony\.png/);
assert.match(library, /loading="lazy"/);
const importedCoverMap = JSON.parse(
  fs.readFileSync(path.join(root, 'src/archiveSourceCovers.json'), 'utf8'));
assert.ok(Object.keys(importedCoverMap).length >= 40000,
  'The public archive should match imported sources beyond the bundled PS4/PS5 games');
for (const path of Object.values(importedCoverMap)) {
  assert.match(path, /^assets\/covers\/[^?#]+\.art$/);
  assert.ok(!path.split('/').includes('..'), 'Archive cover paths must remain under assets/covers/');
}
const pagedBrowserSource = fs.readFileSync(path.join(root, 'src/sourceCatalogues.ts'), 'utf8');
assert.match(pagedBrowserSource, /obsidian-pages\//);
assert.match(pagedBrowserSource, /manifest\.json/);
assert.doesNotMatch(pagedBrowserSource, /import\('\.\/archiveSourceCovers\.json'\)/,
  'Browser startup should not execute a 6MB artwork JSON module');
assert.match(fs.readFileSync(path.join(root, 'scripts/generate_browser_catalog_pages.cjs'), 'utf8'),
  /archiveSourceCovers\.json/,
  'Build-time artwork matching must preserve source cover associations');
assert.match(fs.readFileSync(path.join(root, 'src/sys_bridge.c'), 'utf8'),
  /SDL_strlcpy\(game->boxart_url, entry->boxart/);
assert.match(fs.readFileSync(path.join(root, 'src/ui_netflix.c'), 'utf8'),
  /artwork_get_media\(app, url, 95/);

// All game routes use the sole Ubiquity interface, including existing saved preferences.
const librarySource = fs.readFileSync(path.join(root, 'src/components/LibraryBrowser.tsx'), 'utf8');
assert.match(librarySource, /tv-library-ubiquity-games/);
assert.match(librarySource, /<HomeView games=\{gamesFor\(console\)\}/);
const appSourceForLibrary = fs.readFileSync(path.join(root, 'src/App.tsx'), 'utf8');
assert.match(appSourceForLibrary, /className="tv-app tv-theme-ubiquity"/);
assert.doesNotMatch(appSourceForLibrary, /obsidian-ui-theme|themeChoiceIndex|changeUiTheme/);
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
assert.ok(collapsedSidebar.includes('bsidian'));
assert.match(collapsedSidebar, /ubq-pd-bubble-collapsed/,
  'The animated O emblem must render on the default Ubiquity sidebar');
assert.match(collapsedSidebar, /ubq-pd-ribbon-1/,
  'The O emblem needs its moving crimson ribbons');
assert.match(css, /@keyframes ubqCrimsonRibbon1/,
  'The crimson emblem animation must be included in the web CSS');
assert.match(css, /\.ubq-cinematic-background \{[^}]*left: -92px; width: 1920px; height: 1080px/,
  'Cinematic artwork must extend behind the rail to fill the entire 1920x1080 screen');
const focusedSidebar = render(React.createElement(Sidebar, { ...sidebarProps, isSidebarFocused: true }));
assert.match(focusedSidebar, /ubq-pd-bubble-expanded/,
  'The O emblem must unfold to Obsidian when sidebar receives focus');
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
assert.equal(declaration('.tv-nav-item', 'height'), 44, 'Web sidebar row height must match native');
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
assert.match(native, /int row_h = 48;/);
assert.match(native, /sidebar_indicator_y \+=/);
assert.match(native, /home_grid_anim < 1\.0f/);
assert.match(css, /@keyframes tvGridEnter/);
assert.match(homeSource, /<UbiquityHomeView/);
assert.match(ubiquitySource, /prefers-reduced-motion: reduce/);
assert.match(appSource, /selectedHomeCols\[activeHomeCategory\]/);
assert.match(appSource, /lastGamepadPress\.current/);
assert.match(heroSource, /function ArtworkCrossfade/);
assert.match(heroSource, /<strong>O<\/strong>/, 'Hero banner must use the Obsidian O mark');
assert.match(heroSource, /<span>bsidian<\/span>/, 'Hero banner must spell Obsidian');
assert.match(native, /draw_text\(app, app->font_logo, "O"/, 'Native logo must use the Obsidian O mark');
assert.match(native, /draw_text\(app, app->font_logo, "bsidian"/, 'Native logo must spell Obsidian');

assert.match(css, /\.tv-hero-incoming-loaded/);
assert.match(css, /\.tv-nav-marker \{/);
assert.match(collapsedSidebar, /glass-pill glass-pill--active sliding-highlight tv-nav-marker/);
assert.match(collapsedSidebar, /data-highlight-key="home"/);
assert.match(settingsSidebar, /data-highlight-key="settings"/);
// PS5 glass/pills: parallel native and web styles, no changes to focus semantics.
for (const helper of ['fill_rounded_rect', 'draw_rounded_border', 'draw_rounded_glow']) {
  assert.match(native, new RegExp('static void ' + helper + '\\('));
}
assert.match(native, /draw_hero_metadata_pill\(app, hero->id_code/);
assert.match(native, /int y = app->ui_theme == UI_THEME_UBIQUITY \? -500 : 470, h = 50;/);
assert.match(fs.readFileSync(path.join(root, 'src/glass.css'), 'utf8'), /--pill-active-1: #ff1e27/);
assert.equal(declaration('.tv-hero-btn', 'border-radius'), 25);
assert.equal(declaration('.tv-meta-pill', 'height'), 30);
assert.equal(declaration('.tv-card', 'border-radius'), 18);
assert.match(fs.readFileSync(path.join(root, 'src/glass.css'), 'utf8'), /border-radius: 9999px/);
assert.match(home, /class="ubq-system-label ubq-glass-capsule"/,
  'The Obsidian-branding Obsidian glass capsule should replace the old PS5 Library label');
assert.match(home, /class="ubq-logo-p">O<\/b>/);
assert.doesNotMatch(home, /PS5 LIBRARY/);
assert.match(home, /aria-label="Obsidian"/);
const glassCss = fs.readFileSync(path.join(root, 'src/glass.css'), 'utf8');
assert.match(glassCss, /\.tv-theme-ubiquity \.tv-sidebar\.glass-surface\.tv-sidebar-collapsed \{ width: 64px; \}/,
  'The collapsed Ubiquity rail must be 64px wide');
assert.match(glassCss, /\.tv-theme-ubiquity \.tv-sidebar-collapsed \.tv-nav-item \{[^}]*width: 44px;[^}]*height: 44px;[^}]*border-radius: 50%/s,
  'Collapsed navigation bubbles must be actual 44px circles');
assert.match(glassCss, /\.tv-theme-ubiquity \.tv-sidebar-collapsed \.tv-nav-item > \.tv-nav-label \{[^}]*display: none/s,
  'Invisible labels must not steal flex space from centered collapsed icons');
assert.match(glassCss, /\.tv-theme-ubiquity \.tv-sidebar-collapsed \.tv-nav-icon \{[^}]*justify-content: center/s,
  'The icon container must center its SVG within the circular button');
assert.match(glassCss, /\.tv-theme-ubiquity \.tv-sidebar-collapsed \.tv-language-trigger \{[^}]*width: 44px;[^}]*height: 44px;[^}]*border-radius: 11px/s,
  'The language trigger must be a compact square');
assert.match(glassCss, /\.tv-theme-ubiquity \.tv-sidebar-collapsed \.tv-nav-item:last-child \{[^}]*margin-top: auto/s,
  'Settings stays pinned to the bottom of the narrow rail');
assert.match(native, /SIDEBAR_COLLAPSED_W = 64, RAIL_WIDTH = 92, SIDEBAR_EXPANDED_W = 220/,
  'Native and web collapsed/expanded sidebar sizes should agree');
assert.match(native, /\(SDL_Rect\)\{10, 8, 44, 44\}/,
  'Native Ubiquity language control should be the same 44px square');
assert.match(glassCss, /\.tv-theme-ubiquity \.ubq-pd-bubble-collapsed \{ top: 80px;/,
  'The collapsed O mark should sit lower under the language button');
assert.match(glassCss, /\.tv-theme-ubiquity \.ubq-pd-bubble-expanded \{ top: 77px;/,
  'The expanded Obsidian mark must also clear the language control');
assert.match(glassCss, /\.tv-theme-ubiquity \.tv-sidebar\.glass-surface::after \{[^}]*content: none/s,
  'The bright white glass sheen at the top of the sidebar must be removed');
assert.match(glassCss, /\.tv-theme-ubiquity \.tv-sidebar \.tv-language \{ z-index: 30; \}/,
  'Language stacking context must paint above the O emblem');
assert.match(glassCss, /\.tv-theme-ubiquity \.tv-sidebar \.tv-language-options \{[^}]*width: 100%;[^}]*max-width: 100%;/s,
  'Language menu must fit inside the expanded rail rather than retain a fixed 244px width');
assert.match(native, /LANGUAGE_MENU = \{8, 92, SIDEBAR_EXPANDED_W - 16, LANG_COUNT \* 44 \+ 16\}/,
  'Native language menu must also remain within the expanded sidebar');
assert.match(glassCss, /\.tv-theme-ubiquity \.tv-sidebar \.ubq-pd-bubble \{ z-index: 3; \}/,
  'Obsidian branding must stay behind the language dropdown');
assert.match(native, /by = expanded \? 77 : 80/,
  'Native logo placement must match the web sidebar');
assert.match(fs.readFileSync(path.join(root, 'src/ui_glass.h'), 'utf8'),
  /if\(state!=GLASS_PANEL_UBQ\)/,
  'Native Ubiquity panel must not draw the bright upper-half glass sheen');
assert.match(glassCss, /\.tv-theme-ubiquity \.sliding-highlight\.glass-pill--active \{/,
  'Selected capsules need their own translucent multi-stop bevel gradient');
assert.match(glassCss, /rgba\(255, 136, 166, \.60\)/);
assert.match(glassCss, /\.tv-theme-ubiquity \.ubq-feature-title \{[^}]*font-size: 84px/s,
  'The featured game title uses the original larger Obsidian-branding sizing');
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
  uiTheme: 'ubiquity', onChangeTheme: noop
}));
assert.match(componentSettings, /Obsidian System/, 'Restored Settings dashboard must display the system summary');
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
const glassCSS = fs.readFileSync(path.join(root, 'src/glass.css'), 'utf8');
const highlightSource = fs.readFileSync(path.join(root, 'src/components/SlidingHighlight.tsx'), 'utf8');
assert.match(css, /\.ubq-category-select \{[^}]*margin-top: auto/);
assert.match(css, /\.ubq-category-select \{[^}]*overflow: visible/);
assert.match(glassCSS, /--pill-bg-top: rgba\(28,28,38,\.72\)/);
assert.match(glassCSS, /--pill-active-1: rgba\(229,9,20,\.35\)/);
assert.match(glassCSS, /mask-composite: exclude/);
assert.match(highlightSource, /flight.current\?\.cancel\(\)/);
assert.match(highlightSource, /ResizeObserver/);
assert.match(highlightSource, /document.fonts\?\.ready/);
assert.match(ubiquitySource, /<SlidingHighlight activeKey=\{String\(activeCategoryIndex\)\} axis="x"/);
assert.match(fs.readFileSync(path.join(root, 'src/components/Sidebar.tsx'), 'utf8'), /<SlidingHighlight activeKey=\{language\}/);
assert.match(fs.readFileSync(path.join(root, 'src/components/UbiquityHomeView.tsx'), 'utf8'),
  /<span className="tv-meta-pill glass-pill glass-pill--sm"><GlassLiquid \/>\{game.id_code\}<\/span>/);
for (const markup of [componentSettings, componentDefault]) {
  assert.doesNotMatch(markup, /Appearance|Wygląd|Netflix \/ Glass|tv-theme-option/);
  assert.match(markup, /role="button" tabindex="0"/);
}
assert.match(native, /render_ubiquity_fades\(r\)/);
assert.doesNotMatch(nativeSettings, /theme_save|Appearance|Netflix \/ Glass/);
assert.match(nativeSettings, /settings_target\(1,5\)/);
assert.match(nativeSettings, /settings_selection, 5/);
assert.doesNotMatch(persistedTheme, /WOLODRIVE_THEME_1|theme_load|theme_save/);
assert.match(fs.readFileSync(path.join(root, 'src/sys_bridge.c'), 'utf8'),
  /app->ui_theme = UI_THEME_UBIQUITY/);
assert.match(css, /\.tv-theme-ubiquity \.tv-hero-backdrop/);
assert.match(css, /\.tv-theme-ubiquity \.tv-nav-marker/);
console.log('PASS: layout, Ubiquity default, web SSR and native Settings contracts');
