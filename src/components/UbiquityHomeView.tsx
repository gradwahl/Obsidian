import React, { useEffect, useRef, useState } from 'react';
import { GameItem, LanguageCode, HomeCategoryId } from '../types';
import { nextCoverSource } from '../artworkMatches';
import { t } from '../translations';

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

// Preserve the previous backdrop until the next artwork finishes loading.
function CinematicBackground({ game }: { game: GameItem | null }) {
  const [failed, setFailed] = useState<string[]>([]);
  const candidate = [game?.backdrop_url, game?.cover_local_path,
    game?.cover_url].find(src => src && !failed.includes(src)) || '';
  const [visible, setVisible] = useState(candidate);
  const [next, setNext] = useState('');
  const [ready, setReady] = useState(false);

  useEffect(() => {
    setFailed([]);
  }, [game?.id_code]);

  useEffect(() => {
    if (candidate === visible) return;
    if (!candidate) { setVisible(''); setNext(''); return; }
    setReady(false);
    setNext(candidate);
  }, [candidate, visible]);

  useEffect(() => {
    if (!ready || !next) return;
    const timer = window.setTimeout(() => {
      setVisible(next);
      setNext('');
      setReady(false);
    }, 380);
    return () => window.clearTimeout(timer);
  }, [ready, next]);

  return (
    <div className="ubq-cinematic-background" aria-hidden="true">
      {visible && <img className="ubq-backdrop-current" src={visible} alt=""
        onError={() => setFailed(old => old.includes(visible) ? old : [...old, visible])} />}
      {next && next !== visible && <img key={next} className={'ubq-backdrop-next' + (ready ? ' ubq-backdrop-next-ready' : '')} src={next} alt=""
        onLoad={() => setReady(true)}
        onError={() => { setFailed(old => old.includes(next) ? old : [...old, next]); setNext(''); }} />}
      <div className="ubq-cinematic-vignette" />
    </div>
  );
}

function UbiquityCover({ game, selected, onSelect, onOpen }: {
  game: GameItem; selected: boolean; onSelect: () => void; onOpen: () => void;
}) {
  const [failed, setFailed] = useState<string[]>([]);
  const cover = nextCoverSource(game, failed);
  const downloadable = game.state === 'available' && game.format !== 'catalog';
  return (
    <button type="button"
      className={'ubq-game-cover' + (selected ? ' ubq-game-cover-selected' : '')}
      aria-label={game.title} aria-current={selected ? 'true' : undefined}
      onFocus={onSelect} onClick={() => { onSelect(); onOpen(); }}>
      <span className="ubq-square-art">
        {cover
          ? <img src={cover} alt="" loading="eager"
              onError={() => setFailed(values => [...values, cover])} />
          : <span className="ubq-cover-fallback">{game.title}</span>}
      </span>
      <span className="ubq-cover-footer">
        <span className="ubq-cover-name">{game.title}</span>
        {downloadable && <span className="ubq-cover-download" aria-hidden="true">↓</span>}
      </span>
    </button>
  );
}

export function UbiquityHomeView({
  games, categories, activeCategoryIndex, onSelectCategory, language,
  selected, sidebarFocused, onSelect, onOpen, onPlay,
  heading, emptyMessage, showDate
}: Props) {
  const activeCategory = categories?.[activeCategoryIndex];
  const list = activeCategory ? activeCategory.games : games;
  const index = Math.max(0, Math.min(selected, list.length - 1));
  const game = list[index] || null;
  // Full public source indexes can contain tens of thousands of entries.
  // Keep a small window of real DOM covers without losing scroll geometry.
  const windowStart = list.length > 64 ? Math.max(0, index - 6) : 0;
  const windowEnd = list.length > 64 ? Math.min(list.length, index + 15) : list.length;
  const scrollRef = useRef<HTMLDivElement>(null);

  useEffect(() => {
    const row = scrollRef.current;
    if (!row) return;
    // Keep the active cover near the left, with the upcoming covers visible.
    // Reference-size 232px square artwork + 20px spacing; native SDL2 uses the same step.
    const target = Math.max(0, Math.min(index * 252,
      Math.max(0, row.scrollWidth - row.clientWidth)));
    const motion = window.matchMedia('(prefers-reduced-motion: reduce)').matches ? 'instant' : 'smooth';
    row.scrollTo({ left: target, behavior: motion });
  }, [index, activeCategoryIndex]);

  const date = game?.release_date || String(game?.catalog_added_at || '').slice(0, 8);
  const year = date ? date.slice(0, 4) : '';
  const summary = game
    ? game.description_id ? t(language, game.description_id) : game.description
    : emptyMessage || t(language, 'TXT_NO_LATEST_GAMES');

  return (
    <div className="tv-home ubq-home" data-layout="ubiquity">
      <CinematicBackground game={game} />
      <div className="ubq-ambient-top">
        <span className="ubq-menu-glyph" aria-hidden="true">☰</span>
        <span className="ubq-product-name"><b>P</b>olDrive</span>
        <span className="ubq-system-label">PS5 LIBRARY</span>
      </div>

      <div className="ubq-feature" key={game?.id_code || heading}>
        <h1 className="ubq-feature-title">{game?.title || heading || t(language, 'TXT_LATEST_GAMES')}</h1>
        {game && <div className="ubq-feature-metadata">
          <span>{game.source_name || 'PolDrive'}</span>
          {showDate && year && <><i /> <span>{year}</span></>}
          {(game.rating_score || 0) > 0 && <><i /> <span className="ubq-score">★ {(game.rating_score || 0).toFixed(1)}/10</span></>}
          <i /><span>{game.format.toUpperCase()}</span>
          <i /><span>{game.size_gb.toFixed(1)} GB</span>
        </div>}
        <p className="ubq-feature-description">{summary}</p>
      </div>

      <section className="ubq-shelf" aria-label={activeCategory ? t(language, activeCategory.titleKey) : heading}>
        <div className="ubq-shelf-heading">
          <h2>{activeCategory ? t(language, activeCategory.titleKey) : heading || t(language, 'TXT_LATEST_GAMES')}</h2>
          <span>{list.length ? index + 1 : 0} / {list.length}</span>
        </div>
        <div className="ubq-shelf-track" ref={scrollRef}>
          {!sidebarFocused && list.length > 0 && (
            <div
              className="ubq-focus-pill"
              style={{
                transform: `translate3d(${28 + index * 252 - 15}px, 9px, 0)`,
              }}
            />
          )}
          {windowStart > 0 && <span aria-hidden="true" className="ubq-virtual-spacer"
            style={{ flexBasis: windowStart * 252 - 20 }} />}
          {list.slice(windowStart, windowEnd).map((item, offset) => {
            const gameIndex = windowStart + offset;
            return <UbiquityCover
              key={item.id_code + '-' + gameIndex}
              game={item} selected={gameIndex === index && !sidebarFocused}
              onSelect={() => onSelect(gameIndex)} onOpen={() => onOpen(item)} />;
          })}
          {windowEnd < list.length && <span aria-hidden="true" className="ubq-virtual-spacer"
            style={{ flexBasis: (list.length - windowEnd) * 252 - 20 }} />}
        </div>
      </section>

      {categories && categories.length > 1 && <nav className="ubq-category-select" aria-label="Game categories">
        {categories.map((cat, catIndex) => (
          <button type="button" key={cat.id}
            className={catIndex === activeCategoryIndex ? 'ubq-category-current' : ''}
            aria-current={catIndex === activeCategoryIndex ? 'page' : undefined}
            onClick={() => onSelectCategory?.(catIndex)}>
            {t(language, cat.titleKey)}
          </button>
        ))}
      </nav>}

      <div className="ubq-shortcuts">
        <span><b>✕</b> {t(language, 'TXT_GAME_DETAILS')}</span>
        <button type="button" disabled={!game} onClick={() => game && (onPlay ? onPlay(game) : onOpen(game))}>
          <b>▶</b> {t(language, 'TXT_DOWNLOAD')}
        </button>
        <button type="button" disabled={!game} onClick={() => game && onOpen(game)}>
          <b>ⓘ</b> {t(language, 'TXT_MORE_INFO')}
        </button>
      </div>
    </div>
  );
}
