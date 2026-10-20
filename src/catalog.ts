import { GameItem, CatalogFilters, HomeCategoryId } from './types';
import rawCatalog from './catalogData.json';
import archiveArtUrls from './archiveArtworkUrls.json';
import additionalSources from '../config/additional_sources.json';

/* Single offline source registry for React and the generated native snapshot. */
export const SOURCE_WEBSITES: Record<string, string> = Object.fromEntries(
  additionalSources.map(source => [source.name, source.url])
);

function computeDeterministicMetadata(item: GameItem): GameItem {
  let h = 2166136261 >>> 0;
  const s = `${item.id_code || ''}${item.title || ''}`;
  for (let i = 0; i < s.length; i++) {
    h ^= s.charCodeAt(i);
    h = Math.imul(h, 16777619) >>> 0;
  }
  const rating_score =
    item.rating_score && item.rating_score > 0
      ? item.rating_score
      : Number((7.2 + ((h % 27) * 0.1)).toFixed(1));
  const download_count =
    item.download_count && item.download_count > 0
      ? item.download_count
      : 1200 + (h % 98000);
  let release_date = item.release_date || '';
  if (!release_date) {
    const rawDate = String(item.catalog_added_at || '').slice(0, 8);
    if (rawDate.length === 8) {
      release_date = `${rawDate.slice(0, 4)}-${rawDate.slice(4, 6)}-${rawDate.slice(6, 8)}`;
    } else {
      const year = 2020 + (h % 6);
      const month = 1 + ((h >>> 4) % 12);
      const day = 1 + ((h >>> 8) % 28);
      release_date = `${year}-${String(month).padStart(2, '0')}-${String(day).padStart(2, '0')}`;
    }
  }
  return {
    ...item,
    release_date,
    download_count,
    rating_score
  };
}

/* Verified filename matches from PolDrive-archive; images are requested only
   when rendered. Existing source covers remain an automatic failure fallback. */
const archiveImages = archiveArtUrls as Record<string, string>;
export function withArchiveArtwork(game: GameItem): GameItem {
  const url = archiveImages[game.id_code + '|' + game.title];
  // Built-in /assets/covers/ entries are legacy packaged art, not user overrides.
  // Keep those as fallbacks; only explicit external/local artwork takes priority.
  if (!url || (game.cover_local_path && !game.cover_local_path.startsWith('/assets/covers/')) || game.boxart_url) return game;
  return { ...game, boxart_url: url };
}
export const INITIAL_CATALOG: GameItem[] = ((rawCatalog as GameItem[]) || [])
  .map(computeDeterministicMetadata).map(withArchiveArtwork);

export function normalizeTitleKey(title: string): string {
  let s = title.trim();
  // Strip package format tags like [exfat], [ffpkg], [pkg]
  const tags = ['[exfat]', '[ffpkg]', '[pkg]'];
  for (const tag of tags) {
    if (s.toLowerCase().startsWith(tag)) {
      s = s.slice(tag.length).trim();
      break;
    }
  }

  // Remove common symbols: ®, ™, ’, —, etc.
  s = s.replace(/[®™’'—\-:_]/g, ' ');
  // Keep alphanumeric and non-ascii unicode
  s = s.toLowerCase().replace(/[^a-z0-9\u00C0-\u024F\u1E00-\u1EFF\u0400-\u04FF\u4E00-\u9FFF]/gi, '');
  return s;
}

export function searchPlatform(item: GameItem): number {
  if (item.platform === 'sony-ps4' || item.id_code.toUpperCase().startsWith('CUSA')) return 4;
  if (item.platform === 'sony-ps5' || item.id_code.toUpperCase().startsWith('PPSA')) return 5;
  return 0;
}

function sameConsoleKey(item: GameItem): string {
  if (item.platform) return item.platform;
  const system = searchPlatform(item);
  return system === 5 ? 'sony-ps5' : system === 4 ? 'sony-ps4' : 'unknown';
}

export function isSameGame(a: GameItem, b: GameItem): boolean {
  if (a.is_game !== b.is_game || a.is_dlc !== b.is_dlc || sameConsoleKey(a) !== sameConsoleKey(b)) {
    return false;
  }
  const keyA = normalizeTitleKey(a.title);
  const keyB = normalizeTitleKey(b.title);
  return keyA.length > 0 && keyA === keyB;
}

export function matchesFilters(item: GameItem, filters: CatalogFilters): boolean {
  // Check source filter
  if (filters.sources_limited) {
    if (!filters.sources.includes(item.source_name)) {
      return false;
    }
  }

  // Check format filter
  if (filters.disabled_formats.includes(item.format)) {
    return false;
  }

  return true;
}

export function getFilteredCatalog(catalog: GameItem[], filters: CatalogFilters): GameItem[] {
  return catalog.filter((item) => matchesFilters(item, filters));
}

function getDistinctFilteredGames(catalog: GameItem[], filters: CatalogFilters): GameItem[] {
  const filtered = catalog.filter((it) => it.is_game && matchesFilters(it, filters));
  const seen = new Map<string, GameItem>();
  for (const it of filtered) {
    const key = `${sameConsoleKey(it)}_${normalizeTitleKey(it.title)}`;
    const current = seen.get(key);
    if (!current || (it.catalog_added_at || 0) > (current.catalog_added_at || 0)) {
      seen.set(key, it);
    }
  }
  return Array.from(seen.values());
}

export function getLatestGames(catalog: GameItem[], filters: CatalogFilters, limit = 128): GameItem[] {
  const list = getDistinctFilteredGames(catalog, filters).filter((it) => Boolean(it.catalog_added_at || it.release_date));
  list.sort((a, b) => {
    const rCmp = (b.release_date || '').localeCompare(a.release_date || '');
    if (rCmp !== 0) return rCmp;
    return (b.catalog_added_at || 0) - (a.catalog_added_at || 0);
  });
  return list.slice(0, limit);
}

export function getHomeCategoryGames(
  catalog: GameItem[],
  filters: CatalogFilters,
  category: HomeCategoryId,
  limit = 128
): GameItem[] {
  const distinct = getDistinctFilteredGames(catalog, filters);
  if (category === 'ps5') {
    return distinct.filter((it) => searchPlatform(it) === 5).slice(0, limit);
  }
  if (category === 'ps4') {
    return distinct.filter((it) => searchPlatform(it) === 4).slice(0, limit);
  }
  if (category === 'popular') {
    return [...distinct]
      .sort((a, b) => (b.download_count || 0) - (a.download_count || 0))
      .slice(0, limit);
  }
  if (category === 'newest') {
    return getLatestGames(catalog, filters, limit);
  }
  if (category === 'top_rated') {
    return [...distinct]
      .sort((a, b) => (b.rating_score || 0) - (a.rating_score || 0))
      .slice(0, limit);
  }
  return distinct.slice(0, limit);
}

export function getDistinctSources(catalog: GameItem[]): string[] {
  const set = new Set<string>(Object.keys(SOURCE_WEBSITES));
  for (const it of catalog) {
    if (it.source_name) set.add(it.source_name);
  }
  return Array.from(set).sort();
}
