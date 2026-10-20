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

export const HeroBanner: React.FC<HeroBannerProps> = ({
  game,
  language,
  heading,
  emptyMessage,
  showDate = true,
  onPlay,
  onMoreInfo
}) => {
  const [showOriginal, setShowOriginal] = useState(false);
  const [failedSources, setFailedSources] = useState<string[]>([]);

  useEffect(() => {
    setShowOriginal(false);
  }, [game?.id_code]);

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
      {source && (
        <div className="tv-hero-backdrop">
          <img
            key={`${game?.id_code}_${source}`}
            className="tv-hero-art tv-hero-crossfade"
            src={source}
            alt=""
            onError={() => setFailedSources((values) => [...values, source])}
          />
        </div>
      )}
      <div className="tv-hero-shade" />
      <div className="tv-wordmark">
        <strong>P</strong>
        <span>oldrive</span>
      </div>
      <h1>{game?.title || heading || t(language, 'TXT_LATEST_GAMES')}</h1>
      {game && (
        <div className="tv-hero-metadata">
          {game.id_code}　 v{game.version}　 {game.format.toUpperCase()}　 {game.size_gb.toFixed(1)} GB
          {game.rating_score ? `　 ★ ${game.rating_score.toFixed(1)}/10` : ''}
          {game.download_count ? `　 ↓ ${game.download_count.toLocaleString()}` : ''}
          {showDate && added ? `　 ${added}` : ''}
        </div>
      )}
      <div className="tv-hero-description" key={game?.id_code}>
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
            className="tv-hero-btn tv-hero-btn-primary"
            onClick={() => onPlay?.(game)}
          >
            ▶ {t(language, 'TXT_DOWNLOAD')}
          </button>
          <button
            type="button"
            className="tv-hero-btn tv-hero-btn-secondary"
            onClick={() => onMoreInfo?.(game)}
          >
            ℹ {t(language, 'TXT_MORE_INFO')}
          </button>
          {game.description_id && (
            <button
              type="button"
              className="tv-description-toggle-inline"
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
