import React, { useState } from 'react';
import { GameItem, LanguageCode } from '../types';
import { t } from '../translations';
import { Download, Info, RefreshCw, HardDrive } from 'lucide-react';

interface HeroBannerProps {
  game: GameItem | null;
  language: LanguageCode;
  onOpenDetails: (game: GameItem) => void;
  onTriggerAction: (game: GameItem) => void;
  onRefreshCatalog: () => void;
  syncInProgress: boolean;
}

export const HeroBanner: React.FC<HeroBannerProps> = ({
  game,
  language,
  onOpenDetails,
  onTriggerAction,
  onRefreshCatalog,
  syncInProgress
}) => {
  const [showOriginal, setShowOriginal] = useState(false);

  if (!game) {
    return (
      <div className="relative h-[440px] w-full flex items-center justify-center bg-gradient-to-b from-[#141418] to-[#08080a] border-b border-[#1f1f26]">
        <div className="text-center p-6 max-w-md">
          <div className="text-[#858590] text-sm mb-3">
            {t(language, 'TXT_NO_LATEST_GAMES')}
          </div>
          <button
            onClick={onRefreshCatalog}
            disabled={syncInProgress}
            className="inline-flex items-center gap-2 px-5 py-2.5 bg-[#e50914] hover:bg-[#b80710] text-white font-semibold rounded text-sm transition-all"
          >
            <RefreshCw className={`w-4 h-4 ${syncInProgress ? 'animate-spin' : ''}`} />
            <span>{t(language, 'TXT_REFRESH_HINT')}</span>
          </button>
        </div>
      </div>
    );
  }

  // Cover image URL or fallback to Wolodrive gradient
  const coverSrc = game.cover_local_path || game.cover_url || '';
  const formatBadge =
    game.format === 'exfat' ? 'EXFAT' : game.format === 'ffpkg' ? 'FFPKG' : 'FPKG';

  // Description text
  let descText = game.description;
  if (game.description_id && !showOriginal) {
    descText = t(language, game.description_id);
  }

  return (
    <div className="relative h-[440px] w-full overflow-hidden select-none border-b border-[#1c1c22]">
      {/* Background Cover Artwork */}
      <div className="absolute inset-0">
        {coverSrc ? (
          <img
            src={coverSrc}
            alt={game.title}
            className="w-full h-full object-cover object-center opacity-30 filter blur-sm scale-105 transition-all duration-700"
            onError={(e) => {
              (e.target as HTMLElement).style.display = 'none';
            }}
          />
        ) : null}

        {/* Gradient overlays to match PS5 / Netflix aesthetic */}
        <div
          className="absolute inset-0"
          style={{
            background: `linear-gradient(to right, #08080a 20%, rgba(8, 8, 10, 0.85) 50%, rgba(8, 8, 10, 0.4) 100%),
                         linear-gradient(to bottom, transparent 40%, #08080a 100%)`
          }}
        />
        {/* Accent tint from game color */}
        <div
          className="absolute inset-0 opacity-15 pointer-events-none"
          style={{
            backgroundColor: `rgb(${game.cover_r}, ${game.cover_g}, ${game.cover_b})`
          }}
        />
      </div>

      {/* Content Container */}
      <div className="relative z-10 h-full max-w-5xl px-8 flex flex-col justify-center">
        {/* Source and Subtitle */}
        <div className="flex items-center gap-3 text-xs tracking-wider text-[#94949c] font-medium uppercase mb-2">
          <span className="text-[#e50914] font-bold">WOLODRIVE STORE</span>
          <span>•</span>
          <span>{t(language, 'TXT_SOURCE', game.source_name)}</span>
        </div>

        {/* Game Title */}
        <h1 className="text-4xl md:text-5xl font-black text-white tracking-tight leading-tight line-clamp-2 max-w-3xl drop-shadow-md">
          {game.title}
        </h1>

        {/* Metadata badges */}
        <div className="flex flex-wrap items-center gap-2.5 mt-3 mb-4">
          <span className="px-2.5 py-0.5 rounded text-xs font-mono font-bold bg-[#1f1f26] text-white border border-[#33333f]">
            {game.id_code}
          </span>
          <span className="px-2.5 py-0.5 rounded text-xs font-bold bg-[#e50914] text-white tracking-wider">
            {formatBadge}
          </span>
          <span className="px-2.5 py-0.5 rounded text-xs font-mono bg-[#18181e] text-[#a0a0aa] border border-[#262630]">
            {game.version}
          </span>
          <span className="px-2.5 py-0.5 rounded text-xs font-medium bg-[#18181e] text-[#a0a0aa] border border-[#262630]">
            {t(language, 'TXT_SIZE', game.size_gb)}
          </span>
        </div>

        {/* Description & Toggle */}
        <div className="max-w-2xl text-sm text-[#c8c8d0] leading-relaxed line-clamp-3 mb-6">
          {descText}
        </div>

        {/* Action Buttons */}
        <div className="flex flex-wrap items-center gap-3">
          <button
            onClick={() => onTriggerAction(game)}
            className="flex items-center gap-2.5 px-6 py-3 bg-white text-black font-extrabold rounded hover:bg-[#e0e0e0] active:scale-95 transition-all shadow-lg text-sm tracking-wide"
          >
            {game.format === 'exfat' ? (
              <HardDrive className="w-4 h-4 text-black" />
            ) : (
              <Download className="w-4 h-4 text-black" />
            )}
            <span>
              {game.format === 'exfat'
                ? t(language, 'TXT_MOUNT')
                : t(language, 'TXT_DOWNLOAD')}
            </span>
          </button>

          <button
            onClick={() => onOpenDetails(game)}
            className="flex items-center gap-2 px-5 py-3 bg-[#26262e]/80 hover:bg-[#34343f] text-white font-semibold rounded active:scale-95 transition-all border border-[#40404f] text-sm"
          >
            <Info className="w-4 h-4 text-[#e50914]" />
            <span>{t(language, 'TXT_GAME_DETAILS')}</span>
          </button>

          {game.description_id && (
            <button
              onClick={() => setShowOriginal(!showOriginal)}
              className="px-3.5 py-3 text-xs font-mono text-[#94949c] hover:text-white bg-[#1a1a20]/60 hover:bg-[#25252e] rounded border border-[#30303c] transition-colors"
              title="Toggle Description [I]"
            >
              {showOriginal
                ? t(language, 'TXT_TRANSLATED_DESCRIPTION')
                : t(language, 'TXT_ORIGINAL_DESCRIPTION')}
            </button>
          )}

          <button
            onClick={onRefreshCatalog}
            disabled={syncInProgress}
            className="p-3 text-[#94949c] hover:text-white bg-[#1a1a20]/60 hover:bg-[#25252e] rounded border border-[#30303c] transition-colors"
            title={t(language, 'TXT_REFRESH_HINT')}
          >
            <RefreshCw className={`w-4 h-4 ${syncInProgress ? 'animate-spin text-[#e50914]' : ''}`} />
          </button>
        </div>
      </div>
    </div>
  );
};
