import React, { useState } from 'react';
import { GameItem, LanguageCode } from '../types';
import { t } from '../translations';
import { platformLabel } from '../layout';
import { FittedText } from './FittedText';

export function CoverArt({ game, className = '' }: { game: GameItem; className?: string }) {
  const [failedSources, setFailedSources] = useState<string[]>([]);
  const source = [game.boxart_url, game.cover_local_path, game.cover_url]
    .find(value => value && !failedSources.includes(value)) || '';
  const platform = platformLabel(game.id_code);
  const completeBox = Boolean(game.boxart_url && source === game.boxart_url);
  return <div className={`tv-cover ${className}`} style={{ background:
    `linear-gradient(rgb(${game.cover_r},${game.cover_g},${game.cover_b}), #101014)` }}>
    {platform && !completeBox && <div className={`tv-platform tv-platform-${platform.toLowerCase()}`}>{platform}</div>}
    <div className="tv-cover-image">
      <div className="tv-cover-placeholder"><strong>WOLODRIVE</strong><span>{game.title}</span></div>
      {source && <img src={source} alt={game.title}
        onError={() => setFailedSources(values => [...values, source])} />}
    </div>
  </div>;
}

interface GameCardProps {
  game: GameItem; isSelected: boolean; onSelect: () => void;
  onOpen: () => void; language: LanguageCode;
}

export const GameCard: React.FC<GameCardProps> = ({ game, isSelected, onSelect, onOpen, language }) => (
  <button type="button" className={`tv-card ${isSelected ? 'tv-card-selected' : ''}`}
    aria-label={`${game.title} — ${t(language, 'TXT_GAME_DETAILS')}`}
    onFocus={onSelect} onClick={() => { onSelect(); onOpen(); }}>
    <CoverArt game={game} />
    <span className="tv-card-title" title={game.title}>{game.title}</span>
    <span className="tv-card-footer"><FittedText>{isSelected
      ? `[X] ${t(language, 'TXT_GAME_DETAILS')}`
      : `${game.format === 'ffpkg' ? 'FFPKG' : game.format.toUpperCase()} / ${game.size_gb.toFixed(1)} GB`}</FittedText></span>
  </button>
);
