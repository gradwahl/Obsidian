import React, { useEffect, useRef, useState } from 'react';
import { GameItem, LanguageCode } from '../types';
import { GameCard } from './GameCard';

export function gridWindow(count: number, scrollTop: number, height: number) {
  const rows = Math.ceil(count / 6);
  const first = Math.min(Math.max(0, rows - 1), Math.max(0, Math.floor(scrollTop / 392) - 1));
  const last = Math.min(rows, Math.ceil((scrollTop + height) / 392) + 1);
  return { start: Math.min(count, first * 6), end: Math.min(count, last * 6),
    before: first ? first * 392 - 24 : 0, after: last < rows ? (rows - last) * 392 - 24 : 0 };
}

export function VirtualGameGrid({ games, selected, sidebarFocused, language, onSelect, onOpen }:
  { games: GameItem[]; selected: number; sidebarFocused: boolean; language: LanguageCode;
    onSelect: (index: number) => void; onOpen: (game: GameItem) => void }) {
  const ref = useRef<HTMLDivElement>(null);
  const pendingFocus = useRef<number | null>(null);
  const [viewport, setViewport] = useState({ top: Math.floor(selected / 6) * 392, height: 800 });
  useEffect(() => {
    const node = ref.current;
    if (!node) return;
    const update = () => setViewport({ top: node.scrollTop, height: node.clientHeight });
    node.addEventListener('scroll', update, { passive: true });
    window.addEventListener('resize', update);
    const observer = typeof ResizeObserver === 'function' ? new ResizeObserver(update) : null;
    observer?.observe(node); update();
    return () => { node.removeEventListener('scroll', update); window.removeEventListener('resize', update); observer?.disconnect(); };
  }, []);
  useEffect(() => {
    const node = ref.current;
    if (!node) return;
    const top = Math.floor(selected / 6) * 392;
    if (top < node.scrollTop) node.scrollTop = top;
    else if (top + 368 > node.scrollTop + node.clientHeight)
      node.scrollTop = Math.max(0, top + 368 - node.clientHeight + 24);
    setViewport({ top: node.scrollTop, height: node.clientHeight });
  }, [selected, games.length]);
  const range = gridWindow(games.length, viewport.top, viewport.height);
  useEffect(() => {
    if (pendingFocus.current === null) return;
    const node = ref.current?.querySelector<HTMLButtonElement>(`[data-grid-index="${pendingFocus.current}"]`);
    if (node) { pendingFocus.current = null; node.focus(); }
  }, [selected, viewport]);
  return <div className="tv-expanded-grid" ref={ref} style={{ alignContent: 'start' }}
    onKeyDown={event => {
      // Preserve sequential keyboard traversal across an unmounted row boundary.
      if (event.key !== 'Tab') return;
      const card = (event.target as HTMLElement).closest<HTMLElement>('[data-grid-index]');
      if (!card) return;
      const next = Number(card.dataset.gridIndex) + (event.shiftKey ? -1 : 1);
      if (next < 0 || next >= games.length) return;
      event.preventDefault(); pendingFocus.current = next; onSelect(next);
    }}>
    {range.before > 0 && <span aria-hidden="true" style={{ gridColumn: '1 / -1', height: range.before }} />}
    {games.slice(range.start, range.end).map((game, offset) => {
      const index = range.start + offset;
      return <GameCard key={`${game.source_name}_${game.id_code}_${index}`} game={game} virtualIndex={index}
        isSelected={selected === index && !sidebarFocused} language={language}
        onSelect={() => onSelect(index)} onOpen={() => onOpen(game)} />;
    })}
    {range.after > 0 && <span aria-hidden="true" style={{ gridColumn: '1 / -1', height: range.after }} />}
  </div>;
}
