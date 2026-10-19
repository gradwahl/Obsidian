import { GameItem } from './types';

/** Shared identity is source + game id, never title alone across consoles. */
export interface ArtworkMatch { url: string; provider: string; confidence: number }
export type ArtworkMatches = Record<string, ArtworkMatch>;

export function applyArtworkMatches(catalog: GameItem[], matches: ArtworkMatches): GameItem[] {
  return catalog.map(game => {
    // First-party artwork and user-selected local overrides take precedence.
    if (game.cover_local_path || game.boxart_url || game.cover_url) return game;
    const key = game.source_name + '|' + game.id_code;
    const match = matches[key];
    if (!match || !match.url.startsWith('https://') || match.confidence < 85) return game;
    return { ...game, boxart_url: match.url };
  });
}
