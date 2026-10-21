import React from 'react';
import { GameItem, LanguageCode, HomeCategoryId } from '../types';
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
  return <UbiquityHomeView
    games={games} categories={categories} activeCategoryIndex={activeCategoryIndex}
    onSelectCategory={onSelectCategory} language={language} selected={selected}
    sidebarFocused={sidebarFocused} onSelect={onSelect} onOpen={onOpen}
    onPlay={onPlay} heading={title} emptyMessage={emptyMessage} showDate={showDate} />;
}
