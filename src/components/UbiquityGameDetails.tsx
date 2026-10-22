import { gameSourceFormat, gameSourceMetadata, gameSourceVersion, gameSize } from '../gameList';
import { GlassLiquid } from './GlassLiquid';
import { useCatalogDetails } from '../useCatalogDetails';
import React, { useEffect, useMemo, useRef, useState } from 'react';
import { GameItem, LanguageCode } from '../types';
import { t } from '../translations';
import { isSameGame } from '../catalog';
import { alvroDirectoryForGame, ARCHIVE_ORG_LOGIN } from '../archiveOrgAccount';

interface Props {
  game: GameItem;
  catalog: GameItem[];
  language: LanguageCode;
  onClose: () => void;
  onTriggerAction: (game: GameItem) => void;
  onSelectGame: (game: GameItem) => void;
  parentGame?: GameItem | null;
  onBackToParent?: () => void;
  actionIndex: number;
  onActionIndexChange: (index: number) => void;
}

// Transparent game-logo artwork is not available in Obsidian's metadata.
// Text stays legible until a verified per-game logo can be supplied.
export function UbiquityGameDetails({
  game: summary, catalog, language, onClose, onTriggerAction, onSelectGame,
  parentGame, onBackToParent, actionIndex, onActionIndexChange
}: Props) {
  const game = useCatalogDetails(summary);
  const [optionsOpen, setOptionsOpen] = useState(false);
  const [showOriginal, setShowOriginal] = useState(false);
  // Controller users should land on the first option when the panel opens.
  useEffect(() => {
    if (!optionsOpen) return;
    const frame = requestAnimationFrame(() => {
      document.querySelector<HTMLButtonElement>('.ubq-detail-option-list button')?.focus();
    });
    return () => cancelAnimationFrame(frame);
  }, [optionsOpen]);
  const descriptionRef = useRef<HTMLDivElement>(null);

  useEffect(() => {
    setOptionsOpen(false);
    setShowOriginal(false);
    descriptionRef.current?.scrollTo(0, 0);
  }, [game.id_code]);

  const siblings = useMemo(
    () => catalog.filter(candidate => isSameGame(game, candidate)),
    [catalog, game]
  );
  const dlc = useMemo(() =>
    game.is_dlc ? [] : catalog.filter(item =>
      item.is_dlc && item.parent_title_id?.toUpperCase() === game.id_code.toUpperCase()),
    [catalog, game]
  );

  const alvroDirectory = alvroDirectoryForGame(game.source_name, game.source_url);
  const rawDate = game.release_date || String(game.catalog_added_at || '').slice(0, 8);
  const year = /^\d{4}/.test(rawDate) ? rawDate.slice(0, 4) : null;
  const description = game.description_id && !showOriginal
    ? t(language, game.description_id) : game.description;

  const act = (index: number) => {
    onActionIndexChange(index);
    if (index === 0) {
      if (game.format === 'catalog') setOptionsOpen(true);
      else onTriggerAction(game);
    } else if (index === 1) setOptionsOpen(open => !open);
    else onClose();
  };

  useEffect(() => {
    const onKey = (event: KeyboardEvent) => {
      if (event.key.toLowerCase() === 'i') setShowOriginal(value => !value);
      if (event.key === 'PageDown') descriptionRef.current?.scrollBy({ top: 90, behavior: 'smooth' });
      if (event.key === 'PageUp') descriptionRef.current?.scrollBy({ top: -90, behavior: 'smooth' });
    };
    window.addEventListener('keydown', onKey);
    return () => window.removeEventListener('keydown', onKey);
  }, []);

  return (
    <div className="ubq-detail-screen" role="dialog" aria-modal="true"
      aria-labelledby="ubq-detail-title" data-layout="ubiquity-detail">
      <div className="ubq-detail-provider">
        <span className="ubq-detail-provider-mark">◈</span> Obsidian
      </div>

      <div className="ubq-detail-content">
        <div className="ubq-detail-main">
          <h1 id="ubq-detail-title" className="ubq-detail-game-title">{game.title}</h1>
          <div className="ubq-detail-byline">
            <span>{game.source_name || 'Obsidian'}</span>
            {year && <><span aria-hidden="true">•</span><span>{year}</span></>}
            <span aria-hidden="true">•</span>
            <span>{game.platform || (/^PPSA/i.test(game.id_code) ? 'PS5' : /^CUSA/i.test(game.id_code) ? 'PS4' : 'Game')}</span>
            {(game.rating_score || 0) > 0 && <><span aria-hidden="true">•</span><span>★ {(game.rating_score || 0).toFixed(1)}/10</span></>}
          </div>
          <div className="ubq-detail-tags">
            <span className="glass-pill glass-pill--sm"><GlassLiquid />{game.format === 'catalog' ? gameSourceFormat(game, t(language, 'TXT_SOURCE_FORMAT_UNKNOWN')) : game.format.toUpperCase()}</span>
            <span className="glass-pill glass-pill--sm"><GlassLiquid />{game.id_code}</span>
            {gameSourceVersion(game) && <span className="glass-pill glass-pill--sm"><GlassLiquid />v{gameSourceVersion(game).replace(/^v/i, '')}</span>}
          </div>
          <div className="ubq-detail-description" ref={descriptionRef}>
            {description || game.title}
          </div>
          {game.description_id && <button type="button" className="ubq-detail-translate"
            onClick={() => setShowOriginal(value => !value)}>
            {t(language, showOriginal ? 'TXT_TRANSLATED_DESCRIPTION' : 'TXT_ORIGINAL_DESCRIPTION')} [I]
          </button>}
          <div className="ubq-detail-stats">
            <div><span>Install size</span><strong>{gameSize(game)}</strong></div>
            <div><span>Status</span><strong>{alvroDirectory ? 'Access varies' : game.state === 'installed' ? 'Installed' : game.state === 'downloading' ? 'Downloading' : 'Available'}</strong></div>
            <div><span>Packages</span><strong>{siblings.length}</strong></div>
            {(game.download_count || 0) > 0 && <div><span>Downloads</span><strong>{game.download_count?.toLocaleString()}</strong></div>}
          </div>
        </div>
        {alvroDirectory && <div className="ubq-detail-archive-access flex flex-col gap-2 rounded-xl border border-white/15 bg-black/30 p-3 text-xs" aria-label="Archive.org file access">
          <p className="text-white/70">Archive.org may require you to sign in to access files in this collection. Obsidian cannot verify your permissions.</p>
          <a className="text-indigo-300 underline hover:text-white" href={ARCHIVE_ORG_LOGIN} target="_blank" rel="noopener noreferrer">Sign in on Archive.org ↗</a>
          <a className="text-indigo-300 underline hover:text-white" href={alvroDirectory} target="_blank" rel="noopener noreferrer">Browse collection on Archive.org ↗</a>
        </div>}
        <div className="ubq-detail-actions" role="group" aria-label="Game actions">
          <button type="button" className={actionIndex === 0 ? 'ubq-detail-action-focused' : ''}
            onFocus={() => onActionIndexChange(0)} onClick={() => act(0)}>
            {game.format === 'catalog' ? t(language, 'TXT_GAME_DETAILS') :
              game.state === 'installed' ? t(language, 'TXT_MOUNT') : t(language, 'TXT_GAME_INSTALL')}
          </button>
          <button type="button" aria-label={t(language, 'TXT_GAME_SOURCES')} className={actionIndex === 1 ? 'ubq-detail-action-focused' : ''}
            onFocus={() => onActionIndexChange(1)} onClick={() => act(1)}>•••</button>
          <button type="button" aria-label="Back to games" className={actionIndex === 2 ? 'ubq-detail-action-focused' : ''}
            onFocus={() => onActionIndexChange(2)} onClick={() => act(2)}>←</button>
        </div>
      </div>

      {optionsOpen && <section className="ubq-detail-options" aria-label={t(language, 'TXT_GAME_SOURCES')}>
        <div className="ubq-detail-options-header">
          <h2>{t(language, 'TXT_GAME_SOURCES')}</h2>
          <button type="button" onClick={() => setOptionsOpen(false)} aria-label="Close sources">✕</button>
        </div>
        <div className="ubq-detail-option-list">
          {siblings.map((item, index) => <button type="button" key={item.id_code + '-' + index}
            onClick={() => onTriggerAction(item)}>
            <b>{item.source_name || 'Obsidian'}</b>
            <span>{gameSourceMetadata(item, t(language, 'TXT_SOURCE_FORMAT_UNKNOWN'), t(language, 'TXT_SOURCE_VERSION_UNKNOWN'))}</span>
          </button>)}
          {dlc.map((item, index) => <button type="button" key={'dlc-' + index} onClick={() => onSelectGame(item)}>
            <b>{item.source_name || 'Obsidian'}</b><span>{gameSourceMetadata(item, t(language, 'TXT_SOURCE_FORMAT_UNKNOWN'), t(language, 'TXT_SOURCE_VERSION_UNKNOWN'))}</span>
          </button>)}
          {!siblings.length && !dlc.length &&
            <p>No additional items available.</p>}
        </div>
        <p className="ubq-detail-availability">{t(language, 'TXT_ACTION_UNAVAILABLE')}</p>
      </section>}
      {parentGame && onBackToParent && <button type="button" className="ubq-detail-parent"
        onClick={onBackToParent}>{t(language, 'TXT_LIBRARY_BACK')}: {parentGame.title}</button>}
      <button type="button" className="ubq-detail-close" onClick={onClose}
        aria-label="Close game details">✕</button>
    </div>
  );
}