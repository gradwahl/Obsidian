import { VirtualGameGrid } from './VirtualGameGrid';
import React, { useEffect, useState } from 'react';
import { GameItem, LanguageCode, HomeCategoryId } from '../types';
import { t } from '../translations';
import { UbiquityHomeView } from './UbiquityHomeView';

export interface HomeCategoryRow {
  id: HomeCategoryId;
  titleKey: string;
  games: GameItem[];
}

interface HomeViewProps {
  games: GameItem[];
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
  categories,
  activeCategoryIndex = 0,
  onSelectCategory,
  language,
  selected,
  sidebarFocused,
  onSelect,
  onOpen,
  onPlay,
  title,
  emptyMessage,
  showDate = true
}: HomeViewProps) {
  const [gridExpanded, setGridExpanded] = useState(false);

  const hasMultiCategories = Boolean(categories && categories.length > 0);
  const activeCat = hasMultiCategories
    ? categories![Math.max(0, Math.min(activeCategoryIndex, categories!.length - 1))]
    : null;
  const activeList = activeCat ? activeCat.games : games;
  const selectedIndex = Math.max(0, Math.min(selected, activeList.length - 1));
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

  if (!gridExpanded) {
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
        <VirtualGameGrid games={activeCat.games} selected={selectedIndex}
          sidebarFocused={sidebarFocused} language={language} onSelect={onSelect} onOpen={onOpen} />
        <div className="tv-home-position">
          <span>
            {activeCat.games.length ? selectedIndex + 1 : 0} / {activeCat.games.length}
          </span>
        </div>
      </div>
    );
  }

  return null;
}
