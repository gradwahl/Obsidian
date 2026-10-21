import React, { useState, useEffect } from 'react';
import { CatalogFilters, GameItem, LanguageCode, LibraryCompany, LibraryConsole } from '../types';
import { COMPANIES, consolesForSection } from '../libraryData';
import { t } from '../translations';
import { matchesFilters } from '../catalog';
import { HomeView } from './HomeView';
import { EmulatorReleaseView, getEmulatorRelease, EMULATOR_RELEASES_URL } from './EmulatorReleaseView';
interface LibraryBrowserProps {
  sidebarFocused?: boolean; isEmulatorsView: boolean; catalog: GameItem[]; language: LanguageCode;
  onOpenGame: (game: GameItem) => void; filters: CatalogFilters;
  onRefresh: () => void; syncInProgress: boolean;
}

export const LibraryBrowser: React.FC<LibraryBrowserProps> = ({ sidebarFocused = false, isEmulatorsView, catalog, language,
  onOpenGame, filters, onRefresh, syncInProgress }) => {
  const [company, setCompany] = useState<LibraryCompany | null>(null);
  const [console, setConsole] = useState<LibraryConsole | null>(null);
  const [page, setPage] = useState(0);
  const [selected, setSelected] = useState(0);
  const [focused, setFocused] = useState(0);
  const title = t(language, isEmulatorsView ? 'TXT_NAV_EMULATORS' : 'TXT_NAV_GAMES');
  const matchesSystem = (item: GameItem, system: LibraryConsole) => !isEmulatorsView &&
    matchesFilters(item, filters) && Boolean(
      (system.title_id_prefix && item.id_code.toUpperCase().startsWith(system.title_id_prefix)) ||
      (system.platform_id && item.platform === system.platform_id));
  const gamesFor = (system: LibraryConsole) => catalog.filter(item => matchesSystem(item, system));
  const countFor = (system: LibraryConsole) => {
    let count = 0;
    for (const item of catalog) if (matchesSystem(item, system)) count++;
    return count;
  };
  const back = () => { if (console) setConsole(null); else setCompany(null); setPage(0); setSelected(0); setFocused(0); };
  useEffect(() => {
    if (sidebarFocused || !company) return;
    const handle = (event: KeyboardEvent) => {
      if (document.querySelector('[role="dialog"]')) return;
      if (event.key === 'Escape' || event.key === 'Backspace') {
        event.preventDefault(); event.stopImmediatePropagation(); back();
      }
    };
    window.addEventListener('keydown', handle, true);
    return () => window.removeEventListener('keydown', handle, true);
  }, [sidebarFocused, company, console]);
  const items = company ? consolesForSection(company, isEmulatorsView) : COMPANIES;
  const pages = Math.ceil(items.length / 8);
  const breadcrumb = <div className="tv-breadcrumb">
    {company && <button type="button" onClick={back}>← {t(language, 'TXT_LIBRARY_BACK')}</button>}
    <span>{title}{company && ` / ${company.name}`}{console && ` / ${console.name}`}</span>
  </div>;

  if (console && isEmulatorsView) return <EmulatorReleaseView system={console} onBack={back} />;

  if (console) return <div className="tv-library-games tv-library-ubiquity-games">
    <HomeView games={gamesFor(console)} language={language} title={console.name} showDate={false}
      emptyMessage={t(language, isEmulatorsView ? 'TXT_EMULATOR_NOT_CONNECTED'
        : (console.title_id_prefix || console.platform_id) ? 'TXT_NO_PACKAGES' : 'TXT_CONSOLE_NOT_CONNECTED')}
      selected={selected} sidebarFocused={sidebarFocused} onSelect={setSelected} onOpen={onOpenGame}
      onRefresh={onRefresh} syncInProgress={syncInProgress} />
    {breadcrumb}
  </div>;

  return <section className="tv-library tv-page">
    {breadcrumb}
    <h1>{company?.name || title}</h1>
    <p>{t(language, company ? 'TXT_CHOOSE_CONSOLE' : 'TXT_CHOOSE_COMPANY')}</p>
    <div className="tv-library-grid">
      {items.slice(page * 8, page * 8 + 8).map((item, index) => {
        const system = company ? item as LibraryConsole : null;
        const brand = company ? null : item as LibraryCompany;
        return <button type="button" key={`${item.name}_${index}`}
          className={`tv-library-tile ${focused === index ? 'tv-library-selected' : ''}`}
          onFocus={() => setFocused(index)} onMouseEnter={() => setFocused(index)}
          onClick={() => { if (system) setConsole(system); else setCompany(brand); setPage(0); setFocused(0); }}>
          <strong>{item.name}</strong>
          <span>{brand ? t(language, 'TXT_CONSOLE_COUNT', consolesForSection(brand, isEmulatorsView).length)
            : isEmulatorsView ? (getEmulatorRelease(system!) ? 'Release available' : 'Browse releases')
            : t(language, 'TXT_LIBRARY_GAME_COUNT', countFor(system!))}</span>
        </button>;
      })}
    </div>
    <div className="tv-library-pages">
      <span>{page * 8 + focused + 1} / {items.length}</span>
      <button type="button" disabled={page === 0} onClick={() => { setPage(value => value - 1); setFocused(0); }}>←</button>
      <button type="button" disabled={page >= pages - 1} onClick={() => { setPage(value => value + 1); setFocused(0); }}>→</button>
    </div>
    {isEmulatorsView && <a className="tv-emulator-all tv-emulator-library-link"
      href={EMULATOR_RELEASES_URL} target="_blank" rel="noopener noreferrer">
      Browse Obsidian emulator releases ↗</a>}
    <p className="tv-library-hint">{t(language, 'TXT_LIBRARY_NAV_HINT')}</p>
  </section>;
};
