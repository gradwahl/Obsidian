import React, { useState, useMemo, useRef, useEffect } from 'react';
import { GameItem, LanguageCode } from '../types';
import { t } from '../translations';
import { normalizeTitleKey, searchPlatform, matchesFilters } from '../catalog';
import { GameCard, GameListHeader } from './GameCard';
import { ChevronLeft, ChevronRight, Delete, CornerDownLeft } from 'lucide-react';

interface SearchGridProps {
  catalog: GameItem[];
  language: LanguageCode;
  onOpenGame: (game: GameItem) => void;
  filters: any;
}

const SEARCH_LETTERS = 'abcdefghijklmnopqrstuvwxyz1234567890'.split('');

export const SearchGrid: React.FC<SearchGridProps> = ({
  catalog,
  language,
  onOpenGame,
  filters
}) => {
  const [query, setQuery] = useState('');
  const [page, setPage] = useState(0);
  const [selectedIdx, setSelectedIdx] = useState(0);
  const rootRef = useRef<HTMLDivElement>(null);
  const pendingFocus = useRef(false);

  const pageSize = 9;
  // Count all matching identities, retaining only the existing visible page.
  const results = useMemo(() => {
    const q = query.trim().toLowerCase();
    const accepts = (item: GameItem) => {
      if (!matchesFilters(item, filters)) return false;
      if (!q) return true;
      return (
        item.title.toLowerCase().includes(q) ||
        item.id_code.toLowerCase().includes(q)
      );
    };

    // Group same games across source formats
    const items: GameItem[] = [];
    let count = 0;
    const seen = new Set<string>();
    for (const item of catalog) {
      if (!accepts(item)) continue;
      const titleKey = normalizeTitleKey(item.title);
      const key = titleKey ? `${searchPlatform(item)}:${item.is_game}:${item.is_dlc}:${titleKey}` : item.id_code;
      if (seen.has(key)) continue;
      seen.add(key);
      if (count >= page * pageSize && items.length < pageSize) items.push(item);
      count++;
    }
    return { items, count };
  }, [catalog, query, filters, page]);

  const totalPages = Math.max(1, Math.ceil(results.count / pageSize));
  const currentItems = results.items;

  const handleKeyClick = (ch: string) => {
    setQuery((prev) => prev + ch);
    setPage(0);
    setSelectedIdx(0);
  };

  const handleDelete = () => {
    setQuery((prev) => prev.slice(0, -1));
    setPage(0);
    setSelectedIdx(0);
  };

  const handleSpace = () => {
    setQuery((prev) => prev + ' ');
    setPage(0);
    setSelectedIdx(0);
  };

  useEffect(() => {
    if (!pendingFocus.current) return;
    pendingFocus.current = false;
    rootRef.current?.querySelectorAll<HTMLButtonElement>('.game-list-row')[selectedIdx]?.focus();
  }, [page, selectedIdx]);

  return (
    <div className="tv-search tv-page" ref={rootRef} onKeyDown={event => {
      const target = event.target as HTMLElement;
      const result = target.closest('.game-list-row');
      const rows = rootRef.current?.querySelectorAll<HTMLButtonElement>('.game-list-row');
      if (result && (event.key === 'ArrowDown' || event.key === 'ArrowUp')) {
        event.preventDefault(); event.stopPropagation();
        const next = page * pageSize + selectedIdx + (event.key === 'ArrowDown' ? 1 : -1);
        if (next < 0 || next >= results.count) return;
        pendingFocus.current = true; setPage(Math.floor(next / pageSize)); setSelectedIdx(next % pageSize);
      } else if (event.key === 'ArrowRight' && !result && rows?.length) {
        event.preventDefault(); rows[selectedIdx]?.focus();
      } else if (result && event.key === 'ArrowLeft') {
        event.preventDefault(); rootRef.current?.querySelector<HTMLInputElement>('input')?.focus();
      }
    }}>
      {/* Title & Query input */}
      <div className="max-w-6xl mb-6">
        <h2 className="text-2xl font-black text-white tracking-tight mb-3">
          {t(language, 'TXT_NAV_SEARCH')}
        </h2>

        <div className="flex items-center gap-3">
          <input
            type="text"
            value={query}
            onChange={(e) => {
              setQuery(e.target.value);
              setPage(0);
              setSelectedIdx(0);
            }}
            placeholder={t(language, 'TXT_SEARCH_PROMPT')}
            className="w-full max-w-xl px-4 py-3 rounded-lg bg-[#18181f] border border-[#2e2e3a] text-white text-base focus:outline-none focus:border-white focus:ring-2 focus:ring-white/20 transition-all font-medium"
            autoFocus
          />
          {query && (
            <button
              onClick={() => {
                setQuery('');
                setPage(0);
              }}
              className="px-3 py-3 rounded-lg bg-[#22222a] hover:bg-[#2c2c36] text-xs font-semibold text-[#a0a0aa] hover:text-white"
            >
              Clear
            </button>
          )}
          <span className="text-xs font-mono text-[#8a8a94] shrink-0">
            {t(language, 'TXT_SEARCH_COUNT', results.count)}
          </span>
        </div>
      </div>

      {/* Main Container: On-screen Keyboard on the left, Results on the right */}
      <div className="tv-search-columns">
        {/* On-screen A-Z / 0-9 Keyboard */}
        <div className="tv-search-keyboard">
          {/* Top row: Space & Delete */}
          <div className="grid grid-cols-2 gap-2 mb-2.5">
            <button
              onClick={handleSpace}
              className="flex items-center justify-center gap-1.5 py-2.5 px-3 rounded bg-[#1e1e26] hover:bg-white hover:text-black text-xs font-bold text-white transition-all active:scale-95 border border-[#2e2e3a]"
            >
              <CornerDownLeft className="w-3.5 h-3.5" />
              <span>{t(language, 'TXT_SEARCH_SPACE')}</span>
            </button>
            <button
              onClick={handleDelete}
              className="flex items-center justify-center gap-1.5 py-2.5 px-3 rounded bg-[#1e1e26] hover:bg-[#e50914] hover:text-white text-xs font-bold text-white transition-all active:scale-95 border border-[#2e2e3a]"
            >
              <Delete className="w-3.5 h-3.5" />
              <span>{t(language, 'TXT_SEARCH_DELETE')}</span>
            </button>
          </div>

          {/* Letter / Number grid (6 columns) */}
          <div className="grid grid-cols-6 gap-1.5">
            {SEARCH_LETTERS.map((letter) => (
              <button
                key={letter}
                onClick={() => handleKeyClick(letter)}
                className="h-11 rounded bg-[#1a1a22] hover:bg-white hover:text-black text-sm font-bold text-white transition-all active:scale-90 border border-[#282834] uppercase flex items-center justify-center"
              >
                {letter}
              </button>
            ))}
          </div>

          <div className="mt-4 text-[11px] text-[#71717a] leading-relaxed">
            {t(language, 'TXT_SEARCH_HINT')}
          </div>
        </div>

        {/* 3-Column Results Grid */}
        <div className="flex-1">
          {currentItems.length === 0 ? (
            <div className="py-20 text-center text-[#8a8a94] text-sm bg-[#121216] border border-[#202028] rounded-xl">
              {t(language, 'TXT_SEARCH_EMPTY')}
            </div>
          ) : (
            <div>
              <div className="game-list-table-scroll"><GameListHeader language={language} />
              <div className="tv-search-results">
                {currentItems.map((item, idx) => (
                  <GameCard
                    key={item.id_code + '_' + idx}
                    game={item}
                    isSelected={selectedIdx === idx}
                    onSelect={() => setSelectedIdx(idx)}
                    onOpen={() => onOpenGame(item)}
                    language={language}
                  />
                ))}
              </div>

              </div>
              {/* Paging controls */}
              {totalPages > 1 && (
                <div className="flex items-center justify-between mt-6 pt-4 border-t border-[#1c1c24]">
                  <span className="text-xs font-mono text-[#8a8a94]">
                    Page {page + 1} of {totalPages}
                  </span>

                  <div className="flex items-center gap-2">
                    <button
                      onClick={() => setPage((p) => Math.max(0, p - 1))}
                      disabled={page === 0}
                      className="p-2 rounded bg-[#1e1e26] hover:bg-[#2c2c36] disabled:opacity-40 text-white transition-all"
                    >
                      <ChevronLeft className="w-4 h-4" />
                    </button>
                    <button
                      onClick={() => setPage((p) => Math.min(totalPages - 1, p + 1))}
                      disabled={page >= totalPages - 1}
                      className="p-2 rounded bg-[#1e1e26] hover:bg-[#2c2c36] disabled:opacity-40 text-white transition-all"
                    >
                      <ChevronRight className="w-4 h-4" />
                    </button>
                  </div>
                </div>
              )}
            </div>
          )}
        </div>
      </div>
    </div>
  );
};
