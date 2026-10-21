import React, { useEffect, useRef } from 'react';
import { GameItem, LanguageCode, HomeCategoryId } from '../types';
import { t } from '../translations';
import { GameCard, GameListHeader } from './GameCard';
import { GAME_LIST } from '../layout';

interface Category {
  id: HomeCategoryId;
  titleKey: string;
  games: GameItem[];
}
interface Props {
  games: GameItem[];
  categories?: Category[];
  activeCategoryIndex: number;
  onSelectCategory?: (index: number, gameIndex?: number) => void;
  language: LanguageCode;
  selected: number;
  sidebarFocused: boolean;
  onSelect: (index: number) => void;
  onOpen: (game: GameItem) => void;
  onPlay?: (game: GameItem) => void;
  heading?: string;
  emptyMessage?: string;
  showDate: boolean;
}

export function UbiquityHomeView({ games, categories, activeCategoryIndex,
  onSelectCategory, language, selected, sidebarFocused, onSelect, onOpen,
  heading, emptyMessage }: Props) {
  const category = categories?.[activeCategoryIndex];
  const list = category?.games || games;
  const index = Math.max(0, Math.min(selected, list.length - 1));
  const start = Math.floor(index / GAME_LIST.pageSize) * GAME_LIST.pageSize;
  const ref = useRef<HTMLDivElement>(null);
  const pendingFocus = useRef<number | null>(null);
  useEffect(() => {
    if (pendingFocus.current === null) return;
    ref.current?.querySelector<HTMLButtonElement>(`[data-grid-index="${pendingFocus.current}"]`)?.focus();
    pendingFocus.current = null;
  }, [index]);
  useEffect(() => {
    if (categories || sidebarFocused) return;
    const handle = (event: KeyboardEvent) => {
      if (document.querySelector('[role="dialog"]') || (event.target as HTMLElement)?.closest?.('.game-list-page,input,textarea,button')) return;
      if (event.key === 'ArrowDown' || event.key === 'ArrowUp') {
        event.preventDefault(); onSelect(Math.max(0, Math.min(list.length - 1, index + (event.key === 'ArrowDown' ? 1 : -1))));
      } else if (event.key === 'Enter' && list[index]) { event.preventDefault(); onOpen(list[index]); }
    };
    window.addEventListener('keydown', handle);
    return () => window.removeEventListener('keydown', handle);
  }, [categories, sidebarFocused, list, index, onSelect, onOpen]);
  return <div className="game-list-page" data-layout="game-list">
    <h1>{heading || (category ? t(language, category.titleKey) : t(language, 'TXT_LATEST_GAMES'))}</h1>
    {categories && <nav className="game-list-categories" aria-label="Game categories">
      {categories.map((cat, i) => <button type="button" key={cat.id}
        aria-current={i === activeCategoryIndex ? 'page' : undefined}
        onClick={() => onSelectCategory?.(i)}>{t(language, cat.titleKey)}</button>)}
    </nav>}
    <div className="game-list-table"><GameListHeader language={language} />
    <div className="game-list-rows" ref={ref}
      onWheel={event => { if (event.deltaY && list.length) onSelect(Math.max(0, Math.min(list.length - 1, index + (event.deltaY > 0 ? 1 : -1)))); }}
      onKeyDown={event => {
        if (!categories && !sidebarFocused) {
          if (event.key === 'ArrowDown' || event.key === 'ArrowUp') {
            event.preventDefault(); event.stopPropagation();
            onSelect(Math.max(0, Math.min(list.length - 1, index + (event.key === 'ArrowDown' ? 1 : -1)))); return;
          }
        }
        if (event.key === 'Enter' || event.key === ' ') {
          event.preventDefault(); event.stopPropagation(); if (list[index]) onOpen(list[index]); return;
        }
        if (event.key !== 'Tab') return;
        const row = (event.target as HTMLElement).closest<HTMLElement>('[data-grid-index]');
        if (!row) return;
        const next = Number(row.dataset.gridIndex) + (event.shiftKey ? -1 : 1);
        if (next < 0 || next >= list.length) return;
        event.preventDefault(); pendingFocus.current = next; onSelect(next);
      }}>
      {list.slice(start, start + GAME_LIST.pageSize).map((game, offset) => <GameCard
        key={game.id_code + '-' + offset} game={game} virtualIndex={start + offset}
        isSelected={!sidebarFocused && start + offset === index} language={language}
        onSelect={() => onSelect(start + offset)} onOpen={() => onOpen(game)} />)}
      {!list.length && <p>{emptyMessage || t(language, 'TXT_NO_LATEST_GAMES')}</p>}
    </div>
    </div>
    <footer className="game-list-footer">
      <span>{list.length ? index + 1 : 0} / {list.length}</span>
      <span>↑ ↓ · ✕ {t(language, 'TXT_GAME_DETAILS')}</span>
      <button type="button" disabled={index === 0} onClick={() => onSelect(Math.max(0, start - GAME_LIST.pageSize))}>←</button>
      <button type="button" disabled={start + GAME_LIST.pageSize >= list.length}
        onClick={() => onSelect(Math.min(list.length - 1, start + GAME_LIST.pageSize))}>→</button>
    </footer>
  </div>;
}
