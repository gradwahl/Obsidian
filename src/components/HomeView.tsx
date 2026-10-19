import React, { useEffect, useRef } from 'react';
import { GameItem, LanguageCode } from '../types';
import { TV_LAYOUT } from '../layout';
import { t } from '../translations';
import { HeroBanner } from './HeroBanner';
import { GameCard } from './GameCard';

interface HomeViewProps {
  games: GameItem[]; language: LanguageCode; selected: number; sidebarFocused: boolean;
  onSelect: (index: number) => void; onOpen: (game: GameItem) => void;
  onRefresh: () => void; syncInProgress: boolean;
  title?: string; emptyMessage?: string; showDate?: boolean;
}

export function HomeView({ games, language, selected, sidebarFocused, onSelect, onOpen, onRefresh, syncInProgress,
  title, emptyMessage, showDate = true }: HomeViewProps) {
  const carousel = useRef<HTMLDivElement>(null);
  const selectedIndex = Math.max(0, Math.min(selected, games.length - 1));
  // Render only neighbouring cards. Some supported consoles have thousands of titles.
  const windowStart = Math.max(0, selectedIndex - 12);
  const windowEnd = Math.min(games.length, selectedIndex + 13);
  const step = TV_LAYOUT.cardWidth + TV_LAYOUT.cardGap;
  useEffect(() => {
    carousel.current?.scrollTo({ left: selectedIndex * (TV_LAYOUT.cardWidth + TV_LAYOUT.cardGap),
      behavior: window.matchMedia('(prefers-reduced-motion: reduce)').matches ? 'instant' : 'smooth' });
  }, [selectedIndex]);
  return <div className="tv-home">
    <HeroBanner game={games[selectedIndex] || null} language={language} heading={title} emptyMessage={emptyMessage} showDate={showDate} />
    <h2 className="tv-row-title">{title || t(language, 'TXT_LATEST_GAMES')}</h2>
    {games.length ? <div className="tv-carousel" ref={carousel}>
      {windowStart > 0 && <div aria-hidden="true" style={{ flex: '0 0 auto', width: windowStart * step - TV_LAYOUT.cardGap }} />}
      {games.slice(windowStart, windowEnd).map((game, index) => {
        const actualIndex = windowStart + index;
        return <GameCard key={`${game.id_code}_${actualIndex}`} game={game}
          isSelected={selectedIndex === actualIndex && !sidebarFocused} language={language}
          onSelect={() => onSelect(actualIndex)} onOpen={() => onOpen(game)} />;
      })}
      {windowEnd < games.length && <div aria-hidden="true" style={{ flex: '0 0 auto', width: (games.length - windowEnd) * step - TV_LAYOUT.cardGap }} />}
    </div> : null}
    <div className="tv-home-position">
      <span>{games.length ? selectedIndex + 1 : 0} / {games.length}</span>
      <button type="button" aria-label="Previous game" disabled={selectedIndex <= 0} onClick={() => onSelect(selectedIndex - 1)}>←</button>
      <button type="button" aria-label="Next game" disabled={selectedIndex >= games.length - 1} onClick={() => onSelect(selectedIndex + 1)}>→</button>
    </div>
    <div className="tv-home-hints"><span>← →　↑ ↓ / D-pad　 X / Enter</span><span>{t(language, 'TXT_BACK_HINT')}</span>
      <button type="button" disabled={syncInProgress} onClick={onRefresh}>{t(language, 'TXT_REFRESH_HINT')}</button></div>
    <div className="tv-home-logo">WOLODRIVE</div>
  </div>;
}
