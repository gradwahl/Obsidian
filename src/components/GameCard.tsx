import { gameConsole, gameFormat, gameSize } from '../gameList';
import React from 'react';
import { GameItem, LanguageCode } from '../types';
import { t } from '../translations';

// Retained for legacy detail consumers; game artwork is no longer displayed.
export function CoverArt(_props: { game: GameItem; className?: string }) { return null; }
interface GameCardProps {
  game: GameItem; isSelected: boolean; onSelect: () => void;
  onOpen: () => void; language: LanguageCode; virtualIndex?: number;
}
export const GameCard: React.FC<GameCardProps> = ({ game, isSelected, onSelect, onOpen, language, virtualIndex }) => (
  <button type="button" className={'game-list-row' + (isSelected ? ' game-list-row-selected' : '')}
    data-grid-index={virtualIndex} aria-current={isSelected ? 'true' : undefined}
    aria-label={`${gameConsole(game)} — ${game.title} — ${gameFormat(game)} — ${gameSize(game)} — ${t(language, 'TXT_GAME_DETAILS')}`}
    onFocus={onSelect} onClick={() => { onSelect(); onOpen(); }}>
    <span className="game-list-console" title={gameConsole(game)}>{gameConsole(game)}</span>
    <span className="game-list-title" title={game.title}>{game.title}</span>
    <span className="game-list-format" title={gameFormat(game)}>{gameFormat(game)}</span>
    <span className="game-list-size">{gameSize(game)}</span>
  </button>
);

export function GameListHeader({ language }: { language: LanguageCode }) {
  return <div className="game-list-header">
    <span>{t(language, 'TXT_LIST_CONSOLE')}</span>
    <span>{t(language, 'TXT_LIST_NAME')}</span>
    <span>{t(language, 'TXT_LIST_FORMAT')}</span>
    <span>{t(language, 'TXT_LIST_SIZE')}</span>
  </div>;
}
