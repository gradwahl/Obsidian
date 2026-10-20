import React, { useEffect, useState } from 'react';
import { GameItem, LanguageCode } from '../types';
import { t } from '../translations';
import { X, ExternalLink } from 'lucide-react';
import { isSameGame } from '../catalog';
import { CoverArt } from './GameCard';
import { nextCoverSource } from '../artworkMatches';

interface GameDetailModalProps {
  game: GameItem;
  catalog: GameItem[];
  neighborGames?: GameItem[];
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
  neighborGames,
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
  const [entryPage, setEntryPage] = useState(0);
  const [failedSources, setFailedSources] = useState<string[]>([]);
  useEffect(() => { setEntryPage(0); setActiveTab('install'); setSelectedScreenshot(null); }, [game]);

  const neighborCatalog = neighborGames || catalog;
  const selectedIndex = neighborCatalog.findIndex(item => item === game);
  const neighbors = selectedIndex < 0 ? [game, ...neighborCatalog.slice(0, 3)]
    : neighborCatalog.slice(selectedIndex, selectedIndex + 4);

  // Retained source packages for this title.
  const relatedPackages = catalog.filter((candidate) => isSameGame(game, candidate));

  // Related DLC
  const relatedDLC = catalog.filter((candidate) => {
    if (!candidate.is_dlc || game.is_dlc) return false;
    const parentId = (candidate.parent_title_id || candidate.id_code).toUpperCase();
    return parentId === game.id_code.toUpperCase();
  });

  const screenshots = game.screenshot_urls || [];
  const entryCount = activeTab === 'install' ? relatedPackages.length : activeTab === 'dlc' ? relatedDLC.length : screenshots.length;
  const coverSrc = nextCoverSource(game, failedSources, true);

  let descText = game.description;
  if (game.description_id && !showOriginalDesc) {
    descText = t(language, game.description_id);
  }

  return (
    <div className="tv-detail-overlay" role="dialog" aria-modal="true" aria-labelledby="tv-detail-title">
      <div className="tv-neighbors" aria-hidden="true">{neighbors.map((neighbor, index) =>
        <div key={index} className={`tv-neighbor${index === 0 ? ' tv-neighbor-selected' : ''}`}><CoverArt game={neighbor} /></div>
      )}</div>
      <div className="tv-detail-panel">

        {/* Modal Header with Backdrop Artwork */}
        <div className="tv-detail-header">
          {coverSrc && (
            <img
              src={coverSrc}
              onError={() => setFailedSources(values => [...values, coverSrc])}
              alt=""
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
          <div className="tv-detail-heading">
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
            <h2 id="tv-detail-title" className="tv-detail-title">
              {game.title}
            </h2>
          </div>
        </div>

        {/* Tab Navigation Bar */}
        <div className="tv-detail-tabs" role="tablist">
          <button
            role="tab" aria-selected={activeTab === 'install'} onClick={() => { setActiveTab('install'); setEntryPage(0); }}
            className={`pb-3 px-4 text-sm font-bold border-b-2 transition-all ${
              activeTab === 'install'
                ? 'border-[#e50914] text-white'
                : 'border-transparent text-[#8a8a95] hover:text-white'
            }`}
          >
            {t(language, 'TXT_GAME_INSTALL')}
          </button>
          <button
            role="tab" aria-selected={activeTab === 'dlc'} onClick={() => { setActiveTab('dlc'); setEntryPage(0); }}
            className={`pb-3 px-4 text-sm font-bold border-b-2 transition-all ${
              activeTab === 'dlc'
                ? 'border-[#e50914] text-white'
                : 'border-transparent text-[#8a8a95] hover:text-white'
            }`}
          >
            {t(language, 'TXT_GAME_DLC')} ({relatedDLC.length})
          </button>
          <button
            role="tab" aria-selected={activeTab === 'screenshots'} onClick={() => { setActiveTab('screenshots'); setEntryPage(0); }}
            className={`pb-3 px-4 text-sm font-bold border-b-2 transition-all ${
              activeTab === 'screenshots'
                ? 'border-[#e50914] text-white'
                : 'border-transparent text-[#8a8a95] hover:text-white'
            }`}
          >
            {t(language, 'TXT_GAME_SCREENSHOTS')} ({screenshots.length})
          </button>
          <button
            role="tab" aria-selected={activeTab === 'details'} onClick={() => { setActiveTab('details'); setEntryPage(0); }}
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
        <div className="tv-detail-body">
          {(activeTab === 'install' || activeTab === 'dlc') && (
            <div className="tv-package-list">
              {activeTab === 'dlc' && relatedDLC.length === 0 ?
                <p className="tv-panel-empty">{t(language, 'TXT_GAME_NO_DLC')}</p> :
                <div className="tv-package-grid">
                  {(activeTab === 'install' ? relatedPackages : relatedDLC).slice(entryPage * 3, entryPage * 3 + 3).map((pkg, index) => (
                    <button type="button" key={index} className="tv-package-card"
                      onClick={() => activeTab === 'install' ? onTriggerAction(pkg) : onSelectGame(pkg)}>
                      <CoverArt className="tv-package-cover" game={pkg} />
                      <span className="tv-package-name">{activeTab === 'dlc' ? pkg.title : pkg.source_name}</span>
                      <span className="tv-package-meta">{pkg.id_code} | {pkg.format.toUpperCase()} | {pkg.version}</span>
                      <span className="tv-package-action">{pkg.format === 'catalog' ? 'WEBSITE ONLY' : t(language, activeTab === 'dlc' ? 'TXT_GAME_DETAILS' : pkg.format === 'exfat' ? 'TXT_MOUNT' : 'TXT_DOWNLOAD')}</span>
                    </button>
                  ))}
                </div>}
              {activeTab === 'install' && <p className="tv-action-notice">{t(language, 'TXT_ACTION_UNAVAILABLE')}</p>}
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
                <div className="tv-screenshot-grid">
                  {screenshots.slice(entryPage * 3, entryPage * 3 + 3).map((url, idx) => (
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
              <div className="tv-detail-info">
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
                  {game.format === 'catalog' ? (
                    <div className="text-white text-[11px] mt-0.5">
                      No installer — public catalog listing only
                      {game.source_url && <a href={game.source_url} target="_blank" rel="noopener noreferrer"
                        className="block mt-1 text-[#aaaaff] underline">Open source page ↗</a>}
                    </div>
                  ) : <div className="text-white font-mono text-[11px] mt-0.5 truncate">{game.install_path}</div>}
                </div>
              </div>
            </div>
          )}
        </div>

        {activeTab !== 'details' && <div className="tv-entry-pagination">
          <span>{Math.min(entryPage * 3 + 1, entryCount)} / {entryCount}</span>
          <button type="button" aria-label="Previous entries" disabled={entryPage === 0} onClick={() => setEntryPage(value => value - 1)}>←</button>
          <button type="button" aria-label="Next entries" disabled={(entryPage + 1) * 3 >= entryCount} onClick={() => setEntryPage(value => value + 1)}>→</button>
        </div>}

        {/* Modal Footer with TV instructions */}
        <div className="tv-detail-footer">
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
          className="tv-screenshot-viewer"
          onClick={() => setSelectedScreenshot(null)}
        >
          <img
            src={selectedScreenshot}
            alt="Screenshot enlarged"
            className="tv-screenshot-full"
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
