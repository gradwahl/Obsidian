/* Validate generated source pages against canonical verified source indexes. */
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const { execFileSync } = require('node:child_process');

const root = path.resolve(__dirname, '..');
execFileSync(process.execPath, [path.join(root, 'scripts/generate_browser_catalog_pages.cjs')], { cwd: root });
const dir = path.join(root, 'public/obsidian-pages');
const manifest = JSON.parse(fs.readFileSync(path.join(dir, 'manifest.json'), 'utf8'));
assert.equal(manifest.schemaVersion, 2);
const originals = new Map();
const partials = new Map();
for (const file of fs.readdirSync(path.join(root, 'assets/catalogs')).filter(p => p.endsWith('.json'))) {
  const data = JSON.parse(fs.readFileSync(path.join(root, 'assets/catalogs', file), 'utf8'));
  for (const source of data.sources) {
    if (file === 'additional.json') partials.set(source.id, source);
    else originals.set(source.id, source);
  }
}
for (const [id, source] of partials) if (!originals.has(id)) originals.set(id, source);
assert.equal(manifest.sources.length, originals.size);
const coverPaths = JSON.parse(fs.readFileSync(path.join(root, 'src/archiveSourceCovers.json'), 'utf8'));
let count = 0, covered = 0;
for (const source of manifest.sources) {
  const expected = originals.get(source.id);
  assert.ok(expected, 'Original missing: ' + source.id);
  assert.equal(source.name, expected.name);
  assert.equal(source.website, expected.website);
  const originalEntries = expected.packages;
  assert.equal(source.indexes.length, source.pages.length);
  let offset = 0;
  for (const page of source.pages) {
    assert.match(page, /^[a-zA-Z0-9_-]+-[a-f0-9]{12}-\d{3}-[a-f0-9]{10}\.json$/);
    const contents = fs.readFileSync(path.join(dir, page), 'utf8');
    assert.ok(Buffer.byteLength(contents) < 400000, 'Page too large to parse smoothly: ' + page);
    const items = JSON.parse(contents);
    const index = JSON.parse(fs.readFileSync(path.join(dir, source.indexes[source.pages.indexOf(page)]), 'utf8'));
    assert.equal(index.length, items.length);
    index.forEach((row, i) => {
      const expected = [items[i].id, items[i].title,
        items[i].platform || '', items[i].category || '', items[i].version || '',
        items[i].sizeBytes || 0, items[i].createdAt || ''];
      if (items[i].archiveAccess === 'sign-in-required') expected.push('sign-in-required');
      assert.deepEqual(row, expected);
    });
    assert.ok(items.length > 0 && items.length <= 384);
    for (const item of items) {
      const original = originalEntries[offset++];
      assert.equal(item.id, original.id);
      assert.equal(item.title, original.title);
      assert.equal(item.platform, original.platform);
      const artwork = coverPaths[String(item.platform || '') + '|' + String(item.title || '')];
      if (item.archiveCoverPath) {
        assert.equal(item.archiveCoverPath, artwork);
        covered++;
      }
      count++;
    }
  }
  assert.equal(offset, originalEntries.length, 'Missing source records: ' + source.id);
}
assert.ok(count > 80000);
assert.ok(covered > 1000, 'Archive cover matches should survive paging');
const loader = fs.readFileSync(path.join(root, 'src/sourceCatalogues.ts'), 'utf8');
assert.match(loader, /await nextTask\(\)/);
assert.match(loader, /NOTIFY_RECORDS/);
assert.doesNotMatch(loader, /import\.meta\.glob/);
const pageName = manifest.sources.find(s => s.pages.length)?.pages[0];
assert.ok(pageName && fs.existsSync(path.join(dir, pageName)));
console.log('PASS: ' + count + ' verified browser records, ' + covered +
  ' matched covers, ' + manifest.sources.length + ' sources in small JSON pages');

/* Execute the real TypeScript loader against the generated files. A test that
   only checks the manifest does not detect an accidentally incompatible fetch
   path, status shape or page filename. */
const ts = require('typescript');
require.extensions['.ts'] = (module, filename) => {
  const src = fs.readFileSync(filename, 'utf8');
  const compiled = ts.transpileModule(src, { compilerOptions: {
    target: ts.ScriptTarget.ES2022, module: ts.ModuleKind.CommonJS,
    esModuleInterop: true
  }});
  module._compile(compiled.outputText, filename);
};
const { loadSourceCatalogues, loadCatalogDetails, catalogDetailsStats } = require('../src/sourceCatalogues.ts');
let yields = 0;
global.window = { setTimeout(callback) { yields++; return setTimeout(callback, 0); } };
const served = new Map();
global.fetch = async (url) => {
  const name = String(url).replace(/^\/obsidian-pages\//, '');
  if (!name || name.includes('/') || name.includes('..')) return { ok: false, status: 404 };
  const filename = path.join(dir, name);
  if (!fs.existsSync(filename)) return { ok: false, status: 404 };
  served.set(name, (served.get(name) || 0) + 1);
  return { ok: true, json: async () => JSON.parse(fs.readFileSync(filename, 'utf8')) };
};

(async () => {
  const batches = [];
  const statuses = [];
  const result = await loadSourceCatalogues(
    status => statuses.push(status),
    entries => batches.push(entries.length)
  );
  assert.equal(statuses.length, manifest.sources.length);
  assert.equal(result.statuses.length, manifest.sources.length);
  const sum = result.statuses.reduce((n, status) => n + status.entries, 0);
  assert.equal(sum, result.entries.length);
  assert.equal(batches.at(-1), sum);
  assert.ok(batches.every((size, i) => !i || size > batches[i - 1]));
  assert.ok(sum >= 80000);
  assert.ok(batches.length > 12, 'Expected progressive updates rather than one gigantic commit');
  assert.ok(batches.every((size, i) => size - (batches[i - 1] || 0) < 4000));
  assert.ok(yields > 700, 'Converting 85k records must regularly yield to the browser');
  assert.ok(served.has('manifest.json'));
  assert.equal(served.size, pagesTotal() + 1);
  assert.ok([...served.keys()].every(file => file === 'manifest.json' || file.includes('-index-')),
    'Startup must not fetch any full metadata page');
  assert.ok(result.entries.every(game => !Object.hasOwn(game, 'description') &&
    !Object.hasOwn(game, 'boxart_url') && !Object.hasOwn(game, 'cover_url') &&
    Object.keys(game).length <= 10), 'Browsing records must remain compact');
  const sized=result.entries.find(game=>game.size_gb>0);
  assert.ok(sized);
  const rawSized=JSON.parse(fs.readFileSync(path.join(dir,sized.detailsPage),'utf8'))[sized.detailsOffset];
  assert.equal(sized.size_gb,rawSized.sizeBytes/1e9,'Use decimal GB for source byte counts');
  const samples = [];
  const pages = new Set();
  for (const game of result.entries) {
    if (pages.has(game.detailsPage)) continue;
    pages.add(game.detailsPage); samples.push(game);
    if (samples.length === 7) break;
  }
  const full = await Promise.all(samples.map(game => loadCatalogDetails(game)));
  full.forEach((game, i) => {
    assert.equal(game.title, samples[i].title);
    assert.equal(game.source_name, samples[i].source_name);
    assert.equal(game.is_game, samples[i].is_game);
    assert.equal(game.size_gb, samples[i].size_gb);
    assert.ok(game.description);
    assert.ok(!Object.hasOwn(samples[i], 'description'), 'Hydration must not fill summaries');
  });
  // Await the final request's cache trimming microtask.
  await new Promise(resolve => setImmediate(resolve));
  assert.ok(catalogDetailsStats().retained <= 4, 'At most four full pages remain cached');
  const sample = samples.at(-1), requests = served.get(sample.detailsPage);
  await loadCatalogDetails(sample);
  assert.equal(served.get(sample.detailsPage), requests, 'Reuse a cached details page');
  const controller = new AbortController(); controller.abort();
  await assert.rejects(loadCatalogDetails(sample, controller.signal), { name: 'AbortError' });
  console.log('PASS: browser loader streamed ' + sum + ' entries in ' +
    batches.length + ' updates and ' + yields + ' browser-yield intervals');
})().catch(error => { console.error(error); process.exitCode = 1; });

function pagesTotal() {
  return manifest.sources.reduce((sum, source) => sum + source.pages.length, 0);
}
