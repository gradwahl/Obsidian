#!/usr/bin/env node
/* Offline build-only conversion: small browser JSON pages instead of importing
   multi-megabyte JavaScript modules on the PS5 browser's main thread.
   Original verified source indexes are unchanged (also consumed by native). */
const fs = require('node:fs');
const path = require('node:path');
const crypto = require('node:crypto');

const ROOT = path.resolve(__dirname, '..');
const INPUT = path.join(ROOT, 'assets', 'catalogs');
const OUTPUT = path.join(ROOT, 'public', 'obsidian-pages');
const COVER_INDEX = path.join(ROOT, 'src', 'archiveSourceCovers.json');
const ITEMS_PER_PAGE = 384;

function assertSource(source) {
  return source && typeof source.id === 'string' &&
    typeof source.name === 'string' && Array.isArray(source.packages);
}
function build() {
  const files = fs.readdirSync(INPUT).filter(name => name.endsWith('.json')).sort();
  const fullIds = new Set();
  const sourceFiles = [];
  for (const file of files) {
    const payload = JSON.parse(fs.readFileSync(path.join(INPUT, file), 'utf8'));
    if (!Array.isArray(payload.sources)) throw new Error('Invalid source index: ' + file);
    for (const source of payload.sources) {
      if (!assertSource(source)) continue;
      if (file !== 'additional.json') fullIds.add(source.id);
      sourceFiles.push({ file, source });
    }
  }
  // source-id precedence follows the existing loader: a dedicated full
  // index overrides the overlapping "additional" preview for that source.
  const selected = sourceFiles.filter(({ file, source }) =>
    file !== 'additional.json' || !fullIds.has(source.id));
  const covers = JSON.parse(fs.readFileSync(COVER_INDEX, 'utf8'));
  fs.rmSync(OUTPUT, { recursive: true, force: true });
  fs.mkdirSync(OUTPUT, { recursive: true });
  const manifest = { schemaVersion: 2, sources: [] };
  let records = 0, pagesTotal = 0;
  const usedNames = new Set();
  for (const { source } of selected) {
    const fingerprint = crypto.createHash('sha256').update(source.id).digest('hex').slice(0, 12);
    const basename = source.id.replace(/[^a-z0-9_-]/gi, '_').slice(0, 32) + '-' + fingerprint;
    if (usedNames.has(basename)) throw new Error('Duplicate browser source: ' + source.id);
    usedNames.add(basename);
    const safeSource = { id: source.id, name: source.name,
      website: source.website, status: source.status, note: source.note };
    const pages = [];
    const indexes = [];
    for (let start = 0; start < source.packages.length; start += ITEMS_PER_PAGE) {
      const part = source.packages.slice(start, start + ITEMS_PER_PAGE).map(item => {
        const key = String(item.platform || '') + '|' + String(item.title || '');
        const cover = covers[key];
        if (!cover || typeof cover !== 'string' ||
            !/^assets\/covers\/[a-zA-Z0-9%_./-]+$/.test(cover) ||
            cover.split('/').includes('..')) return item;
        return { ...item, archiveCoverPath: cover };
      });
      const data = JSON.stringify(part);
      const digest = crypto.createHash('sha256').update(data).digest('hex').slice(0, 10);
      const file = basename + '-' + String(pages.length).padStart(3, '0') +
        '-' + digest + '.json';
      fs.writeFileSync(path.join(OUTPUT, file), data);
      pages.push(file);
      // Browsing needs identity, flags, sorting and size, not long descriptions
      // or artwork/source URLs. Offset in this index is the full-page locator.
      const indexData = JSON.stringify(part.map(item => {
        const summary = [
          item.id, item.title, item.platform || '', item.category || '',
          item.version || '', item.sizeBytes || 0, item.createdAt || ''
        ];
        if (item.archiveAccess === 'sign-in-required') summary.push('sign-in-required');
        return summary;
      }));
      const indexDigest = crypto.createHash('sha256').update(indexData).digest('hex').slice(0, 10);
      const indexName = basename + '-index-' + String(indexes.length).padStart(3, '0') + '-' + indexDigest + '.json';
      fs.writeFileSync(path.join(OUTPUT, indexName), indexData);
      indexes.push(indexName);
      records += part.length;
      pagesTotal++;
    }
    manifest.sources.push({ ...safeSource, pages, indexes });
  }
  // Keep generated URLs local and finite even when source counts grow.
  fs.writeFileSync(path.join(OUTPUT, 'manifest.json'), JSON.stringify(manifest));
  console.log('Browser catalogue: ' + manifest.sources.length + ' sources, ' +
              records + ' package records, ' + pagesTotal + ' bounded JSON pages');
}
build();
