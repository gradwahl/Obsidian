import React, { useEffect, useRef, useState } from 'react';
import { GameItem, LanguageCode, HomeCategoryId } from '../types';
import { TV_LAYOUT } from '../layout';
import { t } from '../translations';
import { HeroBanner } from './HeroBanner';
import { GameCard } from './GameCard';

export interface HomeCategoryRow {
  id: HomeCategoryId;
  titleKey: string;
  games: GameItem[];
}

interface HomeViewProps {
  games: GameItem[];
  categories?: HomeCategoryRow[];
  activeCategoryIndex?: number;
  onSelectCategory?: (catIndex: number) => void;
  language: LanguageCode;
  selected: number;
  sidebarFocused: boolean;
  onSelect: (index: number) => void;
  onOpen: (game: GameItem) => void;
  onPlay?: (game: GameItem) => void;
  onRefresh: () => void;
  syncInProgress: boolean;
  title?: string;
  emptyMessage?: string;
  showDate?: boolean;
}

export function HomeView({
  games,
  categories,
  activeCategoryIndex = 0,
  onSelectCategory,
  language,
  selected,
  sidebarFocused,
  onSelect,
  onOpen,
  onPlay,
  onRefresh,
  syncInProgress,
  title,
  emptyMessage,
  showDate = true
}: HomeViewProps) {
  const carousel = useRef<HTMLDivElement>(null);
  const rowRefs = useRef<(HTMLDivElement | null)[]>([]);
  const [gridExpanded, setGridExpanded] = useState(false);

  const hasMultiCategories = Boolean(categories && categories.length > 0);
  const activeCat = hasMultiCategories
    ? categories![Math.max(0, Math.min(activeCategoryIndex, categories!.length - 1))]
    : null;
  const activeList = activeCat ? activeCat.games : games;
  const selectedIndex = Math.max(0, Math.min(selected, activeList.length - 1));
  const focusedGame = activeList[selectedIndex] || activeList[0] || null;

  useEffect(() => {
    const behavior = window.matchMedia('(prefers-reduced-motion: reduce)').matches ? 'instant' : 'smooth';
    if (hasMultiCategories) {
      const el = rowRefs.current[activeCategoryIndex];
      el?.scrollTo({
        left: selectedIndex * (TV_LAYOUT.cardWidth + TV_LAYOUT.cardGap),
        behavior
      });
    } else {
      carousel.current?.scrollTo({
        left: selectedIndex * (TV_LAYOUT.cardWidth + TV_LAYOUT.cardGap),
        behavior
      });
    }
  }, [selectedIndex, activeCategoryIndex, hasMultiCategories]);

  useEffect(() => {
    const onKey = (e: KeyboardEvent) => {
      const target = e.target as HTMLElement;
      if (target.closest('input,textarea,select,[role="dialog"]')) return;
      if ((e.key === 'g' || e.key === 'G') && hasMultiCategories) {
        e.preventDefault();
        setGridExpanded((v) => !v);
      }
    };
    window.addEventListener('keydown', onKey);
    return () => window.removeEventListener('keydown', onKey);
  }, [hasMultiCategories]);

  if (hasMultiCategories && gridExpanded && activeCat) {
    return (
      <div className="tv-home tv-home-grid-mode">
        <div className="tv-grid-header">
          <h2 className="tv-grid-title">{t(language, activeCat.titleKey)}</h2>
          <button
            type="button"
            className="tv-show-all-btn"
            onClick={() => setGridExpanded(false)}
          >
            {t(language, 'TXT_SHOW_LESS')}
          </button>
        </div>
        <div className="tv-expanded-grid">
          {activeCat.games.map((game, index) => (
            <GameCard
              key={`${activeCat.id}_grid_${game.id_code}_${index}`}
              game={game}
              isSelected={selectedIndex === index && !sidebarFocused}
              language={language}
              onSelect={() => onSelect(index)}
              onOpen={() => onOpen(game)}
            />
          ))}
        </div>
        <div className="tv-home-position">
          <span>
            {activeCat.games.length ? selectedIndex + 1 : 0} / {activeCat.games.length}
          </span>
        </div>
        <div className="tv-home-logo">WOLODRIVE</div>
      </div>
    );
  }

  return (
    <div className="tv-home">
      <HeroBanner
        game={focusedGame}
        language={language}
        heading={activeCat ? t(language, activeCat.titleKey) : title}
        emptyMessage={emptyMessage}
        showDate={showDate}
        onPlay={(g) => (onPlay ? onPlay(g) : onOpen(g))}
        onMoreInfo={(g) => onOpen(g)}
      />

      {hasMultiCategories ? (
        <div className="tv-categories-viewport">
          <div
            className="tv-categories-stack"
            style={{ transform: `translateY(-${activeCategoryIndex * 442}px)` }}
          >
            {categories!.map((cat, catIdx) => {
              const isCatActive = catIdx === activeCategoryIndex;
              const colIdx = isCatActive
                ? selectedIndex
                : 0;
              return (
                <div
                  key={cat.id}
                  className={`tv-category-block ${isCatActive ? 'tv-category-active' : ''}`}
                >
                  <div className="tv-category-header">
                    <button
                      type="button"
                      className="tv-row-title-btn"
                      onClick={() => {
                        onSelectCategory?.(catIdx);
                        setGridExpanded(true);
                      }}
                    >
                      {t(language, cat.titleKey)}
                    </button>
                    <button
                      type="button"
                      className="tv-show-all-btn"
                      onClick={() => {
                        onSelectCategory?.(catIdx);
                        setGridExpanded(true);
                      }}
                    >
                      {t(language, 'TXT_SHOW_ALL')} ›
                    </button>
                  </div>
                  <div
                    className="tv-carousel tv-category-carousel"
                    ref={(el) => {
                      rowRefs.current[catIdx] = el;
                    }}
                  >
                    {cat.games.map((game, index) => (
                      <GameCard
                        key={`${cat.id}_${game.id_code}_${index}`}
                        game={game}
                        isSelected={isCatActive && colIdx === index && !sidebarFocused}
                        language={language}
                        onSelect={() => {
                          if (!isCatActive) onSelectCategory?.(catIdx);
                          onSelect(index);
                        }}
                        onOpen={() => onOpen(game)}
                      />
                    ))}
                  </div>
                </div>
              );
            })}
          </div>
        </div>
      ) : (
        <>
          <h2 className="tv-row-title">{title || t(language, 'TXT_LATEST_GAMES')}</h2>
          {games.length ? (
            <div className="tv-carousel" ref={carousel}>
              {games.map((game, index) => (
                <GameCard
                  key={`${game.id_code}_${index}`}
                  game={game}
                  isSelected={selectedIndex === index && !sidebarFocused}
                  language={language}
                  onSelect={() => onSelect(index)}
                  onOpen={() => onOpen(game)}
                />
              ))}
            </div>
          ) : null}
        </>
      )}

      <div className="tv-home-position">
        <span>
          {activeList.length ? selectedIndex + 1 : 0} / {activeList.length}
        </span>
        <button
          type="button"
          aria-label="Previous game"
          disabled={selectedIndex <= 0}
          onClick={() => onSelect(selectedIndex - 1)}
        >
          ←
        </button>
        <button
          type="button"
          aria-label="Next game"
          disabled={selectedIndex >= activeList.length - 1}
          onClick={() => onSelect(selectedIndex + 1)}
        >
          →
        </button>
      </div>
      <div className="tv-home-hints">
        <span>← →　↑ ↓ / D-pad　 X / Enter</span>
        <span>{t(language, 'TXT_BACK_HINT')}</span>
        <button type="button" disabled={syncInProgress} onClick={onRefresh}>
          {t(language, 'TXT_REFRESH_HINT')}
        </button>
      </div>
      <div className="tv-home-logo">WOLODRIVE</div>
    </div>
  );
}
