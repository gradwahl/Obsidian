import { GameItem } from './types';

/** Shared identity is source + game id, never title alone across consoles. */
export interface ArtworkMatch { url: string; provider: string; confidence: number; fallbackOnly?: boolean; fallback?: ArtworkMatch }
export type ArtworkMatches = Record<string, ArtworkMatch>;

export function applyArtworkMatches(catalog: GameItem[], matches: ArtworkMatches): GameItem[] {
  return catalog.map(game => {
    // First-party artwork and user-selected local overrides take precedence.
    if (game.cover_local_path || game.boxart_url) return game;
    const key = game.source_name + '|' + game.id_code;
    const match = matches[key];
    if (!match || !match.url.startsWith('https://') || match.confidence < 85) return game;
    if (game.cover_url) return match.url === game.cover_url ? game
      : { ...game, cover_fallback_url: match.url };
    const fallback = match.fallback;
    return { ...game, boxart_url: match.url,
      ...(fallback?.url.startsWith('https://') && fallback.confidence >= 85
        ? { cover_fallback_url: fallback.url } : {}) };
  });
}

/** Keep healthy source art first; an image decode/load failure advances this list. */
export function coverSources(game: GameItem, backdrop = false): string[] {
  const covers = [game.boxart_url, game.cover_local_path, game.cover_url, game.cover_fallback_url];
  return [...new Set((backdrop ? [game.backdrop_url, ...covers] : covers)
    .filter((url): url is string => Boolean(url)))];
}

export function nextCoverSource(game: GameItem, failed: string[], backdrop = false): string {
  return coverSources(game, backdrop).find(url => !failed.includes(url)) || '';
}
