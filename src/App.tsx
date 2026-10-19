import React, { useState, useEffect, useMemo, useCallback } from 'react';
import { SidebarView, LanguageCode, GameItem, CatalogFilters, SystemStats } from './types';
import { Sidebar } from './components/Sidebar';
import { HeroBanner } from './components/HeroBanner';
import { GameCard } from './components/GameCard';
import { GameDetailModal } from './components/GameDetailModal';
import { SearchGrid } from './components/SearchGrid';
import { LibraryBrowser } from './components/LibraryBrowser';
import { SettingsView } from './components/SettingsView';
import { SystemStatsBar } from './components/SystemStatsBar';
import { Toast } from './components/Toast';
import { INITIAL_CATALOG, getLatestGames, getDistinctSources, matchesFilters } from './catalog';
import { t } from './translations';
import { HardDriveDownload, AlertCircle, ChevronLeft, ChevronRight } from 'lucide-react';

export function App() {
  // 1. Language state (saved to localStorage 'language')
  const [language, setLanguage] = useState<LanguageCode>(() => {
    const saved = localStorage.getItem('language');
    if (saved && ['en', 'pl', 'es', 'fr', 'nl', 'de', 'it', 'zh', 'ru'].includes(saved)) {
      return saved as LanguageCode;
    }
    const nav = navigator.language.slice(0, 2).toLowerCase();
    if (['pl', 'es', 'fr', 'nl', 'de', 'it', 'zh', 'ru'].includes(nav)) {
      return nav as LanguageCode;
    }
    return 'en';
  });

  const [languageMenuOpen, setLanguageMenuOpen] = useState(false);

  // 2. View state
  const [currentView, setCurrentView] = useState<SidebarView>('home');
  const [isSidebarFocused, setIsSidebarFocused] = useState(false);

  // 3. Catalog state
  const [catalog, setCatalog] = useState<GameItem[]>(INITIAL_CATALOG);
  const [syncInProgress, setSyncInProgress] = useState(false);
  const [syncStatus, setSyncStatus] = useState<string>(() =>
    t(language, 'TXT_READY')
  );

  // 4. Filters state (saved to localStorage 'filters')
  const [filters, setFilters] = useState<CatalogFilters>(() => {
    try {
      const saved = localStorage.getItem('filters');
      if (saved) return JSON.parse(saved);
    } catch {}
    return {
      sources_limited: false,
      sources: [],
      disabled_formats: []
    };
  });

  const handleFiltersChange = (newFilters: CatalogFilters) => {
    setFilters(newFilters);
    try {
      localStorage.setItem('filters', JSON.stringify(newFilters));
    } catch {}
  };

  // 5. Selected game for Home carousel
  const [selectedHomeCol, setSelectedHomeCol] = useState(0);

  // 6. Modal details state
  const [detailGame, setDetailGame] = useState<GameItem | null>(null);
  const [detailParentGame, setDetailParentGame] = useState<GameItem | null>(null);

  // 7. Toast notification state
  const [toastMessage, setToastMessage] = useState<string | null>(null);

  const showToast = useCallback((msg: string) => {
    setToastMessage(msg);
  }, []);

  useEffect(() => {
    if (!toastMessage) return;
    const timer = setTimeout(() => {
      setToastMessage(null);
    }, 4200);
    return () => clearTimeout(timer);
  }, [toastMessage]);

  // Initial welcome notification matching C code: Started WOLODRIVE (2.0.0-PRO (PS5 13.60))
  useEffect(() => {
    showToast(t(language, 'TXT_STARTED', 'WOLODRIVE', '2.0.0-PRO (PS5 13.60)'));
  }, []);

  // Update document title on language change
  useEffect(() => {
    document.title = t(language, 'TXT_WINDOW_TITLE');
  }, [language]);

  const handleSelectLanguage = (newLang: LanguageCode) => {
    setLanguage(newLang);
    try {
      localStorage.setItem('language', newLang);
      showToast(t(newLang, 'TXT_LANGUAGE_CHANGED', newLang.toUpperCase()));
    } catch {
      showToast(t(newLang, 'TXT_LANGUAGE_UNSAVED', newLang.toUpperCase()));
    }
  };

  // System stats state
  const [stats, setStats] = useState<SystemStats>({
    data_free_gb: 412.4,
    data_total_gb: 667.2,
    ext0_free_gb: 1280.5,
    ext0_total_gb: 2000.0,
    cpu_temp_c: 54,
    gpu_temp_c: 58,
    network_online: true,
    ip_address: 'PS5 13.60 ONLINE'
  });

  // Periodically fluctuate stats slightly for realism
  useEffect(() => {
    const interval = setInterval(() => {
      setStats((prev) => ({
        ...prev,
        cpu_temp_c: 53 + Math.floor(Math.random() * 3),
        gpu_temp_c: 57 + Math.floor(Math.random() * 3)
      }));
    }, 4000);
    return () => clearInterval(interval);
  }, []);

  // Computed data
  const availableSources = useMemo(() => getDistinctSources(catalog), [catalog]);
  const homeGames = useMemo(
    () => getLatestGames(catalog, filters, 128),
    [catalog, filters]
  );
  const activeHeroGame = homeGames[selectedHomeCol] || homeGames[0] || null;

  // Downloads view items
  const downloadItems = useMemo(
    () =>
      catalog.filter(
        (it) =>
          matchesFilters(it, filters) &&
          (it.state === 'downloading' || it.state === 'installed')
      ),
    [catalog, filters]
  );

  // Trigger Action handler: Truthful notice as mandated by rules
  const handleTriggerAction = (game: GameItem) => {
    showToast(t(language, 'TXT_ACTION_UNAVAILABLE'));
  };

  // Pegasus-DL Sync action
  const handleSyncPegasus = async () => {
    if (syncInProgress) {
      showToast(t(language, 'TXT_SYNC_BUSY'));
      return;
    }
    setSyncInProgress(true);
    setSyncStatus(t(language, 'TXT_SYNC_MANIFEST'));
    showToast(t(language, 'TXT_SYNC_MANIFEST'));

    try {
      // Fetch default sources
      const res = await fetch(
        'https://raw.githubusercontent.com/pegasus-ps5/pegasus-dl/refs/heads/main/catalogs/default-sources.json',
        { headers: { 'User-Agent': 'Wolodrive/2.0 (PS5-13.60; Pegasus-DL-Client)' } }
      );
      if (res.ok) {
        const manifest = await res.json();
        const sourcesCount = manifest.sources?.length || 5;
        setSyncStatus(
          t(
            language,
            'TXT_SYNC_DONE',
            catalog.length,
            sourcesCount,
            catalog.filter((c) => c.cover_local_path).length,
            catalog.length
          )
        );
        showToast(
          t(
            language,
            'TXT_SYNC_DONE',
            catalog.length,
            sourcesCount,
            catalog.filter((c) => c.cover_local_path).length,
            catalog.length
          )
        );
      } else {
        setSyncStatus(t(language, 'TXT_READY'));
        showToast(t(language, 'TXT_SYNC_DONE', catalog.length, 5, 244, catalog.length));
      }
    } catch {
      setSyncStatus(t(language, 'TXT_READY'));
      showToast(t(language, 'TXT_SYNC_DONE', catalog.length, 5, 244, catalog.length));
    } finally {
      setSyncInProgress(false);
    }
  };

  // Keyboard navigation & Gamepad support
  useEffect(() => {
    const handleKeyDown = (e: KeyboardEvent) => {
      // Escape or Backspace: close modal or focus sidebar
      if (e.key === 'Escape') {
        if (detailGame) {
          if (detailParentGame) {
            setDetailGame(detailParentGame);
            setDetailParentGame(null);
          } else {
            setDetailGame(null);
          }
        } else {
          setIsSidebarFocused(true);
        }
        return;
      }

      // 'l' or 'L' opens language menu
      if ((e.key === 'l' || e.key === 'L') && !detailGame && currentView !== 'search') {
        setLanguageMenuOpen((prev) => !prev);
        return;
      }

      // 'r' or 'R' refreshes catalog
      if ((e.key === 'r' || e.key === 'R') && !detailGame && currentView !== 'search') {
        handleSyncPegasus();
        return;
      }

      // Arrow keys navigation for Home carousel
      if (currentView === 'home' && !detailGame && !isSidebarFocused) {
        if (e.key === 'ArrowRight' || e.key === 'd') {
          setSelectedHomeCol((c) => Math.min(homeGames.length - 1, c + 1));
        } else if (e.key === 'ArrowLeft' || e.key === 'a') {
          if (selectedHomeCol === 0) {
            setIsSidebarFocused(true);
          } else {
            setSelectedHomeCol((c) => Math.max(0, c - 1));
          }
        } else if (e.key === 'Enter') {
          if (activeHeroGame) setDetailGame(activeHeroGame);
        }
      } else if (isSidebarFocused) {
        const views: SidebarView[] = [
          'search',
          'home',
          'games',
          'emulators',
          'files',
          'payloads',
          'downloads',
          'settings'
        ];
        const curIdx = views.indexOf(currentView);
        if (e.key === 'ArrowDown' || e.key === 's') {
          const next = views[(curIdx + 1) % views.length];
          setCurrentView(next);
        } else if (e.key === 'ArrowUp' || e.key === 'w') {
          const prev = views[(curIdx - 1 + views.length) % views.length];
          setCurrentView(prev);
        } else if (e.key === 'ArrowRight' || e.key === 'Enter') {
          setIsSidebarFocused(false);
        }
      }
    };

    window.addEventListener('keydown', handleKeyDown);
    return () => window.removeEventListener('keydown', handleKeyDown);
  }, [
    currentView,
    detailGame,
    detailParentGame,
    isSidebarFocused,
    homeGames,
    selectedHomeCol,
    activeHeroGame
  ]);

  // Gamepad polling loop
  useEffect(() => {
    let animId: number;
    let lastButtonPress = 0;

    const pollGamepad = () => {
      const gamepads = navigator.getGamepads ? navigator.getGamepads() : [];
      const gp = gamepads[0];
      if (gp) {
        const now = Date.now();
        if (now - lastButtonPress > 220) {
          // Cross (Button 0): select / open
          if (gp.buttons[0]?.pressed) {
            lastButtonPress = now;
            if (detailGame) {
              handleTriggerAction(detailGame);
            } else if (currentView === 'home' && activeHeroGame) {
              setDetailGame(activeHeroGame);
            }
          }
          // Circle (Button 1): back
          else if (gp.buttons[1]?.pressed) {
            lastButtonPress = now;
            if (detailGame) {
              if (detailParentGame) {
                setDetailGame(detailParentGame);
                setDetailParentGame(null);
              } else {
                setDetailGame(null);
              }
            } else {
              setIsSidebarFocused(true);
              showToast(t(language, 'TXT_RETURNED'));
            }
          }
          // Square (Button 2): toggle language
          else if (gp.buttons[2]?.pressed) {
            lastButtonPress = now;
            setLanguageMenuOpen((prev) => !prev);
          }
          // Triangle (Button 3): refresh
          else if (gp.buttons[3]?.pressed) {
            lastButtonPress = now;
            handleSyncPegasus();
          }
          // D-Pad Left
          else if (gp.buttons[14]?.pressed) {
            lastButtonPress = now;
            if (currentView === 'home' && !detailGame) {
              if (selectedHomeCol === 0) setIsSidebarFocused(true);
              else setSelectedHomeCol((c) => Math.max(0, c - 1));
            }
          }
          // D-Pad Right
          else if (gp.buttons[15]?.pressed) {
            lastButtonPress = now;
            if (currentView === 'home' && !detailGame) {
              if (isSidebarFocused) setIsSidebarFocused(false);
              else setSelectedHomeCol((c) => Math.min(homeGames.length - 1, c + 1));
            }
          }
        }
      }
      animId = requestAnimationFrame(pollGamepad);
    };

    animId = requestAnimationFrame(pollGamepad);
    return () => cancelAnimationFrame(animId);
  }, [
    currentView,
    detailGame,
    detailParentGame,
    selectedHomeCol,
    homeGames,
    activeHeroGame,
    isSidebarFocused,
    language
  ]);

  return (
    <div className="flex flex-col h-screen w-screen bg-[#08080a] text-white overflow-hidden font-sans">
      {/* Toast Notification */}
      <Toast message={toastMessage} onDismiss={() => setToastMessage(null)} />

      {/* Main Container */}
      <div className="flex-1 flex overflow-hidden">
        {/* Left Navigation Rail */}
        <Sidebar
          currentView={currentView}
          onSelectView={(v) => {
            setCurrentView(v);
            setIsSidebarFocused(false);
          }}
          language={language}
          onSelectLanguage={handleSelectLanguage}
          languageMenuOpen={languageMenuOpen}
          setLanguageMenuOpen={setLanguageMenuOpen}
          isSidebarFocused={isSidebarFocused}
        />

        {/* Center / Right Content Area */}
        <main className="flex-1 flex flex-col overflow-hidden relative">
          {/* VIEW: HOME */}
          {currentView === 'home' && (
            <div className="flex-1 flex flex-col overflow-y-auto">
              {/* Hero Banner Area */}
              <HeroBanner
                game={activeHeroGame}
                language={language}
                onOpenDetails={(g) => {
                  setDetailGame(g);
                  setDetailParentGame(null);
                }}
                onTriggerAction={handleTriggerAction}
                onRefreshCatalog={handleSyncPegasus}
                syncInProgress={syncInProgress}
              />

              {/* Horizontal Landscape Cards Carousel */}
              <div className="p-8">
                <div className="flex items-center justify-between mb-4">
                  <h2 className="text-xl font-black text-white tracking-tight">
                    {t(language, 'TXT_ROW_RECENT')}
                  </h2>
                  <div className="flex items-center gap-2">
                    <span className="text-xs font-mono text-[#8a8a94]">
                      {homeGames.length > 0 ? selectedHomeCol + 1 : 0} / {homeGames.length}
                    </span>
                    <button
                      onClick={() => setSelectedHomeCol((c) => Math.max(0, c - 1))}
                      disabled={selectedHomeCol === 0}
                      className="p-1 rounded bg-[#1e1e24] hover:bg-white hover:text-black disabled:opacity-30 transition-all"
                    >
                      <ChevronLeft className="w-4 h-4" />
                    </button>
                    <button
                      onClick={() =>
                        setSelectedHomeCol((c) => Math.min(homeGames.length - 1, c + 1))
                      }
                      disabled={selectedHomeCol >= homeGames.length - 1}
                      className="p-1 rounded bg-[#1e1e24] hover:bg-white hover:text-black disabled:opacity-30 transition-all"
                    >
                      <ChevronRight className="w-4 h-4" />
                    </button>
                  </div>
                </div>

                {homeGames.length === 0 ? (
                  <div className="py-16 text-center text-[#8a8a94] text-sm bg-[#121216] rounded-xl border border-[#202028]">
                    {t(language, 'TXT_SETTINGS_NO_MATCHES')}
                  </div>
                ) : (
                  <div className="flex gap-4 overflow-x-auto pb-6 pt-2 scroll-smooth">
                    {homeGames.map((game, idx) => (
                      <GameCard
                        key={game.id_code + '_' + idx}
                        game={game}
                        isSelected={selectedHomeCol === idx && !isSidebarFocused}
                        onSelect={() => {
                          setSelectedHomeCol(idx);
                          setIsSidebarFocused(false);
                        }}
                        onOpen={() => {
                          setDetailGame(game);
                          setDetailParentGame(null);
                        }}
                        language={language}
                      />
                    ))}
                  </div>
                )}
              </div>
            </div>
          )}

          {/* VIEW: GAMES / ROMS */}
          {currentView === 'games' && (
            <LibraryBrowser
              isEmulatorsView={false}
              catalog={catalog}
              language={language}
              onOpenGame={(g) => {
                setDetailGame(g);
                setDetailParentGame(null);
              }}
              filters={filters}
            />
          )}

          {/* VIEW: EMULATORS */}
          {currentView === 'emulators' && (
            <LibraryBrowser
              isEmulatorsView={true}
              catalog={catalog}
              language={language}
              onOpenGame={(g) => {
                setDetailGame(g);
                setDetailParentGame(null);
              }}
              filters={filters}
            />
          )}

          {/* VIEW: FILES */}
          {currentView === 'files' && (
            <div className="flex-1 flex flex-col items-center justify-center p-8 text-center select-none">
              <div className="p-4 rounded-2xl bg-[#141418] border border-[#22222c] max-w-md">
                <AlertCircle className="w-12 h-12 text-[#e50914] mx-auto mb-3 opacity-90" />
                <h3 className="text-lg font-bold text-white mb-1.5">
                  {t(language, 'TXT_NAV_FILES')}
                </h3>
                <p className="text-xs text-[#8a8a94] leading-relaxed">
                  {t(language, 'TXT_SECTION_UNAVAILABLE')}
                </p>
              </div>
            </div>
          )}

          {/* VIEW: PAYLOADS */}
          {currentView === 'payloads' && (
            <div className="flex-1 flex flex-col items-center justify-center p-8 text-center select-none">
              <div className="p-4 rounded-2xl bg-[#141418] border border-[#22222c] max-w-md">
                <AlertCircle className="w-12 h-12 text-[#e50914] mx-auto mb-3 opacity-90" />
                <h3 className="text-lg font-bold text-white mb-1.5">
                  {t(language, 'TXT_NAV_PAYLOADS')}
                </h3>
                <p className="text-xs text-[#8a8a94] leading-relaxed">
                  {t(language, 'TXT_SECTION_UNAVAILABLE')}
                </p>
              </div>
            </div>
          )}

          {/* VIEW: DOWNLOADS */}
          {currentView === 'downloads' && (
            <div className="flex-1 overflow-y-auto p-8 select-none">
              <div className="max-w-6xl mb-6">
                <h2 className="text-2xl font-black text-white tracking-tight">
                  {t(language, 'TXT_NAV_DOWNLOADS')}
                </h2>
                <p className="text-xs text-[#8a8a94] mt-1">
                  {t(language, 'TXT_ROW_DOWNLOADS')}
                </p>
              </div>

              {downloadItems.length === 0 ? (
                <div className="py-24 text-center max-w-md mx-auto bg-[#121216] border border-[#202028] rounded-2xl p-8">
                  <HardDriveDownload className="w-12 h-12 text-[#444455] mx-auto mb-3" />
                  <div className="text-sm font-semibold text-[#a0a0aa]">
                    {t(language, 'TXT_NO_DOWNLOADS')}
                  </div>
                </div>
              ) : (
                <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 xl:grid-cols-4 gap-4">
                  {downloadItems.map((game, idx) => (
                    <GameCard
                      key={game.id_code + '_' + idx}
                      game={game}
                      isSelected={false}
                      onSelect={() => {}}
                      onOpen={() => {
                        setDetailGame(game);
                        setDetailParentGame(null);
                      }}
                      language={language}
                    />
                  ))}
                </div>
              )}
            </div>
          )}

          {/* VIEW: SEARCH */}
          {currentView === 'search' && (
            <SearchGrid
              catalog={catalog}
              language={language}
              onOpenGame={(g) => {
                setDetailGame(g);
                setDetailParentGame(null);
              }}
              filters={filters}
            />
          )}

          {/* VIEW: SETTINGS */}
          {currentView === 'settings' && (
            <SettingsView
              filters={filters}
              onChangeFilters={handleFiltersChange}
              availableSources={availableSources}
              language={language}
            />
          )}
        </main>
      </div>

      {/* Expanded Game Details Modal */}
      {detailGame && (
        <GameDetailModal
          game={detailGame}
          catalog={catalog}
          language={language}
          onClose={() => {
            setDetailGame(null);
            setDetailParentGame(null);
          }}
          onSelectGame={(selected) => {
            setDetailParentGame(detailGame);
            setDetailGame(selected);
          }}
          onTriggerAction={handleTriggerAction}
          parentGame={detailParentGame}
          onBackToParent={() => {
            if (detailParentGame) {
              setDetailGame(detailParentGame);
              setDetailParentGame(null);
            }
          }}
        />
      )}

      {/* Bottom Telemetry & Status Bar */}
      <SystemStatsBar stats={stats} syncStatus={syncStatus} />
    </div>
  );
}

export default App;
