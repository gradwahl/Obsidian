import React, { useState, useMemo } from 'react';
import { GameItem, LanguageCode } from '../types';
import { t } from '../translations';
import { normalizeTitleKey, isSameGame, matchesFilters } from '../catalog';
import { GameCard } from './GameCard';
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

  // Filter and group search results matching the C code logic
  const results = useMemo(() => {
    const q = query.trim().toLowerCase();
    const filtered = catalog.filter((item) => {
      if (!matchesFilters(item, filters)) return false;
      if (!q) return true;
      return (
        item.title.toLowerCase().includes(q) ||
        item.id_code.toLowerCase().includes(q)
      );
    });

    // Group same games across source formats
    const unique: GameItem[] = [];
    for (const item of filtered) {
      const exists = unique.some((existing) => isSameGame(existing, item));
      if (!exists) {
        unique.push(item);
      }
    }
    return unique;
  }, [catalog, query, filters]);

  const pageSize = 9;
  const totalPages = Math.max(1, Math.ceil(results.length / pageSize));
  const currentItems = results.slice(page * pageSize, (page + 1) * pageSize);

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

  return (
    <div className="flex-1 h-full overflow-y-auto p-8 select-none">
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
            {t(language, 'TXT_SEARCH_COUNT', results.length)}
          </span>
        </div>
      </div>

      {/* Main Container: On-screen Keyboard on the left, Results on the right */}
      <div className="flex flex-col lg:flex-row gap-8 max-w-7xl">
        {/* On-screen A-Z / 0-9 Keyboard */}
        <div className="w-full lg:w-[380px] shrink-0 bg-[#121216] border border-[#202028] rounded-xl p-4.5">
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
              <div className="grid grid-cols-1 md:grid-cols-2 xl:grid-cols-3 gap-4">
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
