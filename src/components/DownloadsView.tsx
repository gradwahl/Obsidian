import { gameSize } from '../gameList';
import React, { useState } from 'react';
import { LanguageCode, GameItem } from '../types';
import { t } from '../translations';
import { Pause, Play, X, Download, HardDrive, Activity, CheckCircle, AlertCircle } from 'lucide-react';

interface DownloadsViewProps {
  catalog: GameItem[];
  language: LanguageCode;
  onOpenGame?: (game: GameItem) => void;
}

type DownloadFilter = 'active' | 'completed' | 'errors';

export const DownloadsView: React.FC<DownloadsViewProps> = ({
  catalog,
  language,
  onOpenGame
}) => {
  const [activeFilter, setActiveFilter] = useState<DownloadFilter>('active');
  const [pausedTasks, setPausedTasks] = useState<Record<string, boolean>>({});

  // Dynamic filter lists from real catalog state
  const activeGames = catalog.filter((g) => g.state === 'downloading');
  const completedGames = catalog.filter((g) => g.state === 'installed');
  const errorGames = catalog.filter((g) => (g.state as string) === 'error');

  const activeCount = activeGames.length;
  const completedCount = completedGames.length;
  const errorCount = errorGames.length;

  const currentGames =
    activeFilter === 'active'
      ? activeGames
      : activeFilter === 'completed'
      ? completedGames
      : errorGames;

  const togglePause = (id: string) => {
    setPausedTasks((prev) => ({ ...prev, [id]: !prev[id] }));
  };

  return (
    <div className="tv-downloads-dashboard tv-page px-6 pt-4 pb-12 text-white">
      {/* Header & Filter Pills */}
      <div className="flex items-center justify-between mb-6">
        <div>
          <h2 className="text-2xl font-black tracking-tight text-white flex items-center gap-3">
            <Download className="w-6 h-6 text-[#e50914]" />
            <span>{t(language, 'TXT_NAV_DOWNLOADS')}</span>
          </h2>
          <p className="text-xs text-[#9a9aa8] mt-1">
            {t(language, 'TXT_DOWNLOADS_HELP')}
          </p>
        </div>

        {/* Filter Pills */}
        <div className="flex items-center gap-3">
          <button
            type="button"
            onClick={() => setActiveFilter('active')}
            className={`flex items-center gap-2 px-4 py-2 rounded-full text-xs font-bold transition-all ${
              activeFilter === 'active'
                ? 'bg-[#1e1e28] text-white border-2 border-[#e50914] shadow-[0_0_16px_rgba(229,9,20,0.4)]'
                : 'bg-[#14141d]/80 text-[#9a9aa8] border border-white/10 hover:border-white/20'
            }`}
          >
            <Activity className="w-3.5 h-3.5 text-[#e50914]" />
            <span>{t(language, 'TXT_STATUS_ACTIVE')} · {activeCount}</span>
          </button>

          <button
            type="button"
            onClick={() => setActiveFilter('completed')}
            className={`flex items-center gap-2 px-4 py-2 rounded-full text-xs font-bold transition-all ${
              activeFilter === 'completed'
                ? 'bg-[#1e1e28] text-white border-2 border-[#e50914] shadow-[0_0_16px_rgba(229,9,20,0.4)]'
                : 'bg-[#14141d]/80 text-[#9a9aa8] border border-white/10 hover:border-white/20'
            }`}
          >
            <CheckCircle className="w-3.5 h-3.5 text-[#22c55e]" />
            <span>{t(language, 'TXT_STATUS_COMPLETED')} · {completedCount}</span>
          </button>

          <button
            type="button"
            onClick={() => setActiveFilter('errors')}
            className={`flex items-center gap-2 px-4 py-2 rounded-full text-xs font-bold transition-all ${
              activeFilter === 'errors'
                ? 'bg-[#1e1e28] text-white border-2 border-[#e50914] shadow-[0_0_16px_rgba(229,9,20,0.4)]'
                : 'bg-[#14141d]/80 text-[#9a9aa8] border border-white/10 hover:border-white/20'
            }`}
          >
            <AlertCircle className="w-3.5 h-3.5 text-[#ef4444]" />
            <span>{t(language, 'TXT_STATUS_ERRORS')} · {errorCount}</span>
          </button>
        </div>
      </div>

      {/* Main 2-Column Dashboard */}
      <div className="grid grid-cols-1 lg:grid-cols-3 gap-8">
        {/* Left Column (Task List) - Spans 2 cols */}
        <div className="lg:col-span-2 space-y-4">
          {currentGames.length === 0 ? (
            <div className="bg-[#14141c]/85 backdrop-blur-md border border-white/10 rounded-[18px] p-8 text-center text-[#8a8a9a]">
              {t(language, 'TXT_NO_ACTIVE_DOWNLOADS')}
            </div>
          ) : (
            currentGames.map((game, idx) => {
              const isPaused = pausedTasks[game.id_code];
              const isCompleted = game.state === 'installed';
              const progress = isCompleted ? 100 : game.progress || 0;

              const stages = [
                { key: 'download', label: t(language, 'TXT_STAGE_DOWNLOADING') },
                { key: 'unpack', label: t(language, 'TXT_STAGE_UNPACKING') },
                { key: 'install', label: t(language, 'TXT_STAGE_INSTALLING') },
                { key: 'ready', label: t(language, 'TXT_STAGE_READY') }
              ];

              const activeStageIndex = isCompleted ? 3 : progress < 80 ? 0 : progress < 95 ? 1 : 2;

              return (
                <div
                  key={`${game.id_code}_${idx}`}
                  onClick={() => onOpenGame && onOpenGame(game)}
                  className="bg-[#14141c]/85 backdrop-blur-md border border-white/12 rounded-[18px] p-5 hover:border-white/25 transition-all cursor-pointer shadow-lg relative overflow-hidden group"
                >
                  <div className="flex items-start gap-4">
                    {/* Task Info & Stages */}
                    <div className="flex-1 min-w-0">
                      <div className="flex items-center justify-between gap-2 mb-2">
                        <h3 className="text-base font-bold text-white truncate group-hover:text-[#ff3b45] transition-colors">
                          {game.title}
                        </h3>
                        <span className="text-xs font-mono text-[#a0a0b0]">
                          {gameSize(game)}
                        </span>
                      </div>

                      {/* Installation Stage Pills */}
                      <div className="flex items-center gap-1.5 mb-4">
                        {stages.map((st, idx) => {
                          const isActive = activeStageIndex === idx;
                          const isDone = activeStageIndex > idx;

                          return (
                            <span
                              key={st.key}
                              className={`px-3 py-1 rounded-full text-[11px] font-semibold transition-all ${
                                isActive
                                  ? 'bg-[#e50914] text-white shadow-[0_0_12px_rgba(229,9,20,0.6)] font-bold'
                                  : isDone
                                  ? 'bg-[#1f2937] text-[#22c55e] border border-[#22c55e]/30'
                                  : 'bg-[#181822] text-[#6b7280] border border-white/5'
                              }`}
                            >
                              {st.label}
                            </span>
                          );
                        })}
                      </div>

                      {/* Progress Bar */}
                      <div className="w-full bg-[#1c1c28] rounded-full h-2.5 overflow-hidden border border-white/5 mb-3">
                        <div
                          className="h-full transition-all duration-300 rounded-full"
                          style={{
                            width: `${progress}%`,
                            background: isCompleted
                              ? '#22c55e'
                              : 'linear-gradient(90deg, #ff1e27 0%, #c80812 100%)',
                            boxShadow: isCompleted ? '0 0 12px rgba(34, 197, 94, 0.6)' : '0 0 12px rgba(229, 9, 20, 0.6)'
                          }}
                        />
                      </div>

                      {/* Stats & Actions Row */}
                      <div className="flex items-center justify-between text-xs text-[#a0a0b2] font-mono">
                        <div>
                          <span className="text-white font-bold">{progress}%</span>
                          <span className="mx-2">•</span>
                          <span>{isCompleted ? t(language, 'TXT_STAGE_READY') : isPaused ? t(language, 'TXT_PAUSE') : '1.10 GB/s'}</span>
                        </div>

                        {/* Round Action Buttons */}
                        {!isCompleted && (
                          <div className="flex items-center gap-2" onClick={(e) => e.stopPropagation()}>
                            <button
                              type="button"
                              onClick={() => togglePause(game.id_code)}
                              className="w-8 h-8 rounded-full bg-[#20202c] hover:bg-[#e50914] border border-white/15 hover:border-[#e50914] text-white flex items-center justify-center transition-all shadow-md active:scale-95"
                              title={isPaused ? t(language, 'TXT_RESUME') : t(language, 'TXT_PAUSE')}
                            >
                              {isPaused ? (
                                <Play className="w-3.5 h-3.5 fill-current" />
                              ) : (
                                <Pause className="w-3.5 h-3.5 fill-current" />
                              )}
                            </button>

                            <button
                              type="button"
                              className="w-8 h-8 rounded-full bg-[#20202c] hover:bg-[#ef4444] border border-white/15 hover:border-[#ef4444] text-white flex items-center justify-center transition-all shadow-md active:scale-95"
                              title={t(language, 'TXT_CANCEL')}
                            >
                              <X className="w-3.5 h-3.5" />
                            </button>
                          </div>
                        )}
                      </div>
                    </div>
                  </div>
                </div>
              );
            })
          )}
        </div>

        {/* Right Column (2 Glass Telemetry Widgets) */}
        <div className="space-y-6">
          {/* Top Widget: Download Speed Telemetry */}
          <div className="bg-[#14141c]/85 backdrop-blur-md border border-white/12 rounded-[18px] p-5 shadow-lg relative overflow-hidden">
            <div className="flex items-center justify-between mb-2">
              <h3 className="text-xs font-black uppercase tracking-wider text-[#a0a0b2] flex items-center gap-2">
                <Activity className="w-4 h-4 text-[#e50914]" />
                <span>{t(language, 'TXT_SPEED_DOWNLOAD')}</span>
              </h3>
              <span className={`inline-block w-2 h-2 rounded-full ${activeCount > 0 ? 'bg-[#22c55e] animate-pulse' : 'bg-[#505060]'}`} />
            </div>

            {/* Main Value */}
            <div className="text-3xl font-black text-white font-mono tracking-tight my-1">
              {activeCount > 0 ? '1.10' : '0.00'} <span className="text-lg font-bold text-[#e50914]">{activeCount > 0 ? 'GB/s' : 'MB/s'}</span>
            </div>

            <div className="text-[11px] text-[#9090a2] font-mono mb-4">
              {activeCount > 0
                ? t(language, 'TXT_SPEED_AVG_PEAK', '940 MB/s', '1.28 GB/s')
                : 'Avg: 0.00 MB/s · Peak: 0.00 MB/s'}
            </div>

            {/* Area Chart */}
            <div className="w-full h-24 relative mt-2">
              <svg className="w-full h-full overflow-visible" viewBox="0 0 360 100" preserveAspectRatio="none">
                <defs>
                  <linearGradient id="speedGrad" x1="0" y1="0" x2="0" y2="1">
                    <stop offset="0%" stopColor="#e50914" stopOpacity={activeCount > 0 ? '0.45' : '0.1'} />
                    <stop offset="100%" stopColor="#e50914" stopOpacity="0.0" />
                  </linearGradient>
                </defs>
                {activeCount > 0 ? (
                  <>
                    <path
                      d="M 0 100 L 0 70 L 40 55 L 80 30 L 120 45 L 160 20 L 200 35 L 240 15 L 280 25 L 320 10 L 360 18 L 360 100 Z"
                      fill="url(#speedGrad)"
                    />
                    <path
                      d="M 0 70 L 40 55 L 80 30 L 120 45 L 160 20 L 200 35 L 240 15 L 280 25 L 320 10 L 360 18"
                      fill="none"
                      stroke="#e50914"
                      strokeWidth="3"
                      strokeLinecap="round"
                      strokeLinejoin="round"
                    />
                  </>
                ) : (
                  <>
                    <path d="M 0 100 L 360 100 L 360 100 L 0 100 Z" fill="url(#speedGrad)" />
                    <path
                      d="M 0 96 L 360 96"
                      fill="none"
                      stroke="#e50914"
                      strokeWidth="2"
                      strokeOpacity="0.4"
                    />
                  </>
                )}
              </svg>
            </div>
          </div>

          {/* Bottom Widget: Internal SSD Storage */}
          <div className="bg-[#14141c]/85 backdrop-blur-md border border-white/12 rounded-[18px] p-5 shadow-lg">
            <div className="flex items-center justify-between mb-3">
              <h3 className="text-xs font-black uppercase tracking-wider text-[#a0a0b2] flex items-center gap-2">
                <HardDrive className="w-4 h-4 text-[#e50914]" />
                <span>{t(language, 'TXT_INTERNAL_SSD')}</span>
              </h3>
              <span className="text-xs font-mono font-bold text-[#8a8a9a]">{t(language, 'TXT_CONSOLE_ONLY')}</span>
            </div>

            {/* Empty Muted Progress Bar for Web Preview */}
            <div className="w-full h-4 bg-[#181822] rounded-full overflow-hidden flex border border-white/10 mb-4 p-0.5">
              <div className="h-full bg-[#2a2a38] rounded-full w-full" />
            </div>

            {/* Legend */}
            <div className="space-y-2 text-xs font-mono">
              <div className="flex items-center justify-between">
                <div className="flex items-center gap-2">
                  <span className="w-2.5 h-2.5 rounded-full bg-[#505064]" />
                  <span className="text-[#d0d0e0]">{t(language, 'TXT_GAMES_APPS')}</span>
                </div>
                <span className="font-bold text-[#707080]">N/A</span>
              </div>

              <div className="flex items-center justify-between">
                <div className="flex items-center gap-2">
                  <span className="w-2.5 h-2.5 rounded-full bg-[#505064]" />
                  <span className="text-[#d0d0e0]">{t(language, 'TXT_SYSTEM_FILES')}</span>
                </div>
                <span className="font-bold text-[#707080]">N/A</span>
              </div>

              <div className="flex items-center justify-between pt-1 border-t border-white/10">
                <div className="flex items-center gap-2">
                  <span className="w-2.5 h-2.5 rounded-full bg-[#20202e] border border-white/20" />
                  <span className="text-[#d0d0e0]">{t(language, 'TXT_FREE_SPACE')}</span>
                </div>
                <span className="font-bold text-[#707080]">N/A</span>
              </div>
            </div>
          </div>
        </div>
      </div>
    </div>
  );
};
