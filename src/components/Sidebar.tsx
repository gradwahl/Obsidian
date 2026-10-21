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
import { useLanguagePresentation } from '../useLanguagePresentation';
import { SlidingHighlight } from './SlidingHighlight';
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
  const { displayLanguage, fading } = useLanguagePresentation(language);
  const [menuVisible, setMenuVisible] = useState(languageMenuOpen);
  const closeTimer = useRef<number | undefined>(undefined);
  useEffect(() => {
    if (languageMenuOpen) { setMenuVisible(true); return; }
    window.clearTimeout(closeTimer.current);
    const timer = window.setTimeout(() => setMenuVisible(false), 140);
    return () => window.clearTimeout(timer);
  }, [languageMenuOpen]);
  useEffect(() => () => window.clearTimeout(closeTimer.current), []);
  const menuRef = useRef<HTMLDivElement>(null);
  const [isHovered, setIsHovered] = useState(false);
  // Expand on hover, sidebar focus or language selection only.
  const isExpanded = isHovered || isSidebarFocused || languageMenuOpen;

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

  const currentLangObj = LANGUAGES.find((l) => l.code === displayLanguage) || LANGUAGES[0];


  return (
    <>
      <div
        className={`tv-sidebar-dim ${isExpanded && currentView !== 'settings' ? 'tv-sidebar-dim-active' : ''}`}
        aria-hidden="true"
      />
      <aside
        className={`tv-sidebar glass-surface ${isExpanded ? 'tv-sidebar-expanded' : 'tv-sidebar-collapsed'} ${currentView === 'settings' ? 'tv-sidebar-settings' : ''}`}
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
              className="tv-language-trigger"
              title="Language / Język [L / Square]"
            >
              <div className="flex items-center gap-2.5">
                <Globe className="w-5 h-5 text-[#e50914] shrink-0" />
                <span data-language-fading={fading} className="tv-nav-label language-copy font-semibold text-white tracking-wide">
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
            {menuVisible && (
              <div id="tv-language-options" className={`tv-language-options glass-surface ${languageMenuOpen ? '' : 'is-closing'}`}>
                <SlidingHighlight activeKey={language} />
                {LANGUAGES.map((item) => (
                  <button
                    key={item.code}
                    data-highlight-key={item.code}
                    onClick={(event) => {
                      const list = event.currentTarget.parentElement!;
                      const marker = list.querySelector('.sliding-highlight')!;
                      const old = marker.getBoundingClientRect(), next = event.currentTarget.getBoundingClientRect();
                      const scale = list.getBoundingClientRect().width / list.offsetWidth || 1;
                      const duration = window.matchMedia('(prefers-reduced-motion: reduce)').matches ? 150
                        : Math.min(520, 280 + Math.hypot(next.x-old.x,next.y-old.y)/scale*.25);
                      onSelectLanguage(item.code);
                      window.clearTimeout(closeTimer.current);
                      closeTimer.current = window.setTimeout(() => setLanguageMenuOpen(false), duration + 120);
                    }}
                    className={`tv-language-option ${item.code === language ? 'is-highlighted' : ''}`}
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

          {/* Ubiquity theme emblem: 38x38 crimson squircle 'PD' when collapsed,
              transforming into dark smoked glass with animated crimson ribbons and 'PolDrive' when expanded. */}
          {uiTheme === 'ubiquity' && (
            <div
              className={`ubq-pd-bubble ${isExpanded ? 'ubq-pd-bubble-expanded' : 'ubq-pd-bubble-collapsed'}`}
              aria-label="PolDrive"
            >
              <span className="ubq-pd-ribbons" aria-hidden="true">
                <span className="ubq-pd-ribbon ubq-pd-ribbon-1" />
                <span className="ubq-pd-ribbon ubq-pd-ribbon-2" />
                <span className="ubq-pd-ribbon ubq-pd-ribbon-3" />
              </span>
              <span className="ubq-pd-text" aria-hidden="true">
                <span className="ubq-pd-char-p">P</span>
                <span className="ubq-pd-slide-ol">ol</span>
                <span className="ubq-pd-char-d">D</span>
                <span className="ubq-pd-slide-rive">rive</span>
              </span>
            </div>
          )}

          {/* Navigation list */}
          <nav className="tv-navigation">
            <SlidingHighlight activeKey={currentView} motionEnabled={isExpanded} className="tv-nav-marker" />
            {navItems.map((item) => {
              const isActive = currentView === item.view;
              return (
                <button
                  key={item.view}
                  data-highlight-key={item.view}
                  onClick={() => onSelectView(item.view)}
                  aria-current={isActive ? 'page' : undefined}
                  aria-label={t(language, item.labelKey)}
                  className={`tv-nav-item ${isActive ? 'tv-nav-active' : ''} ${
                    isActive && isSidebarFocused ? 'tv-nav-focused' : ''
                  }`}
                >
                  <div className="tv-nav-icon">{item.icon}</div>
                  <span className="tv-nav-label">
                    <span className="tv-nav-title language-copy" data-language-fading={fading}>{t(displayLanguage, item.labelKey)}</span>
                  </span>
                  <span className="tv-nav-tooltip glass-pill" role="tooltip">{t(displayLanguage, item.labelKey)}</span>
                </button>
              );
            })}
          </nav>
        </div>
      </aside>
    </>
  );
};
