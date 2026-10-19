import { GameItem, CatalogFilters } from './types';
import rawCatalog from './catalogData.json';

export const INITIAL_CATALOG: GameItem[] = (rawCatalog as GameItem[]) || [];

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
  if (item.id_code.toUpperCase().startsWith('CUSA')) return 4;
  if (item.id_code.toUpperCase().startsWith('PPSA')) return 5;
  return 0;
}

export function isSameGame(a: GameItem, b: GameItem): boolean {
  if (a.is_game !== b.is_game || a.is_dlc !== b.is_dlc || searchPlatform(a) !== searchPlatform(b)) {
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

export function getLatestGames(catalog: GameItem[], filters: CatalogFilters, limit = 128): GameItem[] {
  const filtered = catalog.filter((it) => it.is_game && it.catalog_added_at && matchesFilters(it, filters));

  // Group by title per platform so newest copy is shown
  const seen = new Map<string, GameItem>();
  for (const it of filtered) {
    const key = `${searchPlatform(it)}_${normalizeTitleKey(it.title)}`;
    const current = seen.get(key);
    if (!current || (it.catalog_added_at || 0) > (current.catalog_added_at || 0)) {
      seen.set(key, it);
    }
  }

  const list = Array.from(seen.values());
  list.sort((a, b) => (b.catalog_added_at || 0) - (a.catalog_added_at || 0));
  return list.slice(0, limit);
}

export function getDistinctSources(catalog: GameItem[]): string[] {
  const set = new Set<string>();
  for (const it of catalog) {
    if (it.source_name) set.add(it.source_name);
  }
  return Array.from(set).sort();
}
