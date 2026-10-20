import React, { useState } from 'react';
import { GameItem, LanguageCode } from '../types';
import { t } from '../translations';
import { platformLabel } from '../layout';
import { FittedText } from './FittedText';
import { nextCoverSource } from '../artworkMatches';

export function CoverArt({ game, className = '' }: { game: GameItem; className?: string }) {
  const [failedSources, setFailedSources] = useState<string[]>([]);
  const source = nextCoverSource(game, failedSources);
  const platform = platformLabel(game.id_code);
  const completeBox = Boolean(source && (source === game.boxart_url || source === game.cover_fallback_url));
  return <div className={`tv-cover ${className}`} style={{ background:
    `linear-gradient(rgb(${game.cover_r},${game.cover_g},${game.cover_b}), #101014)` }}>
    {platform && !completeBox && <div className={`tv-platform tv-platform-${platform.toLowerCase()}`}>{platform}</div>}
    {game.rating_score ? (
      <div className="tv-rating-badge">★ {game.rating_score.toFixed(1)}</div>
    ) : null}
    <div className="tv-cover-image">
      {!source && <div className="tv-cover-placeholder"><strong>PolDrive</strong><span>{game.title}</span></div>}
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
