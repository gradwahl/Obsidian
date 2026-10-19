import React, { useState } from 'react';
import { GameItem, LanguageCode } from '../types';
import { t } from '../translations';
import { X, ExternalLink, Download, HardDrive } from 'lucide-react';
import { isSameGame } from '../catalog';

interface GameDetailModalProps {
  game: GameItem;
  catalog: GameItem[];
  language: LanguageCode;
  onClose: () => void;
  onSelectGame: (game: GameItem) => void;
  onTriggerAction: (game: GameItem) => void;
  parentGame?: GameItem | null;
  onBackToParent?: () => void;
}

export const GameDetailModal: React.FC<GameDetailModalProps> = ({
  game,
  catalog,
  language,
  onClose,
  onSelectGame,
  onTriggerAction,
  parentGame,
  onBackToParent
}) => {
  const [activeTab, setActiveTab] = useState<'install' | 'dlc' | 'screenshots' | 'details'>('install');
  const [selectedScreenshot, setSelectedScreenshot] = useState<string | null>(null);
  const [showOriginalDesc, setShowOriginalDesc] = useState(false);

  // Neighboring covers for the top preview strip (like SDL2 C version)
  const relatedPackages = catalog.filter((candidate) => isSameGame(game, candidate));

  // Related DLC
  const relatedDLC = catalog.filter((candidate) => {
    if (!candidate.is_dlc || game.is_dlc) return false;
    const parentId = (candidate.parent_title_id || candidate.id_code).toUpperCase();
    return parentId === game.id_code.toUpperCase();
  });

  const screenshots = game.screenshot_urls || [];
  const coverSrc = game.cover_local_path || game.cover_url || '';

  let descText = game.description;
  if (game.description_id && !showOriginalDesc) {
    descText = t(language, game.description_id);
  }

  return (
    <div className="fixed inset-0 z-50 flex items-center justify-center bg-black/85 backdrop-blur-md p-4 select-none">
      <div className="relative w-full max-w-5xl max-h-[92vh] bg-[#141418] border border-[#2b2b36] rounded-xl shadow-2xl flex flex-col overflow-hidden text-white animate-in fade-in zoom-in-95 duration-150">

        {/* Modal Header with Backdrop Artwork */}
        <div className="relative h-64 shrink-0 overflow-hidden bg-[#0d0d10] border-b border-[#22222c]">
          {coverSrc && (
            <img
              src={coverSrc}
              alt={game.title}
              className="w-full h-full object-cover opacity-25 filter blur-xs scale-105"
            />
          )}
          <div className="absolute inset-0 bg-gradient-to-t from-[#141418] via-[#141418]/80 to-transparent" />

          {/* Close button */}
          <button
            onClick={onClose}
            className="absolute top-4 right-4 p-2.5 rounded-full bg-black/60 hover:bg-[#e50914] text-white transition-colors z-20"
            title="Close [Escape / Circle]"
          >
            <X className="w-5 h-5" />
          </button>

          {/* Back to parent button if viewing DLC */}
          {parentGame && onBackToParent && (
            <button
              onClick={onBackToParent}
              className="absolute top-4 left-4 px-3 py-1.5 rounded-md bg-black/60 hover:bg-[#252530] text-xs font-semibold text-white transition-colors z-20 border border-white/20"
            >
              {t(language, 'TXT_LIBRARY_BACK')} ({parentGame.title})
            </button>
          )}

          {/* Title and metadata in header */}
          <div className="absolute bottom-5 left-6 right-6">
            <div className="flex items-center gap-2 mb-1.5">
              <span className="px-2 py-0.5 rounded text-[11px] font-mono font-bold bg-[#e50914] text-white">
                {game.format.toUpperCase()}
              </span>
              <span className="px-2 py-0.5 rounded text-[11px] font-mono font-semibold bg-[#22222c] text-[#b0b0bc] border border-white/10">
                {game.id_code}
              </span>
              <span className="text-xs text-[#8a8a94] font-medium">
                {game.version} • {game.size_gb.toFixed(1)} GB
              </span>
            </div>
            <h2 className="text-2xl md:text-3xl font-extrabold text-white tracking-tight line-clamp-2">
              {game.title}
            </h2>
          </div>
        </div>

        {/* Tab Navigation Bar */}
        <div className="flex items-center gap-2 px-6 pt-3 border-b border-[#202028] bg-[#111114]">
          <button
            onClick={() => setActiveTab('install')}
            className={`pb-3 px-4 text-sm font-bold border-b-2 transition-all ${
              activeTab === 'install'
                ? 'border-[#e50914] text-white'
                : 'border-transparent text-[#8a8a95] hover:text-white'
            }`}
          >
            {t(language, 'TXT_GAME_INSTALL')}
          </button>
          <button
            onClick={() => setActiveTab('dlc')}
            className={`pb-3 px-4 text-sm font-bold border-b-2 transition-all ${
              activeTab === 'dlc'
                ? 'border-[#e50914] text-white'
                : 'border-transparent text-[#8a8a95] hover:text-white'
            }`}
          >
            {t(language, 'TXT_GAME_DLC')} ({relatedDLC.length})
          </button>
          <button
            onClick={() => setActiveTab('screenshots')}
            className={`pb-3 px-4 text-sm font-bold border-b-2 transition-all ${
              activeTab === 'screenshots'
                ? 'border-[#e50914] text-white'
                : 'border-transparent text-[#8a8a95] hover:text-white'
            }`}
          >
            {t(language, 'TXT_GAME_SCREENSHOTS')} ({screenshots.length})
          </button>
          <button
            onClick={() => setActiveTab('details')}
            className={`pb-3 px-4 text-sm font-bold border-b-2 transition-all ${
              activeTab === 'details'
                ? 'border-[#e50914] text-white'
                : 'border-transparent text-[#8a8a95] hover:text-white'
            }`}
          >
            {t(language, 'TXT_GAME_DETAILS')}
          </button>
        </div>

        {/* Tab Content Body */}
        <div className="flex-1 overflow-y-auto p-6 min-h-[280px]">
          {/* 1. INSTALL TAB */}
          {activeTab === 'install' && (
            <div className="space-y-4">
              <div className="text-xs text-[#8a8a94] mb-2 font-mono">
                {t(language, 'TXT_ACTION_UNAVAILABLE')}
              </div>
              <div className="grid grid-cols-1 md:grid-cols-2 gap-3.5">
                {relatedPackages.map((pkg, idx) => (
                  <div
                    key={idx}
                    className="p-4 rounded-lg bg-[#18181f] border border-[#262632] hover:border-[#444455] flex items-center justify-between transition-all"
                  >
                    <div>
                      <div className="flex items-center gap-2 mb-1">
                        <span className="px-2 py-0.5 rounded text-[10px] font-bold bg-[#e50914] text-white">
                          {pkg.format.toUpperCase()}
                        </span>
                        <span className="text-xs font-mono font-semibold text-[#c0c0c8]">
                          {pkg.id_code}
                        </span>
                        <span className="text-xs text-[#71717a]">
                          {pkg.version}
                        </span>
                      </div>
                      <div className="text-xs font-medium text-white/90">
                        {pkg.source_name}
                      </div>
                      <div className="text-[11px] text-[#82828e] mt-0.5">
                        {pkg.size_gb.toFixed(1)} GB
                      </div>
                    </div>

                    <button
                      onClick={() => onTriggerAction(pkg)}
                      className="px-4 py-2 bg-white text-black font-bold text-xs rounded hover:bg-[#e2e2e2] active:scale-95 transition-all flex items-center gap-1.5 shrink-0"
                    >
                      {pkg.format === 'exfat' ? (
                        <HardDrive className="w-3.5 h-3.5" />
                      ) : (
                        <Download className="w-3.5 h-3.5" />
                      )}
                      <span>
                        {pkg.format === 'exfat'
                          ? t(language, 'TXT_MOUNT')
                          : t(language, 'TXT_DOWNLOAD')}
                      </span>
                    </button>
                  </div>
                ))}
              </div>
            </div>
          )}

          {/* 2. DLC TAB */}
          {activeTab === 'dlc' && (
            <div>
              {relatedDLC.length === 0 ? (
                <div className="text-center py-12 text-sm text-[#8a8a94]">
                  {t(language, 'TXT_GAME_NO_DLC')}
                </div>
              ) : (
                <div className="grid grid-cols-1 md:grid-cols-2 gap-3.5">
                  {relatedDLC.map((dlc, idx) => (
                    <div
                      key={idx}
                      onClick={() => onSelectGame(dlc)}
                      className="p-3.5 rounded-lg bg-[#18181f] border border-[#262632] hover:border-white cursor-pointer transition-all flex items-center gap-3.5"
                    >
                      <div className="w-20 h-14 rounded bg-[#101014] overflow-hidden shrink-0">
                        {dlc.cover_local_path || dlc.cover_url ? (
                          <img
                            src={dlc.cover_local_path || dlc.cover_url}
                            alt={dlc.title}
                            className="w-full h-full object-cover"
                          />
                        ) : (
                          <div className="w-full h-full flex items-center justify-center text-[10px] font-bold text-white/40 bg-zinc-800">
                            DLC
                          </div>
                        )}
                      </div>
                      <div className="flex-1 min-w-0">
                        <div className="text-xs font-bold text-white truncate">
                          {dlc.title}
                        </div>
                        <div className="text-[11px] font-mono text-[#8a8a94] mt-0.5">
                          {dlc.id_code} • {dlc.size_gb.toFixed(1)} GB
                        </div>
                      </div>
                    </div>
                  ))}
                </div>
              )}
            </div>
          )}

          {/* 3. SCREENSHOTS TAB */}
          {activeTab === 'screenshots' && (
            <div>
              {screenshots.length === 0 ? (
                <div className="text-center py-12 text-sm text-[#8a8a94]">
                  {t(language, 'TXT_GAME_NO_SCREENSHOTS')}
                </div>
              ) : (
                <div className="grid grid-cols-2 md:grid-cols-3 gap-3.5">
                  {screenshots.slice(0, 6).map((url, idx) => (
                    <div
                      key={idx}
                      onClick={() => setSelectedScreenshot(url)}
                      className="group relative aspect-video rounded-lg overflow-hidden bg-[#181820] border border-[#282834] hover:border-white cursor-pointer transition-all"
                    >
                      <img
                        src={url}
                        alt={`Screenshot ${idx + 1}`}
                        className="w-full h-full object-cover group-hover:scale-105 transition-transform duration-300"
                        onError={(e) => {
                          (e.target as HTMLElement).style.display = 'none';
                        }}
                      />
                      <div className="absolute inset-0 bg-black/40 opacity-0 group-hover:opacity-100 flex items-center justify-center transition-opacity">
                        <ExternalLink className="w-5 h-5 text-white drop-shadow" />
                      </div>
                    </div>
                  ))}
                </div>
              )}
            </div>
          )}

          {/* 4. DETAILS TAB */}
          {activeTab === 'details' && (
            <div className="space-y-5">
              <div>
                <div className="flex items-center justify-between mb-2">
                  <span className="text-xs font-bold uppercase tracking-wider text-[#8a8a94]">
                    {t(language, 'TXT_GAME_DETAILS')}
                  </span>
                  {game.description_id && (
                    <button
                      onClick={() => setShowOriginalDesc(!showOriginalDesc)}
                      className="text-xs font-mono text-[#e50914] hover:underline"
                    >
                      {showOriginalDesc
                        ? t(language, 'TXT_TRANSLATED_DESCRIPTION')
                        : t(language, 'TXT_ORIGINAL_DESCRIPTION')}
                    </button>
                  )}
                </div>
                <div className="p-4 rounded-lg bg-[#18181f] border border-[#242430] text-sm text-[#d0d0d8] leading-relaxed whitespace-pre-wrap">
                  {descText}
                </div>
              </div>

              {/* Technical package info */}
              <div className="grid grid-cols-2 md:grid-cols-4 gap-3 text-xs">
                <div className="p-3 rounded bg-[#18181f] border border-[#22222c]">
                  <div className="text-[#7c7c88] text-[10px] uppercase font-bold">SOURCE</div>
                  <div className="text-white font-semibold mt-0.5 truncate">{game.source_name}</div>
                </div>
                <div className="p-3 rounded bg-[#18181f] border border-[#22222c]">
                  <div className="text-[#7c7c88] text-[10px] uppercase font-bold">TITLE ID</div>
                  <div className="text-white font-mono font-semibold mt-0.5">{game.id_code}</div>
                </div>
                <div className="p-3 rounded bg-[#18181f] border border-[#22222c]">
                  <div className="text-[#7c7c88] text-[10px] uppercase font-bold">VERSION</div>
                  <div className="text-white font-mono font-semibold mt-0.5">{game.version}</div>
                </div>
                <div className="p-3 rounded bg-[#18181f] border border-[#22222c]">
                  <div className="text-[#7c7c88] text-[10px] uppercase font-bold">TARGET PATH</div>
                  <div className="text-white font-mono text-[11px] mt-0.5 truncate">{game.install_path}</div>
                </div>
              </div>
            </div>
          )}
        </div>

        {/* Modal Footer with TV instructions */}
        <div className="px-6 py-3 bg-[#0d0d10] border-t border-[#202028] flex items-center justify-between text-[11px] text-[#71717a]">
          <span className="truncate">{t(language, 'TXT_GAME_PANEL_HINT')}</span>
          <button
            onClick={onClose}
            className="px-3 py-1 rounded bg-[#22222c] hover:bg-[#32323e] text-white text-xs font-semibold"
          >
            {t(language, 'TXT_BACK_HINT')} [Esc]
          </button>
        </div>
      </div>

      {/* Screenshot lightbox zoom */}
      {selectedScreenshot && (
        <div
          className="fixed inset-0 z-60 bg-black/95 flex items-center justify-center p-6"
          onClick={() => setSelectedScreenshot(null)}
        >
          <img
            src={selectedScreenshot}
            alt="Screenshot enlarged"
            className="max-w-full max-h-[90vh] object-contain rounded-lg shadow-2xl"
          />
          <button
            onClick={() => setSelectedScreenshot(null)}
            className="absolute top-6 right-6 p-3 rounded-full bg-black/60 text-white hover:bg-[#e50914]"
          >
            <X className="w-6 h-6" />
          </button>
        </div>
      )}
    </div>
  );
};
