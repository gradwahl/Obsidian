import React, { useState, useEffect, useMemo, useCallback, useRef } from 'react';
import { SidebarView, LanguageCode, GameItem, CatalogFilters, SystemStats } from './types';
import { Sidebar } from './components/Sidebar';
import { HomeView, HomeCategoryRow } from './components/HomeView';
import { NativeViewport } from './components/NativeViewport';
import { UbiquityGameDetails } from './components/UbiquityGameDetails';
import { SearchGrid } from './components/SearchGrid';
import { LibraryBrowser } from './components/LibraryBrowser';
import { FileBrowser } from './components/FileBrowser';
import { SettingsView } from './components/SettingsView';
import { DownloadsView } from './components/DownloadsView';
import { SystemStatsBar } from './components/SystemStatsBar';
import { Toast } from './components/Toast';
import { INITIAL_CATALOG, getLatestGames, getHomeCategoryLists, getDistinctSources, matchesFilters } from './catalog';
import { loadSourceCatalogues } from './sourceCatalogues';
import { t } from './translations';

export function focusSettingsControl(direction: number) {
  const controls = Array.from(document.querySelectorAll<HTMLElement>(
    '.tv-settings button, .tv-settings input, .tv-settings [tabindex="0"]'));
  if (!controls.length) return;
  const index = controls.indexOf(document.activeElement as HTMLElement);
  controls[index < 0 ? 0 : Math.max(0, Math.min(controls.length - 1, index + direction))].focus();
}

function App() {
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
  const lastGamepadPress = useRef(0);

  // 3. Catalog state
  const [catalog, setCatalog] = useState<GameItem[]>(INITIAL_CATALOG);
  const [syncInProgress, setSyncInProgress] = useState(false);
  const catalogLoader = useRef<AbortController | null>(null);
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
  // Each Netflix row remembers its own focused cover, like the native SDL view.
  const [selectedHomeCols, setSelectedHomeCols] = useState<number[]>([0, 0, 0, 0, 0]);
  const selectedHomeCol = selectedHomeCols[activeHomeCategory] ?? 0;
  const setSelectedHomeCol = useCallback<React.Dispatch<React.SetStateAction<number>>>((value) => {
    setSelectedHomeCols((previous) => {
      const current = previous[activeHomeCategory] ?? 0;
      const next = typeof value === 'function' ? value(current) : value;
      if (next === current) return previous;
      const updated = [...previous];
      updated[activeHomeCategory] = next;
      return updated;
    });
  }, [activeHomeCategory]);

  // 6. Modal details state
  const [detailGame, setDetailGame] = useState<GameItem | null>(null);
  const [detailParentGame, setDetailParentGame] = useState<GameItem | null>(null);
  const [detailActionIndex, setDetailActionIndex] = useState(0);

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

  // Initial welcome notification matching C code: Started Obsidian (2.0.0-PRO (PS5 13.60))
  useEffect(() => {
    showToast(window.location.hash === '#native-unavailable'
      ? t(language, 'TXT_NATIVE_FALLBACK')
      : t(language, 'TXT_STARTED', 'Obsidian', '2.0.0-PRO (PS5 13.60)'));
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
  const homeCategories = useMemo<HomeCategoryRow[]>(() => {
    // The former implementation re-normalized/deduplicated up to 80,000
    // entries five times, every catalogue update. Share one deduped pass.
    const groups = getHomeCategoryLists(catalog, filters, 128);
    return [
      { id: 'ps5', titleKey: 'TXT_CAT_PS5', games: groups.ps5 },
      { id: 'ps4', titleKey: 'TXT_CAT_PS4', games: groups.ps4 },
      { id: 'popular', titleKey: 'TXT_CAT_POPULAR', games: groups.popular },
      { id: 'newest', titleKey: 'TXT_CAT_NEWEST', games: groups.newest },
      { id: 'top_rated', titleKey: 'TXT_CAT_TOP_RATED', games: groups.top_rated }
    ];
  }, [catalog, filters]);
  const homeGames = useMemo(
    () => homeCategories[activeHomeCategory]?.games || getLatestGames(catalog, filters, 128),
    [homeCategories, activeHomeCategory, catalog, filters]
  );
  const activeHeroGame = homeGames[selectedHomeCol] || homeGames[0] || null;
  useEffect(() => {
    setSelectedHomeCol((value) => Math.max(0, Math.min(value, homeGames.length - 1)));
  }, [homeGames.length, activeHomeCategory, setSelectedHomeCol]);

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

  // The verified seed catalogue is immediately interactive. Download and
  // convert the big offline source indexes in small background JSON pages;
  // never parse giant Vite JS modules on the browser's rendering thread.
  const startCatalogLoad = async (notify: boolean) => {
    catalogLoader.current?.abort();
    const controller = new AbortController();
    catalogLoader.current = controller;
    let loaded: GameItem[] = [];
    const completedSources: string[] = [];
    let failedSources = 0;
    let pendingCommit: number | null = null;
    const commit = () => {
      pendingCommit = null;
      if (!controller.signal.aborted) setCatalog([...INITIAL_CATALOG, ...loaded]);
    };
    const scheduleCommit = () => {
      if (pendingCommit === null) {
        // One expensive 80k-item Home ranking update per second at most.
        // Otherwise the loader would cause dozens of React re-renders.
        pendingCommit = window.setTimeout(commit, 900);
      }
    };
    setSyncInProgress(true);
    setSyncStatus('Loading source catalogues in the background...');
    try {
      // Allow the initial hero, navigation and CSS spinner to paint.
      await new Promise<void>(resolve => window.setTimeout(resolve, 120));
      if (controller.signal.aborted) return;
      const result = await loadSourceCatalogues(
        status => {
          if (controller.signal.aborted) return;
          if (status.entries > 0) completedSources.push(status.name);
          if (status.status === 'error') failedSources++;
          setSyncStatus('Loaded ' + loaded.length.toLocaleString() +
            ' entries from ' + completedSources.length + ' sources' +
            (failedSources ? ' (' + failedSources + ' unavailable)' : '') +
            ' • loading in background');
        },
        batch => {
          if (controller.signal.aborted) return;
          loaded = batch;
          // Batch notifications do not each force a large React re-render.
          scheduleCommit();
        },
        controller.signal
      );
      if (controller.signal.aborted) return;
      if (pendingCommit !== null) {
        window.clearTimeout(pendingCommit);
        pendingCommit = null;
      }
      setCatalog([...INITIAL_CATALOG, ...result.entries]);
      const successes = result.statuses.filter(status => status.entries > 0);
      const errors = result.statuses.filter(status => status.status === 'error');
      const message = 'Loaded ' + result.entries.length.toLocaleString() +
        ' additional entries from ' + successes.length + ' sources' +
        (errors.length ? ' (' + errors.length + ' unavailable)' : '');
      setSyncStatus(message);
      if (notify) showToast(message);
    } catch (error) {
      if (!controller.signal.aborted) {
        const message = 'Catalogue loading failed: ' + String(error);
        setSyncStatus(message);
        if (notify) showToast(message);
      }
    } finally {
      if (pendingCommit !== null) window.clearTimeout(pendingCommit);
      if (catalogLoader.current === controller) {
        catalogLoader.current = null;
        setSyncInProgress(false);
      }
    }
  };

  useEffect(() => {
    const timer = window.setTimeout(() => { void startCatalogLoad(false); }, 500);
    return () => {
      window.clearTimeout(timer);
      catalogLoader.current?.abort();
    };
  }, []);

  const handleSyncPegasus = async () => {
    if (syncInProgress) {
      showToast(t(language, 'TXT_SYNC_BUSY'));
      return;
    }
    await startCatalogLoad(true);
  };

  // Keyboard navigation & Gamepad support
  useEffect(() => {
    const handleKeyDown = (e: KeyboardEvent) => {
      if (currentView === 'files' && languageMenuOpen) {
        const choices = Array.from(document.querySelectorAll<HTMLButtonElement>('#tv-language-options button'));
        const index = choices.indexOf(document.activeElement as HTMLButtonElement);
        if (e.key === 'Escape' || e.key === 'Backspace') { e.preventDefault(); setLanguageMenuOpen(false); return; }
        if (e.key === 'ArrowUp' || e.key === 'ArrowDown') {
          e.preventDefault(); choices[Math.max(0, Math.min(choices.length - 1, index + (e.key === 'ArrowUp' ? -1 : 1)))]?.focus(); return;
        }
        if (e.key === 'Enter' || e.key === ' ') { e.preventDefault(); choices[Math.max(0, index)]?.click(); return; }
      }
      if (currentView === 'files' && !isSidebarFocused && !['l', 'L'].includes(e.key)) return;
      // Escape or Backspace: close modal or focus sidebar
      if (e.key === 'Escape') {
        if (detailGame) {
          const subpanelClose = document.querySelector<HTMLButtonElement>(
            '.ubq-detail-lightbox button, .ubq-detail-options-header button'
          );
          if (subpanelClose) { subpanelClose.click(); return; }
        }
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

      if (detailGame) {
        const options = Array.from(document.querySelectorAll<HTMLButtonElement>(
          '.ubq-detail-option-list button'));
        if (options.length) {
          const idx = options.indexOf(document.activeElement as HTMLButtonElement);
          if (e.key === 'ArrowDown' || e.key === 'ArrowUp') {
            e.preventDefault();
            options[Math.max(0, Math.min(options.length - 1,
              idx + (e.key === 'ArrowDown' ? 1 : -1)))].focus();
            return;
          }
          if ((e.key === 'Enter' || e.key === ' ') && !(e.target as HTMLElement).closest('button')) {
            e.preventDefault();
            options[Math.max(0, idx)].click();
            return;
          }
          return;
        }
        if (e.key === 'ArrowLeft' || e.key === 'a') {
          e.preventDefault(); setDetailActionIndex(index => Math.max(0, index - 1)); return;
        }
        if (e.key === 'ArrowRight' || e.key === 'd') {
          e.preventDefault(); setDetailActionIndex(index => Math.min(2, index + 1)); return;
        }
        if ((e.key === 'Enter' || e.key === ' ') &&
            !(e.target as HTMLElement).closest('button,input,textarea')) {
          e.preventDefault();
          document.querySelectorAll<HTMLButtonElement>('.ubq-detail-actions button')[detailActionIndex]?.click();
          return;
        }
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
      if (!isSidebarFocused && target.closest('button') && !target.closest('.tv-carousel,.tv-navigation')) return;
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
      } else if (currentView === 'settings' && !detailGame && !isSidebarFocused) {
        if (e.key === 'ArrowUp') { e.preventDefault(); focusSettingsControl(-1); }
        else if (e.key === 'ArrowDown') { e.preventDefault(); focusSettingsControl(1); }
        else if (e.key === 'Enter') {
          if (!document.activeElement?.closest('.tv-settings')) focusSettingsControl(0);
        } else if (e.key === 'ArrowLeft') setIsSidebarFocused(true);
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
          const next = views[Math.min(views.length - 1, curIdx + 1)];
          setCurrentView(next);
        } else if (e.key === 'ArrowUp' || e.key === 'w') {
          const prev = views[Math.max(0, curIdx - 1)];
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
    activeHeroGame,
    detailActionIndex,
    languageMenuOpen
  ]);

  // Gamepad polling loop
  useEffect(() => {
    let animId: number;
    // Persist the input cooldown across view/state changes while a button is held.

    const pollGamepad = () => {
      const gamepads = navigator.getGamepads ? navigator.getGamepads() : [];
      const gp = gamepads[0];
      if (gp) {
        const now = Date.now();
        if (now - lastGamepadPress.current > 165) {
          if (currentView === 'files' && !isSidebarFocused) {
            const keys: Record<number, string> = { 0: 'Enter', 1: 'Escape', 2: 'l', 3: 'r',
              12: 'ArrowUp', 13: 'ArrowDown', 14: 'ArrowLeft', 15: 'ArrowRight' };
            const button = Object.keys(keys).map(Number).find(index => gp.buttons[index]?.pressed);
            if (button !== undefined) {
              lastGamepadPress.current = now;
              window.dispatchEvent(new KeyboardEvent('keydown', { key: keys[button], bubbles: true }));
            }
            animId = requestAnimationFrame(pollGamepad);
            return;
          }
          // Cross (Button 0): select / open
          if (gp.buttons[0]?.pressed) {
            lastGamepadPress.current = now;
            if (isSidebarFocused && !detailGame) {
              setIsSidebarFocused(false);
            } else if (currentView === 'settings' && !detailGame) {
              (document.activeElement as HTMLElement)?.closest('.tv-settings')
                ? (document.activeElement as HTMLElement).click() : focusSettingsControl(0);
            } else if (detailGame) {
              const screenshotClose = document.querySelector<HTMLButtonElement>('.ubq-detail-lightbox button');
              const options = Array.from(document.querySelectorAll<HTMLButtonElement>(
                '.ubq-detail-option-list button'));
              if (screenshotClose) screenshotClose.click();
              else if (options.length) {
                options[Math.max(0, options.indexOf(document.activeElement as HTMLButtonElement))].click();
              } else {
                document.querySelectorAll<HTMLButtonElement>('.ubq-detail-actions button')[detailActionIndex]?.click();
              }
            } else if (currentView === 'home' && activeHeroGame) {
              setDetailGame(activeHeroGame);
            }
          }
          // Circle (Button 1): back
          else if (gp.buttons[1]?.pressed) {
            lastGamepadPress.current = now;
            if (detailGame) {
              const subpanelClose = document.querySelector<HTMLButtonElement>(
                '.ubq-detail-lightbox button, .ubq-detail-options-header button');
              if (subpanelClose) subpanelClose.click();
              else if (detailParentGame) {
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
            lastGamepadPress.current = now;
            setLanguageMenuOpen((prev) => !prev);
          }
          // Triangle (Button 3): refresh
          else if (gp.buttons[3]?.pressed) {
            lastGamepadPress.current = now;
            handleSyncPegasus();
          }
          // D-Pad Up
          else if (gp.buttons[12]?.pressed) {
            lastGamepadPress.current = now;
            if (detailGame) {
              const options = Array.from(document.querySelectorAll<HTMLButtonElement>(
                '.ubq-detail-option-list button'));
              if (options.length) {
                const idx = options.indexOf(document.activeElement as HTMLButtonElement);
                options[Math.max(0, idx - 1)].focus();
              }
            } else if (isSidebarFocused && !detailGame) {
              const views: SidebarView[] = ['search', 'home', 'games', 'emulators', 'files', 'payloads', 'downloads', 'settings'];
              setCurrentView((view) => views[Math.max(0, views.indexOf(view) - 1)]);
            } else if (currentView === 'settings' && !detailGame) {
              focusSettingsControl(-1);
            } else if (currentView === 'home' && !detailGame) {
              setActiveHomeCategory((cat) => Math.max(0, cat - 1));
            }
          }
          // D-Pad Down
          else if (gp.buttons[13]?.pressed) {
            lastGamepadPress.current = now;
            if (detailGame) {
              const options = Array.from(document.querySelectorAll<HTMLButtonElement>(
                '.ubq-detail-option-list button'));
              if (options.length) {
                const idx = options.indexOf(document.activeElement as HTMLButtonElement);
                options[Math.min(options.length - 1, idx + 1)].focus();
              }
            } else if (isSidebarFocused && !detailGame) {
              const views: SidebarView[] = ['search', 'home', 'games', 'emulators', 'files', 'payloads', 'downloads', 'settings'];
              setCurrentView((view) => views[Math.min(views.length - 1, views.indexOf(view) + 1)]);
            } else if (currentView === 'settings' && !detailGame) {
              focusSettingsControl(1);
            } else if (currentView === 'home' && !detailGame) {
              setActiveHomeCategory((cat) => Math.min(homeCategories.length - 1, cat + 1));
            }
          }
          // D-Pad Left
          else if (gp.buttons[14]?.pressed) {
            lastGamepadPress.current = now;
            if (detailGame) {
              if (!document.querySelector('.ubq-detail-options')) {
                setDetailActionIndex(index => Math.max(0, index - 1));
              }
            } else if (currentView === 'settings' && !detailGame) {
              setIsSidebarFocused(true);
            } else if (currentView === 'home' && !detailGame) {
              if (selectedHomeCol === 0) setIsSidebarFocused(true);
              else setSelectedHomeCol((c) => Math.max(0, c - 1));
            }
          }
          // D-Pad Right
          else if (gp.buttons[15]?.pressed) {
            lastGamepadPress.current = now;
            if (detailGame) {
              if (!document.querySelector('.ubq-detail-options')) {
                setDetailActionIndex(index => Math.min(2, index + 1));
              }
            }
            else if (isSidebarFocused && !detailGame) setIsSidebarFocused(false);
            else if (currentView === 'home' && !detailGame) {
              setSelectedHomeCol((c) => Math.min(homeGames.length - 1, c + 1));
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
    language,
    detailActionIndex
  ]);

  return (
    <NativeViewport><div className="tv-app tv-theme-ubiquity">
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
              onSelectCategory={(catIdx, colIdx) => {
                setActiveHomeCategory(catIdx);
                if (colIdx !== undefined) {
                  setSelectedHomeCols((previous) => {
                    const updated = [...previous];
                    updated[catIdx] = colIdx;
                    return updated;
                  });
                }
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

          {/* The File Manager remains mounted to preserve console folder state. */}
          <div hidden={currentView !== 'files'}><FileBrowser language={language}
            sidebarFocused={isSidebarFocused || languageMenuOpen || currentView !== 'files'}
            onSidebar={() => setIsSidebarFocused(true)} /></div>
          {/* Backup-version-1 did not include a Payloads manager; keep its original placeholder. */}
          {currentView === 'payloads' &&
            <HomeView games={[]} language={language}
              title={t(language, 'TXT_NAV_PAYLOADS')}
              emptyMessage={t(language, 'TXT_SECTION_UNAVAILABLE')}
              showDate={false} selected={0}
              sidebarFocused={isSidebarFocused} onSelect={() => {}}
              onOpen={(game) => { setDetailGame(game); setDetailParentGame(null); }}
              onPlay={(game) => handleTriggerAction(game)}
              onRefresh={handleSyncPegasus} syncInProgress={syncInProgress} />}

          {/* VIEW: DOWNLOADS */}
          {currentView === 'downloads' && (
            <DownloadsView
              catalog={catalog}
              language={language}

              onOpenGame={(g) => {
                setDetailGame(g);
                setDetailParentGame(null);
              }}
            />
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



              catalogCount={catalog.length}
            />
          )}
        </main>
      </div>

      {/* Expanded Game Details Modal */}
      {detailGame && (
        <UbiquityGameDetails
          game={detailGame}
          catalog={catalog}
          language={language}
          actionIndex={detailActionIndex}
          onActionIndexChange={setDetailActionIndex}
          onClose={() => {
            setDetailGame(null);
            setDetailParentGame(null);
            setDetailActionIndex(0);
          }}
          onTriggerAction={handleTriggerAction}
          onSelectGame={(selected) => {
            setDetailParentGame(detailGame);
            setDetailGame(selected);
            setDetailActionIndex(0);
          }}
          parentGame={detailParentGame}
          onBackToParent={() => {
            if (detailParentGame) {
              setDetailGame(detailParentGame);
              setDetailParentGame(null);
              setDetailActionIndex(0);
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
