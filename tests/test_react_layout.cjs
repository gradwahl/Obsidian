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
const { LibraryBrowser } = require('../src/components/LibraryBrowser.tsx');
const { isAvailableCatalogSource } = require('../src/catalog.ts');
const sources = [
  { id: 'evox-coreos', url: 'https://example.test/broken.json' },
  { name: 'evoX-CoreOS' },
  { sourceUrl: 'https://example.test/evoX-CoreOS/catalog.json' },
  ...['dlps', 'pippo', 'pfs', 'zer0'].map(id => ({ id, url: `https://example.test/${id}.json` }))
];
assert.deepEqual(sources.filter(isAvailableCatalogSource).map(source => source.id), ['dlps', 'pippo', 'pfs', 'zer0']);
const { t } = require('../src/translations.ts');
const native = fs.readFileSync(path.join(root, 'src/ui_netflix.c'), 'utf8');
const css = fs.readFileSync(path.join(root, 'src/native.css'), 'utf8');
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
assert.match(missing, /WOLODRIVE/);
const games = Array.from({ length: 5 }, (_, index) => ({ ...game, id_code: `PPSA0000${index}`, title: `Game ${index}` }));
const homeProps = { games, language: 'en', selected: 1, sidebarFocused: false, onSelect: noop, onOpen: noop, onRefresh: noop, syncInProgress: false };
const home = render(React.createElement(NativeViewport, null, React.createElement(HomeView, homeProps)));
assert.match(home, /width:1920px;height:1080px/);
assert.match(home, /<h1>Game 1<\/h1>/);
assert.match(home, /2026-10-19/);
assert.equal((home.match(/class="tv-card tv-card-selected"/g) || []).length, 1);
assert.ok(!render(React.createElement(HomeView, { ...homeProps, sidebarFocused: true })).includes('tv-card-selected'));
assert.match(render(React.createElement(HomeView, { ...homeProps, selected: 99 })), /<h1>Game 4<\/h1>/);
const largeGames = Array.from({ length: 5000 }, (_, index) => ({
  ...game, id_code: `EXT${index}`, title: `Large game ${index}`
}));
const largeMarkup = render(React.createElement(HomeView, { ...homeProps, games: largeGames, selected: 3500 }));
assert.match(largeMarkup, /<h1>Large game 3500<\/h1>/);
assert.equal((largeMarkup.match(/class="tv-card /g) || []).length, 25,
  'Large Games/Roms lists must only mount nearby cards');
assert.match(largeMarkup, /3501 \/ 5000/);

for (const language of ['en', 'pl', 'fr', 'nl', 'de', 'es', 'it', 'zh', 'ru']) {
  assert.ok(render(React.createElement(GameCard, { ...cardProps, language })).includes(t(language, 'TXT_GAME_DETAILS')));
}
const empty = render(React.createElement(HomeView, { ...homeProps, games: [] }));
assert.ok(!empty.includes('NaN') && !empty.includes('undefined'));
assert.ok(empty.includes(t('en', 'TXT_NO_LATEST_GAMES')));
const detail = render(React.createElement(GameDetailModal, { game: games[1], catalog: games, language: 'en', onClose: noop, onSelectGame: noop, onTriggerAction: noop }));
assert.match(detail, /role="dialog"/);
assert.equal((detail.match(/role="tab"/g) || []).length, 4);
assert.equal((detail.match(/class="tv-neighbor(?: tv-neighbor-selected)?"/g) || []).length, 4);
assert.ok(detail.indexOf('tv-detail-tabs') < detail.indexOf('tv-detail-body'));
assert.ok(detail.includes(t('en', 'TXT_ACTION_UNAVAILABLE')));
const library = render(React.createElement(LibraryBrowser, { isEmulatorsView: false, catalog: games,
  language: 'en', onOpenGame: noop, onRefresh: noop, syncInProgress: false,
  filters: { sources_limited: false, sources: [], disabled_formats: [] } }));
assert.equal((library.match(/class="tv-library-tile /g) || []).length, 8);
console.log('PASS: native layout geometry, viewport fitting, portrait/platform covers, selection, empty state, library page, and game-panel SSR');
