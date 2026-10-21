/// <reference types="vite/client" />
import type { GameItem } from './types';
import { ARCHIVE_ASSET_BASE } from './archiveAssets';
import { ResourceCache } from './resourceCache';

interface SourceInfo {
  id: string; name: string; website: string; status: string; note?: string;
  pages: string[]; indexes: string[];
}
interface BrowserManifest { schemaVersion: number; sources: SourceInfo[]; }
interface SourcePackage {
  id: string; title: string; platform?: string; category?: string;
  version?: string; sizeBytes?: number; coverUrl?: string;
  sourceUrl?: string; description?: string; createdAt?: string;
  archiveCoverPath?: string;
}
export interface SourceLoadStatus {
  id: string; name: string; entries: number; status: string; note?: string;
}
export interface SourceLoadResult {
  entries: GameItem[];
  statuses: SourceLoadStatus[];
}

// Each source index is split at build time into 384-record JSON pages.
// Do not import the original multi-megabyte indexes or the 6MB artwork
// mapping in the PS5 browser: evaluating those JS modules can freeze the
// browser thread and trigger "This page isn't responding".
const PAGES = '/obsidian-pages/';
const YIELD_RECORDS = 96;
const NOTIFY_RECORDS = 1536;

function nextTask(): Promise<void> {
  // A macrotask (not a resolved promise) lets input, CSS animations and
  // React painting run even on browsers without requestIdleCallback.
  return new Promise(resolve => window.setTimeout(resolve, 0));
}
async function readJSON(url: string, signal?: AbortSignal): Promise<unknown> {
  const response = await fetch(url, { credentials: 'same-origin', cache: 'no-store', signal });
  if (!response.ok) throw new Error('HTTP ' + response.status + ' from ' + url);
  return response.json();
}
function safeGame(entry: SourcePackage, source: SourceInfo): GameItem | null {
  if (!entry || typeof entry.title !== 'string' || typeof entry.id !== 'string' ||
      !entry.title.trim() || !entry.id.trim()) return null;
  const safeUrl = (value?: string) => value && /^https:\/\//i.test(value) ? value : undefined;
  const created = entry.createdAt && /^\d{4}-\d{2}-\d{2}/.test(entry.createdAt)
    ? entry.createdAt.slice(0, 10) : '';
  const timestamp = entry.createdAt ? Date.parse(entry.createdAt) : NaN;
  const size = Number(entry.sizeBytes);
  const artworkPath = entry.archiveCoverPath;
  const streamCover = artworkPath && !artworkPath.split('/').includes('..') &&
    /^assets\/covers\/[a-zA-Z0-9%_./-]+$/.test(artworkPath)
    ? ARCHIVE_ASSET_BASE + artworkPath : undefined;
  return {
    id_code: entry.id, title: entry.title, version: entry.version || '',
    source_name: source.name,
    is_game: source.id !== 'retrogamechampion' && entry.category !== 'dlc',
    is_dlc: entry.category === 'dlc', screenshot_urls: [],
    platform: entry.platform || '',
    source_url: safeUrl(entry.sourceUrl) || safeUrl(source.website),
    boxart_url: streamCover,
    cover_url: safeUrl(entry.coverUrl),
    description: entry.description || 'Catalog metadata only - ' + source.name,
    install_path: '',
    size_gb: Number.isFinite(size) && size > 0 ? size / 1e9 : 0,
    format: 'catalog', state: 'available', progress: 0,
    cover_r: 24, cover_g: 30, cover_b: 40,
    ...(created ? { release_date: created } : {}),
    ...(Number.isFinite(timestamp) ? { catalog_added_at: timestamp } : {})
  };
}

export async function loadSourceCatalogues(
  onProgress?: (status: SourceLoadStatus) => void,
  onBatch?: (entries: GameItem[]) => void,
  signal?: AbortSignal
): Promise<SourceLoadResult> {
  const manifest = await readJSON(PAGES + 'manifest.json', signal) as BrowserManifest;
  if (!manifest || manifest.schemaVersion !== 2 || !Array.isArray(manifest.sources))
    throw new Error('Browser catalogue manifest is missing or invalid');
  const entries: GameItem[] = [];
  const statuses: SourceLoadStatus[] = [];
  let lastNotified = 0;

  const flush = () => {
    if (onBatch && entries.length !== lastNotified) {
      lastNotified = entries.length;
      onBatch(entries);
    }
  };

  for (const source of manifest.sources) {
    if (signal?.aborted) break;
    if (!source || !Array.isArray(source.pages) || !Array.isArray(source.indexes) ||
        source.pages.length !== source.indexes.length || typeof source.name !== 'string') continue;
    let count = 0;
    let errors = 0;
    const seen = new Set<string>();
    // Shared immutable defaults avoid allocating empty strings/arrays and
    // source metadata as own properties on every browsing record.
    const defaults = [false, true].map(dlc => safeGame({
      id: '_', title: '_', category: dlc ? 'dlc' : ''
    }, source)!);
    for (let page = 0; page < source.indexes.length; page++) {
      const file = source.indexes[page];
      const detailsPage = source.pages[page];
      if (signal?.aborted) break;
      if (!/^[a-zA-Z0-9_-]+-[a-f0-9]{12}-index-\d{3}-[a-f0-9]{10}\.json$/.test(file) ||
          !validDetailsPage(detailsPage)) {
        errors++; continue;
      }
      try {
        const part = await readJSON(PAGES + file, signal);
        if (!Array.isArray(part)) throw new Error('Expected a package array');
        for (let offset = 0; offset < part.length; offset += YIELD_RECORDS) {
          if (signal?.aborted) break;
          for (let row = offset; row < Math.min(part.length, offset + YIELD_RECORDS); row++) {
            const raw = part[row];
            if (!Array.isArray(raw) || typeof raw[0] !== 'string' || !raw[0].trim() ||
                typeof raw[1] !== 'string' || !raw[1].trim()) continue;
            const converted = Object.assign(Object.create(defaults[raw[3] === 'dlc' ? 1 : 0]), {
              id_code: raw[0], title: raw[1], platform: raw[2], version: raw[4],
              size_gb: Number.isFinite(Number(raw[5])) && Number(raw[5]) > 0 ? Number(raw[5]) / 1e9 : 0,
              detailsPage, detailsOffset: row
            }) as CatalogSummary;
            const created = raw[6];
            if (typeof created === 'string' && /^\d{4}-\d{2}-\d{2}/.test(created))
              converted.release_date = created.slice(0, 10);
            const timestamp = created ? Date.parse(created) : NaN;
            if (Number.isFinite(timestamp)) converted.catalog_added_at = timestamp;
            const key = converted.id_code + '|' + converted.title + '|' + converted.platform;
            if (seen.has(key)) continue;
            seen.add(key);
            entries.push(converted);
            count++;
          }
          if (entries.length - lastNotified >= NOTIFY_RECORDS) flush();
          await nextTask();
        }
      } catch (_error) {
        if (signal?.aborted) break;
        errors++;
      }
    }
    flush();
    if (signal?.aborted) break;
    const status: SourceLoadStatus = {
      id: source.id, name: source.name, entries: count,
      status: errors ? 'error' : source.status,
      note: errors ? (errors + ' catalogue pages failed to load') : source.note
    };
    statuses.push(status);
    onProgress?.(status);
    await nextTask();
  }
  flush();
  return { entries, statuses };
}


interface CatalogSummary extends GameItem { detailsPage: string; detailsOffset: number; }
function validDetailsPage(file: string): boolean {
  return /^[a-zA-Z0-9_-]+-[a-f0-9]{12}-\d{3}-[a-f0-9]{10}\.json$/.test(file);
}
const details = new ResourceCache<SourcePackage[]>(async (file, signal) => {
  if (!validDetailsPage(file)) throw new Error('Invalid details page');
  const page = await readJSON(PAGES + file, signal);
  if (!Array.isArray(page) || page.length > 384) throw new Error('Invalid details page');
  return page;
}, 4, 2);

/** Full metadata lives only in a four-page LRU and currently mounted consumers.
 * Summary objects are never mutated or filled with retained full records. */
export async function loadCatalogDetails(game: GameItem, signal?: AbortSignal): Promise<GameItem> {
  const summary = game as CatalogSummary;
  if (!summary.detailsPage) return game;
  if (signal?.aborted) throw new DOMException('Aborted', 'AbortError');
  const lease = details.acquire(summary.detailsPage);
  const abort = () => lease.release();
  signal?.addEventListener('abort', abort, { once: true });
  try {
    const page = await lease.promise;
    if (signal?.aborted) throw new DOMException('Aborted', 'AbortError');
    const raw = page[summary.detailsOffset];
    if (!raw || raw.id !== game.id_code || raw.title !== game.title)
      throw new Error('Catalogue details do not match the selected item');
    const full = safeGame(raw, { id: '', name: game.source_name, website: game.source_url || '', status: '', pages: [], indexes: [] });
    if (!full) throw new Error('Invalid catalogue record');
    return { ...full, is_game: game.is_game, is_dlc: game.is_dlc };
  } finally {
    signal?.removeEventListener('abort', abort); lease.release();
  }
}
export function catalogDetailsStats() { return details.stats; }
