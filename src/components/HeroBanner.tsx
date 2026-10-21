import { MemoryImage } from './MemoryImage';
import { useCatalogDetails } from '../useCatalogDetails';
import React, { useEffect, useState } from 'react';
import { GameItem, LanguageCode } from '../types';
import { t } from '../translations';

interface HeroBannerProps {
  game: GameItem | null;
  language: LanguageCode;
  heading?: string;
  emptyMessage?: string;
  showDate?: boolean;
  onPlay?: (game: GameItem) => void;
  onMoreInfo?: (game: GameItem) => void;
}

// Keep the last loaded cover visible until the next backdrop has actually loaded.
// This avoids black flashes on slow artwork sources and fast controller navigation.
function ArtworkCrossfade({ src, onError }: { src: string; onError: (source: string) => void }) {
  const [visible, setVisible] = useState(src);
  const [loadedSource, setLoadedSource] = useState('');

  useEffect(() => {
    if (!src && visible) setVisible('');
  }, [src, visible]);

  useEffect(() => {
    if (!src || src === visible || loadedSource !== src) return;
    const timer = window.setTimeout(() => setVisible(src), 380);
    return () => window.clearTimeout(timer);
  }, [src, visible, loadedSource]);

  const incoming = src && src !== visible ? src : '';
  return <div className="tv-hero-backdrop">
    {visible && <MemoryImage artworkSize="hero" className="tv-hero-art" src={visible} alt=""
      onError={() => onError(visible)} />}
    {incoming && <MemoryImage artworkSize="hero" key={incoming}
      className={`tv-hero-art tv-hero-incoming ${loadedSource === incoming ? 'tv-hero-incoming-loaded' : ''}`}
      src={incoming} alt=""
      onLoad={() => requestAnimationFrame(() => setLoadedSource(incoming))}
      onError={() => onError(incoming)} />}
  </div>;
}

export const HeroBanner: React.FC<HeroBannerProps> = ({
  game: summary,
  language,
  heading,
  emptyMessage,
  showDate = true,
  onPlay,
  onMoreInfo
}) => {
  const game = useCatalogDetails(summary);
  const [showOriginal, setShowOriginal] = useState(false);
  const [failedSources, setFailedSources] = useState<string[]>([]);

  useEffect(() => {
    setShowOriginal(false);
    setFailedSources([]);
  }, [summary]);

  useEffect(() => {
    const toggle = (event: KeyboardEvent) => {
      const target = event.target as HTMLElement;
      if (event.key.toLowerCase() === 'i' && !target.closest('input,textarea,[role="dialog"]')) {
        setShowOriginal((value) => !value);
      }
    };
    window.addEventListener('keydown', toggle);
    return () => window.removeEventListener('keydown', toggle);
  }, []);

  const source =
    [game?.backdrop_url, game?.cover_local_path, game?.cover_url].find(
      (value) => value && !failedSources.includes(value)
    ) || '';
  const date = game?.release_date || String(game?.catalog_added_at || '').slice(0, 8);
  const added =
    date.length === 8 && !date.includes('-')
      ? `${date.slice(0, 4)}-${date.slice(4, 6)}-${date.slice(6)}`
      : date;

  return (
    <section className="tv-hero" aria-label={game?.title || heading || t(language, 'TXT_LATEST_GAMES')}>
      <ArtworkCrossfade src={source}
        onError={(failed) => setFailedSources((values) => values.includes(failed) ? values : [...values, failed])} />
      <div className="tv-hero-shade" />
      <div className="tv-wordmark">
        <strong>P</strong>
        <span>olDrive</span>
      </div>
      <h1 key={game?.id_code || heading} className="tv-hero-info-appear">{game?.title || heading || t(language, 'TXT_LATEST_GAMES')}</h1>
      {game && (
        <div key={game.id_code} className="tv-hero-metadata tv-hero-info-appear">
          <span className="tv-meta-pill glass-pill glass-pill--sm">{game.id_code}</span>
          <span className="tv-meta-pill glass-pill glass-pill--sm">v{game.version}</span>
          <span className="tv-meta-pill glass-pill glass-pill--sm">{game.format.toUpperCase()}</span>
          <span className="tv-meta-pill glass-pill glass-pill--sm">{game.size_gb.toFixed(1)} GB</span>
          {game.rating_score ? <span className="tv-meta-pill glass-pill glass-pill--sm">★ {game.rating_score.toFixed(1)}/10</span> : null}
          {game.download_count ? <span className="tv-meta-pill glass-pill glass-pill--sm">↓ {game.download_count.toLocaleString()}</span> : null}
          {showDate && added ? <span className="tv-meta-pill glass-pill glass-pill--sm">{added}</span> : null}
        </div>
      )}
      <div className="tv-hero-description tv-hero-info-appear" key={game?.id_code}>
        {game
          ? game.description_id && !showOriginal
            ? t(language, game.description_id)
            : game.description
          : emptyMessage || t(language, 'TXT_NO_LATEST_GAMES')}
      </div>
      {game && (
        <div className="tv-hero-actions">
          <button
            type="button"
            className="tv-hero-btn tv-hero-btn-primary glass-pill glass-pill--active"
            onClick={() => onPlay?.(game)}
          >
            ▶ {t(language, 'TXT_DOWNLOAD')}
          </button>
          <button
            type="button"
            className="tv-hero-btn tv-hero-btn-secondary glass-pill"
            onClick={() => onMoreInfo?.(game)}
          >
            ℹ {t(language, 'TXT_MORE_INFO')}
          </button>
          {game.description_id && (
            <button
              type="button"
              className="tv-description-toggle-inline glass-pill"
              onClick={() => setShowOriginal((value) => !value)}
            >
              {t(language, showOriginal ? 'TXT_TRANSLATED_DESCRIPTION' : 'TXT_ORIGINAL_DESCRIPTION')} [I]
            </button>
          )}
        </div>
      )}
    </section>
  );
};
