import React, { useRef, useEffect, useState } from 'react';
import {
  Search,
  Home,
  Gamepad2,
  Tv,
  Folder,
  Cpu,
  Download,
  Sliders,
  Globe,
  ChevronDown
} from 'lucide-react';
import { SidebarView, LanguageCode } from '../types';
import { t, LANGUAGES } from '../translations';

interface SidebarProps {
  uiTheme?: 'netflix' | 'ubiquity';
  currentView: SidebarView;
  onSelectView: (view: SidebarView) => void;
  language: LanguageCode;
  onSelectLanguage: (lang: LanguageCode) => void;
  languageMenuOpen: boolean;
  setLanguageMenuOpen: (open: boolean) => void;
  isSidebarFocused: boolean;
}

export const Sidebar: React.FC<SidebarProps> = ({
  uiTheme = 'netflix',
  currentView,
  onSelectView,
  language,
  onSelectLanguage,
  languageMenuOpen,
  setLanguageMenuOpen,
  isSidebarFocused
}) => {
  const menuRef = useRef<HTMLDivElement>(null);
  const [isHovered, setIsHovered] = useState(false);
  // Native SDL2 keeps the rail expanded on Settings as well as on hover/focus.
  const isExpanded = isHovered || isSidebarFocused || languageMenuOpen || currentView === 'settings';

  useEffect(() => {
    function handleClickOutside(event: MouseEvent) {
      if (menuRef.current && !menuRef.current.contains(event.target as Node)) {
        setLanguageMenuOpen(false);
      }
    }
    if (languageMenuOpen) {
      document.addEventListener('mousedown', handleClickOutside);
    }
    return () => {
      document.removeEventListener('mousedown', handleClickOutside);
    };
  }, [languageMenuOpen, setLanguageMenuOpen]);

  const navItems: { view: SidebarView; labelKey: string; icon: React.ReactNode }[] = [
    { view: 'search', labelKey: 'TXT_NAV_SEARCH', icon: <Search className="w-6 h-6" /> },
    { view: 'home', labelKey: 'TXT_NAV_HOME', icon: <Home className="w-6 h-6" /> },
    { view: 'games', labelKey: 'TXT_NAV_GAMES', icon: <Gamepad2 className="w-6 h-6" /> },
    { view: 'emulators', labelKey: 'TXT_NAV_EMULATORS', icon: <Tv className="w-6 h-6" /> },
    { view: 'files', labelKey: 'TXT_NAV_FILES', icon: <Folder className="w-6 h-6" /> },
    { view: 'payloads', labelKey: 'TXT_NAV_PAYLOADS', icon: <Cpu className="w-6 h-6" /> },
    { view: 'downloads', labelKey: 'TXT_NAV_DOWNLOADS', icon: <Download className="w-6 h-6" /> },
    { view: 'settings', labelKey: 'TXT_NAV_SETTINGS', icon: <Sliders className="w-6 h-6" /> }
  ];

  const currentLangObj = LANGUAGES.find((l) => l.code === language) || LANGUAGES[0];
  const activeNavIndex = Math.max(0, navItems.findIndex((item) => item.view === currentView));

  return (
    <>
      <div
        className={`tv-sidebar-dim ${isExpanded && currentView !== 'settings' ? 'tv-sidebar-dim-active' : ''}`}
        aria-hidden="true"
      />
      <aside
        className={`tv-sidebar ${isExpanded ? 'tv-sidebar-expanded' : 'tv-sidebar-collapsed'} ${currentView === 'settings' ? 'tv-sidebar-settings' : ''}`}
        aria-label="Main navigation"
        data-state={isExpanded ? 'expanded' : 'collapsed'}
        onMouseEnter={() => setIsHovered(true)}
        onMouseLeave={() => setIsHovered(false)}
      >
        <div>
          {/* Top Header / Language Selector */}
          <div className="tv-language" ref={menuRef}>
            <button
              onClick={() => setLanguageMenuOpen(!languageMenuOpen)}
              aria-expanded={languageMenuOpen}
              aria-controls="tv-language-options"
              className={`w-full flex items-center justify-between px-3 py-2.5 rounded bg-[#18181c] hover:bg-[#23232a] text-sm font-medium transition-all ${
                languageMenuOpen
                  ? 'ring-2 ring-white text-white'
                  : 'text-[#94949c] hover:text-white'
              }`}
              title="Language / Język [L / Square]"
            >
              <div className="flex items-center gap-2.5">
                <Globe className="w-5 h-5 text-[#e50914] shrink-0" />
                <span className="tv-nav-label font-semibold text-white tracking-wide">
                  {currentLangObj.name}
                </span>
              </div>
              <ChevronDown
                className={`tv-nav-label w-4 h-4 text-[#94949c] transition-transform ${
                  languageMenuOpen ? 'rotate-180 text-white' : ''
                }`}
              />
            </button>

            {/* Language dropdown menu */}
            {languageMenuOpen && (
              <div id="tv-language-options" className="absolute top-16 left-2 w-56 bg-[#16161a] border border-[#2e2e38] rounded-md shadow-2xl py-1.5 z-50 backdrop-blur-md">
                {LANGUAGES.map((item) => (
                  <button
                    key={item.code}
                    onClick={() => {
                      onSelectLanguage(item.code);
                      setLanguageMenuOpen(false);
                    }}
                    className={`w-full text-left px-3.5 py-2 text-xs font-medium flex items-center justify-between transition-colors ${
                      item.code === language
                        ? 'bg-[#e50914] text-white font-bold'
                        : 'text-[#c0c0c8] hover:bg-[#25252e] hover:text-white'
                    }`}
                  >
                    <span>{item.name}</span>
                    {item.code === language && (
                      <span className="text-[10px] tracking-wider uppercase opacity-80">ACTIVE</span>
                    )}
                  </button>
                ))}
              </div>
            )}
          </div>

          {/* Native PS5 wordmark: a red P on the collapsed rail, 'oldrive' revealed when expanded. */}
          <div className="tv-brand-logo" aria-label="PolDrive">
            <span className="tv-brand-mark" aria-hidden="true">P</span>
            <span className="tv-brand-title" aria-hidden="true">olDrive</span>
          </div>

          {/* Navigation list */}
          <nav className="tv-navigation">
            <div className="tv-nav-marker" aria-hidden="true"
              style={{ transform: `translate3d(0, ${activeNavIndex * (uiTheme === 'ubiquity' ? 64 : 92)}px, 0)` }} />
            {navItems.map((item) => {
              const isActive = currentView === item.view;
              return (
                <button
                  key={item.view}
                  onClick={() => onSelectView(item.view)}
                  aria-current={isActive ? 'page' : undefined}
                  title={t(language, item.labelKey)}
                  className={`tv-nav-item ${isActive ? 'tv-nav-active' : ''} ${
                    isActive && isSidebarFocused ? 'tv-nav-focused' : ''
                  }`}
                >
                  <div className="tv-nav-icon">{item.icon}</div>
                  <span className="tv-nav-label">
                    <span className="tv-nav-title">{t(language, item.labelKey)}</span>
                  </span>
                </button>
              );
            })}
          </nav>
        </div>
      </aside>
    </>
  );
};
