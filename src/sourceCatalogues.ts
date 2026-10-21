/// <reference types="vite/client" />
import type { GameItem } from './types';
import { ARCHIVE_ASSET_BASE } from './archiveAssets';

interface SourceSnapshot {
  schemaVersion: number;
  sources: Array<{
    id: string;
    name: string;
    website: string;
    status: string;
    note?: string;
    packages: Array<{
      id: string; title: string; platform?: string; category?: string;
      version?: string; sizeBytes?: number; coverUrl?: string;
      sourceUrl?: string; description?: string; createdAt?: string;
    }>;
  }>;
}
export interface SourceLoadStatus {
  id: string; name: string; entries: number; status: string; note?: string;
}
export interface SourceLoadResult {
  entries: GameItem[];
  statuses: SourceLoadStatus[];
}

/* Each JSON becomes its own Vite JS chunk, requested only when the loader runs. */
const indexedFiles = import.meta.glob<{ default: SourceSnapshot }>('../assets/catalogs/*.json');

function safeGame(entry: SourceSnapshot['sources'][number]['packages'][number],
                  source: SourceSnapshot['sources'][number],
                  archiveCovers: Record<string, string>): GameItem | null {
  if (!entry || typeof entry.title !== 'string' || typeof entry.id !== 'string' ||
      !entry.title.trim() || !entry.id.trim()) return null;
  const safeUrl = (value?: string) => value && /^https:\/\//i.test(value) ? value : undefined;
  const created = entry.createdAt && /^\d{4}-\d{2}-\d{2}/.test(entry.createdAt)
    ? entry.createdAt.slice(0, 10) : '';
  const timestamp = entry.createdAt ? Date.parse(entry.createdAt) : NaN;
  const size = Number(entry.sizeBytes);
  const artworkPath = archiveCovers[(entry.platform || '') + '|' + entry.title];
  const streamCover = artworkPath && !artworkPath.split('/').includes('..') && /^assets\/covers\/[a-zA-Z0-9%_./-]+$/.test(artworkPath)
    ? ARCHIVE_ASSET_BASE + artworkPath : undefined;
  return {
    id_code: entry.id, title: entry.title, version: entry.version || '',
    source_name: source.name,
    // Retro Game Champion publishes system collections, not verified game pages.
    is_game: source.id !== 'retrogamechampion' && entry.category !== 'dlc',
    is_dlc: entry.category === 'dlc', screenshot_urls: [],
    platform: entry.platform || '',
    source_url: safeUrl(entry.sourceUrl) || safeUrl(source.website),
    boxart_url: streamCover,
    cover_url: safeUrl(entry.coverUrl),
    description: entry.description || 'Catalog metadata only - ' + source.name,
    install_path: '',
    size_gb: Number.isFinite(size) && size > 0 ? size / (1024 * 1024 * 1024) : 0,
    format: 'catalog', state: 'available', progress: 0,
    cover_r: 24, cover_g: 30, cover_b: 40,
    ...(created ? { release_date: created } : {}),
    ...(Number.isFinite(timestamp) ? { catalog_added_at: timestamp } : {})
  };
}

export async function loadSourceCatalogues(
  onProgress?: (status: SourceLoadStatus) => void
): Promise<SourceLoadResult> {
  // Load path-only cover matches as a separate JS chunk. Fail open to source
  // artwork if that optional metadata chunk is unavailable or outdated.
  const archiveCovers: Record<string, string> = await import('./archiveSourceCovers.json')
    .then(module => module.default as Record<string, string>)
    .catch(() => ({}));
  const snapshots = await Promise.all(
    Object.entries(indexedFiles).map(async ([path, importer]) => {
      try {
        const loaded = await importer();
        return { path, data: loaded.default, error: '' };
      } catch (error) {
        return { path, data: null, error: String(error) };
      }
    })
  );
  const full = new Map<string, SourceSnapshot['sources'][number]>();
  const partial = new Map<string, SourceSnapshot['sources'][number]>();
  const statuses: SourceLoadStatus[] = [];
  for (const snapshot of snapshots) {
    if (!snapshot.data || !Array.isArray(snapshot.data.sources)) {
      statuses.push({ id: snapshot.path, name: snapshot.path, entries: 0,
        status: 'error', note: snapshot.error || 'Invalid source file' });
      continue;
    }
    for (const source of snapshot.data.sources) {
      if (!source?.id || !Array.isArray(source.packages)) continue;
      if (snapshot.path.endsWith('/additional.json')) partial.set(source.id, source);
      else full.set(source.id, source);
    }
  }
  const entries: GameItem[] = [];
  const addSource = (id: string, source: SourceSnapshot['sources'][number]) => {
    let added = 0;
    const unique = new Set<string>();
    for (const item of source.packages) {
      const converted = safeGame(item, source, archiveCovers);
      if (!converted) continue;
      const key = converted.id_code + '|' + converted.title + '|' + converted.platform;
      if (unique.has(key)) continue;
      unique.add(key);
      entries.push(converted);
      added++;
    }
    const status = { id, name: source.name, entries: added,
      status: source.status, note: source.note };
    statuses.push(status);
    onProgress?.(status);
  };
  for (const [id, source] of partial) {
    if (!full.has(id)) addSource(id, source);
  }
  for (const [id, source] of full) addSource(id, source);
  return { entries, statuses };
}
