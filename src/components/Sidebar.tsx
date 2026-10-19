import React, { useRef, useEffect } from 'react';
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
  currentView: SidebarView;
  onSelectView: (view: SidebarView) => void;
  language: LanguageCode;
  onSelectLanguage: (lang: LanguageCode) => void;
  languageMenuOpen: boolean;
  setLanguageMenuOpen: (open: boolean) => void;
  isSidebarFocused: boolean;
}

export const Sidebar: React.FC<SidebarProps> = ({
  currentView,
  onSelectView,
  language,
  onSelectLanguage,
  languageMenuOpen,
  setLanguageMenuOpen,
  isSidebarFocused
}) => {
  const menuRef = useRef<HTMLDivElement>(null);

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

  return (
    <aside className="w-[272px] shrink-0 h-full bg-[#101012] border-r border-[#1e1e24] flex flex-col justify-between select-none z-30">
      <div>
        {/* Top Header / Language Selector */}
        <div className="relative p-4 border-b border-[#1f1f26]" ref={menuRef}>
          <button
            onClick={() => setLanguageMenuOpen(!languageMenuOpen)}
            className={`w-full flex items-center justify-between px-3 py-2.5 rounded bg-[#18181c] hover:bg-[#23232a] text-sm font-medium transition-all ${
              languageMenuOpen
                ? 'ring-2 ring-white text-white'
                : 'text-[#94949c] hover:text-white'
            }`}
            title="Language / Język [L / Square]"
          >
            <div className="flex items-center gap-2.5">
              <Globe className="w-4 h-4 text-[#e50914]" />
              <span className="font-semibold text-white tracking-wide">
                {currentLangObj.name}
              </span>
            </div>
            <ChevronDown
              className={`w-4 h-4 text-[#94949c] transition-transform ${
                languageMenuOpen ? 'rotate-180 text-white' : ''
              }`}
            />
          </button>

          {/* Language dropdown menu */}
          {languageMenuOpen && (
            <div className="absolute top-16 left-3 right-3 bg-[#16161a] border border-[#2e2e38] rounded-md shadow-2xl py-1.5 z-50 backdrop-blur-md">
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
                  {item.code === language && <span className="text-[10px] tracking-wider uppercase opacity-80">ACTIVE</span>}
                </button>
              ))}
            </div>
          )}
        </div>

        {/* Navigation list */}
        <nav className="p-3 space-y-1.5">
          {navItems.map((item) => {
            const isActive = currentView === item.view;
            return (
              <button
                key={item.view}
                onClick={() => onSelectView(item.view)}
                className={`w-full flex items-center gap-3.5 px-3.5 py-3 rounded text-sm font-semibold tracking-wide transition-all ${
                  isActive
                    ? isSidebarFocused
                      ? 'bg-[#22222a] text-white border-2 border-white shadow-lg'
                      : 'bg-[#1c1c22] text-white border-l-4 border-l-[#e50914]'
                    : 'text-[#94949c] hover:text-white hover:bg-[#18181f]'
                }`}
              >
                <div className={`${isActive ? 'text-[#e50914]' : 'text-[#858590]'}`}>
                  {item.icon}
                </div>
                <span>{t(language, item.labelKey)}</span>
              </button>
            );
          })}
        </nav>
      </div>

      {/* Brand & PS5 Homebrew footer */}
      <div className="p-4 border-t border-[#1a1a20] bg-[#0c0c0e]">
        <div className="flex items-center gap-2">
          <div className="w-2.5 h-2.5 rounded-full bg-[#e50914] animate-pulse"></div>
          <span className="font-extrabold tracking-wider text-sm text-white">WOLODRIVE</span>
        </div>
        <div className="text-[11px] text-[#71717a] font-mono mt-1">
          v2.0.0-PRO (PS5 13.60)
        </div>
      </div>
    </aside>
  );
};
