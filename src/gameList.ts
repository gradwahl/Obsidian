import { GameItem } from './types';
import { COMPANIES } from './libraryData';

const consoles = COMPANIES.flatMap(company => company.consoles);
export function gameConsole(game: GameItem): string {
  const platform = consoles.find(system => system.platform_id && system.platform_id === game.platform);
  if (platform) return platform.name;
  const byId = consoles.find(system => system.title_id_prefix &&
    game.id_code.toUpperCase().startsWith(system.title_id_prefix));
  return byId?.name || game.platform || '—';
}

/** Metadata-only entries do not establish an actual downloadable file format. */
export function gameFormat(game: GameItem): string {
  return game.format === 'catalog' ? '—' : game.format.toUpperCase();
}
export function gameSize(game: GameItem): string {
  const gb = game.size_gb;
  if (!Number.isFinite(gb) || gb <= 0) return '—';
  if (gb >= 1000) return (gb / 1000).toFixed(1) + ' TB';
  if (gb >= 1) return gb.toFixed(1) + ' GB';
  if (gb >= 0.001) return (gb * 1000).toFixed(1) + ' MB';
  if (gb >= 0.000001) return (gb * 1000000).toFixed(1) + ' KB';
  return Math.max(1, Math.round(gb * 1000000000)) + ' B';
}
