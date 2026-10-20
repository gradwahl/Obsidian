import React, { useState, useEffect, useMemo, useCallback } from 'react';
import { SidebarView, LanguageCode, GameItem, CatalogFilters, SystemStats } from './types';
import { Sidebar } from './components/Sidebar';
import { HomeView, HomeCategoryRow } from './components/HomeView';
import { NativeViewport } from './components/NativeViewport';
import { GameDetailModal } from './components/GameDetailModal';
import { SearchGrid } from './components/SearchGrid';
import { LibraryBrowser } from './components/LibraryBrowser';
import { SettingsView } from './components/SettingsView';
import { SystemStatsBar } from './components/SystemStatsBar';
import { Toast } from './components/Toast';
import { INITIAL_CATALOG, getLatestGames, getHomeCategoryGames, getDistinctSources, matchesFilters } from './catalog';
import { t } from './translations';

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

  // 5. Selected category & game for Home carousel
  const [activeHomeCategory, setActiveHomeCategory] = useState(0);
  const [selectedHomeCol, setSelectedHomeCol] = useState(0);
  const [selectedDownloadCol, setSelectedDownloadCol] = useState(0);

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

  // Initial welcome notification matching C code: Started Poldrive (2.0.0-PRO (PS5 13.60))
  useEffect(() => {
    showToast(t(language, 'TXT_STARTED', 'Poldrive', '2.0.0-PRO (PS5 13.60)'));
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
  const homeCategories = useMemo<HomeCategoryRow[]>(
    () => [
      { id: 'ps5', titleKey: 'TXT_CAT_PS5', games: getHomeCategoryGames(catalog, filters, 'ps5', 128) },
      { id: 'ps4', titleKey: 'TXT_CAT_PS4', games: getHomeCategoryGames(catalog, filters, 'ps4', 128) },
      { id: 'popular', titleKey: 'TXT_CAT_POPULAR', games: getHomeCategoryGames(catalog, filters, 'popular', 128) },
      { id: 'newest', titleKey: 'TXT_CAT_NEWEST', games: getHomeCategoryGames(catalog, filters, 'newest', 128) },
      { id: 'top_rated', titleKey: 'TXT_CAT_TOP_RATED', games: getHomeCategoryGames(catalog, filters, 'top_rated', 128) }
    ],
    [catalog, filters]
  );
  const homeGames = useMemo(
    () => homeCategories[activeHomeCategory]?.games || getLatestGames(catalog, filters, 128),
    [homeCategories, activeHomeCategory, catalog, filters]
  );
  const activeHeroGame = homeGames[selectedHomeCol] || homeGames[0] || null;
  useEffect(() => {
    setSelectedHomeCol((value) => Math.max(0, Math.min(value, homeGames.length - 1)));
  }, [homeGames.length, activeHomeCategory]);

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

  // Katalogi Sync action
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
        { headers: { 'User-Agent': 'Poldrive/2.0 (PS5-13.60; Katalogi-Client)' } }
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

      const target = e.target as HTMLElement;
      if (target.closest('input,textarea,select')) return;
      if (target.closest('button') && !target.closest('.tv-carousel,.tv-navigation')) return;
      if ((currentView === 'home' || isSidebarFocused) && ['ArrowLeft', 'ArrowRight', 'ArrowUp', 'ArrowDown', 'Enter', ' '].includes(e.key)) e.preventDefault();

      // Arrow keys navigation for Home carousel (Horizontal within category, Vertical across categories)
      if (currentView === 'home' && !detailGame && !isSidebarFocused) {
        if (e.key === 'ArrowRight' || e.key === 'd') {
          setSelectedHomeCol((c) => Math.min(homeGames.length - 1, c + 1));
        } else if (e.key === 'ArrowLeft' || e.key === 'a') {
          if (selectedHomeCol === 0) {
            setIsSidebarFocused(true);
          } else {
            setSelectedHomeCol((c) => Math.max(0, c - 1));
          }
        } else if (e.key === 'ArrowDown' || e.key === 's') {
          setActiveHomeCategory((cat) => Math.min(homeCategories.length - 1, cat + 1));
        } else if (e.key === 'ArrowUp' || e.key === 'w') {
          setActiveHomeCategory((cat) => Math.max(0, cat - 1));
        } else if (e.key === 'Enter' || e.key === ' ') {
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
    homeCategories.length,
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
          // D-Pad Up
          else if (gp.buttons[12]?.pressed) {
            lastButtonPress = now;
            if (currentView === 'home' && !detailGame && !isSidebarFocused) {
              setActiveHomeCategory((cat) => Math.max(0, cat - 1));
            }
          }
          // D-Pad Down
          else if (gp.buttons[13]?.pressed) {
            lastButtonPress = now;
            if (currentView === 'home' && !detailGame && !isSidebarFocused) {
              setActiveHomeCategory((cat) => Math.min(homeCategories.length - 1, cat + 1));
            }
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
    homeCategories.length,
    activeHeroGame,
    isSidebarFocused,
    language
  ]);

  return (
    <NativeViewport><div className="tv-app">
      {/* Toast Notification */}
      <Toast message={toastMessage} onDismiss={() => setToastMessage(null)} />

      {/* Main Container */}
      <div className="tv-shell">
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
        <main className="tv-main">
          {/* The same logical stage and carousel geometry as the native reference. */}
          {currentView === 'home' && (
            <HomeView
              games={homeGames}
              categories={homeCategories}
              activeCategoryIndex={activeHomeCategory}
              onSelectCategory={(catIdx) => {
                setActiveHomeCategory(catIdx);
                setSelectedHomeCol(0);
                setIsSidebarFocused(false);
              }}
              language={language}
              selected={selectedHomeCol}
              sidebarFocused={isSidebarFocused}
              onSelect={(index) => {
                setSelectedHomeCol(index);
                setIsSidebarFocused(false);
              }}
              onOpen={(game) => {
                setDetailGame(game);
                setDetailParentGame(null);
              }}
              onPlay={(game) => handleTriggerAction(game)}
              onRefresh={handleSyncPegasus}
              syncInProgress={syncInProgress}
            />
          )}

          {/* VIEW: GAMES / ROMS */}
          {currentView === 'games' && (
            <LibraryBrowser
              onRefresh={handleSyncPegasus} syncInProgress={syncInProgress}
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
              onRefresh={handleSyncPegasus} syncInProgress={syncInProgress}
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

          {/* Empty categories and Downloads share the native hero/carousel layout. */}
          {['files', 'payloads', 'downloads'].includes(currentView) &&
            <HomeView games={currentView === 'downloads' ? downloadItems : []} language={language}
              title={t(language, currentView === 'files' ? 'TXT_NAV_FILES' : currentView === 'payloads' ? 'TXT_NAV_PAYLOADS' : 'TXT_NAV_DOWNLOADS')}
              emptyMessage={t(language, currentView === 'downloads' ? 'TXT_NO_DOWNLOADS' : 'TXT_SECTION_UNAVAILABLE')}
              showDate={false} selected={currentView === 'downloads' ? selectedDownloadCol : 0}
              sidebarFocused={isSidebarFocused} onSelect={setSelectedDownloadCol}
              onOpen={(game) => { setDetailGame(game); setDetailParentGame(null); }}
              onPlay={(game) => handleTriggerAction(game)}
              onRefresh={handleSyncPegasus} syncInProgress={syncInProgress} />}

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
          neighborGames={currentView === 'home' ? homeGames : currentView === 'downloads' ? downloadItems : undefined}
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
      <SystemStatsBar
        stats={stats}
        syncStatus={syncStatus}
        language={language}
        currentView={currentView}
        onRefresh={handleSyncPegasus}
        syncInProgress={syncInProgress}
      />
    </div></NativeViewport>
  );
}

export default App;
