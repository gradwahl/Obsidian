import React, { useEffect, useRef, useState } from 'react';
import { GameItem, LanguageCode, HomeCategoryId } from '../types';
import { TV_LAYOUT } from '../layout';
import { t } from '../translations';
import { HeroBanner } from './HeroBanner';
import { UbiquityHomeView } from './UbiquityHomeView';
import { GameCard } from './GameCard';

export interface HomeCategoryRow {
  id: HomeCategoryId;
  titleKey: string;
  games: GameItem[];
}

interface HomeViewProps {
  games: GameItem[];
  uiTheme?: 'netflix' | 'ubiquity';
  categories?: HomeCategoryRow[];
  activeCategoryIndex?: number;
  onSelectCategory?: (catIndex: number, colIndex?: number) => void;
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
  uiTheme = 'netflix',
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
  // Retain scroll velocity when D-pad presses arrive before the previous move settles.
  const scrollMotion = useRef(new WeakMap<HTMLDivElement, { velocity: number }>());
  const [gridExpanded, setGridExpanded] = useState(false);

  const hasMultiCategories = Boolean(categories && categories.length > 0);
  const activeCat = hasMultiCategories
    ? categories![Math.max(0, Math.min(activeCategoryIndex, categories!.length - 1))]
    : null;
  const activeList = activeCat ? activeCat.games : games;
  const selectedIndex = Math.max(0, Math.min(selected, activeList.length - 1));
  const focusedGame = activeList[selectedIndex] || activeList[0] || null;
  const singleStart = !hasMultiCategories && games.length > 64 ? Math.max(0, selectedIndex - 6) : 0;
  const singleEnd = !hasMultiCategories && games.length > 64
    ? Math.min(games.length, selectedIndex + 15) : games.length;

  useEffect(() => {
    const el = hasMultiCategories ? rowRefs.current[activeCategoryIndex] : carousel.current;
    if (!el) return;
    const target = Math.min(selectedIndex * (TV_LAYOUT.cardWidth + TV_LAYOUT.cardGap),
      Math.max(0, el.scrollWidth - el.clientWidth));
    const motion = scrollMotion.current.get(el) || { velocity: 0 };
    scrollMotion.current.set(el, motion);
    if (window.matchMedia('(prefers-reduced-motion: reduce)').matches) {
      el.scrollLeft = target;
      motion.velocity = 0;
      return;
    }

    // The native SDL carousel uses this exact critically damped spring.
    // Updating its destination preserves momentum when someone taps quickly.
    const speed = 24;
    let frame = 0;
    let previous = performance.now();
    const tick = (now: number) => {
      const dt = Math.min(0.05, Math.max(0, (now - previous) / 1000));
      previous = now;
      const offset = el.scrollLeft - target;
      const movement = (motion.velocity + speed * offset) * dt;
      const decay = Math.exp(-speed * dt);
      const next = target + (offset + movement) * decay;
      motion.velocity = (motion.velocity - speed * movement) * decay;
      if (Math.abs(next - target) < 0.05 && Math.abs(motion.velocity) < 0.5) {
        el.scrollLeft = target;
        motion.velocity = 0;
        return;
      }
      el.scrollLeft = next;
      frame = requestAnimationFrame(tick);
    };
    frame = requestAnimationFrame(tick);
    return () => cancelAnimationFrame(frame);
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

  if (uiTheme === 'ubiquity' && !gridExpanded) {
    return <UbiquityHomeView
      games={games} categories={categories} activeCategoryIndex={activeCategoryIndex}
      onSelectCategory={onSelectCategory} language={language} selected={selected}
      sidebarFocused={sidebarFocused} onSelect={onSelect} onOpen={onOpen}
      onPlay={onPlay} heading={title} emptyMessage={emptyMessage} showDate={showDate} />;
  }

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
                          if (!isCatActive) onSelectCategory?.(catIdx, index);
                          else onSelect(index);
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
              {singleStart > 0 && <span aria-hidden="true" className="tv-virtual-spacer"
                style={{ flexBasis: singleStart * (TV_LAYOUT.cardWidth + TV_LAYOUT.cardGap) - TV_LAYOUT.cardGap }} />}
              {games.slice(singleStart, singleEnd).map((game, offset) => {
                const index = singleStart + offset;
                return <GameCard
                  key={`${game.id_code}_${index}`}
                  game={game}
                  isSelected={selectedIndex === index && !sidebarFocused}
                  language={language}
                  onSelect={() => onSelect(index)}
                  onOpen={() => onOpen(game)}
                />;
              })}
              {singleEnd < games.length && <span aria-hidden="true" className="tv-virtual-spacer"
                style={{ flexBasis: (games.length - singleEnd) * (TV_LAYOUT.cardWidth + TV_LAYOUT.cardGap) - TV_LAYOUT.cardGap }} />}
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
    </div>
  );
}
