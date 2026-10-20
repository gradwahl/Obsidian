import React, { useState } from 'react';
import { CatalogFilters, GameItem, LanguageCode, LibraryCompany, LibraryConsole } from '../types';
import { COMPANIES } from '../libraryData';
import { t } from '../translations';
import { matchesFilters } from '../catalog';
import { HomeView } from './HomeView';

interface LibraryBrowserProps {
  isEmulatorsView: boolean; catalog: GameItem[]; language: LanguageCode;
  onOpenGame: (game: GameItem) => void; filters: CatalogFilters;
  uiTheme?: 'netflix' | 'ubiquity';
  onRefresh: () => void; syncInProgress: boolean;
}

export const LibraryBrowser: React.FC<LibraryBrowserProps> = ({ isEmulatorsView, catalog, language,
  onOpenGame, filters, uiTheme = 'netflix', onRefresh, syncInProgress }) => {
  const [company, setCompany] = useState<LibraryCompany | null>(null);
  const [console, setConsole] = useState<LibraryConsole | null>(null);
  const [page, setPage] = useState(0);
  const [selected, setSelected] = useState(0);
  const [focused, setFocused] = useState(0);
  const title = t(language, isEmulatorsView ? 'TXT_NAV_EMULATORS' : 'TXT_NAV_GAMES');
  const gamesFor = (system: LibraryConsole) => isEmulatorsView || (!system.title_id_prefix && !system.platform_id) ? []
    : catalog.filter(item => matchesFilters(item, filters) && (
      (system.title_id_prefix && item.id_code.toUpperCase().startsWith(system.title_id_prefix)) ||
      (system.platform_id && item.platform === system.platform_id)
    ));
  const back = () => { if (console) setConsole(null); else setCompany(null); setPage(0); setSelected(0); setFocused(0); };
  const items = company ? company.consoles : COMPANIES;
  const pages = Math.ceil(items.length / 8);
  const breadcrumb = <div className="tv-breadcrumb">
    {company && <button type="button" onClick={back}>← {t(language, 'TXT_LIBRARY_BACK')}</button>}
    <span>{title}{company && ` / ${company.name}`}{console && ` / ${console.name}`}</span>
  </div>;

  if (console) return <div className={uiTheme === 'ubiquity' ? 'tv-library-games tv-library-ubiquity-games' : 'tv-library-games'}>
    <HomeView games={gamesFor(console)} uiTheme={uiTheme} language={language} title={console.name} showDate={false}
      emptyMessage={t(language, isEmulatorsView ? 'TXT_EMULATOR_NOT_CONNECTED'
        : (console.title_id_prefix || console.platform_id) ? 'TXT_NO_PACKAGES' : 'TXT_CONSOLE_NOT_CONNECTED')}
      selected={selected} sidebarFocused={false} onSelect={setSelected} onOpen={onOpenGame}
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
        return <button type="button" key={item.name}
          className={`tv-library-tile ${focused === index ? 'tv-library-selected' : ''}`}
          onFocus={() => setFocused(index)} onMouseEnter={() => setFocused(index)}
          onClick={() => { if (system) setConsole(system); else setCompany(brand); setPage(0); setFocused(0); }}>
          <div className={`tv-library-image ${brand ? 'tv-library-brand' : ''}`}>
            {item.artwork_path ? <img src={item.artwork_path} alt={item.name} /> : <span>{item.name}</span>}
          </div>
          {system?.logo_path && <img className="tv-library-logo" src={system.logo_path} alt="" />}
          <strong>{item.name}</strong>
          <span>{brand ? t(language, 'TXT_CONSOLE_COUNT', brand.consoles.length)
            : t(language, isEmulatorsView ? 'TXT_LIBRARY_EMULATOR_COUNT' : 'TXT_LIBRARY_GAME_COUNT', gamesFor(system!).length)}</span>
        </button>;
      })}
    </div>
    <div className="tv-library-pages">
      <span>{page * 8 + focused + 1} / {items.length}</span>
      <button type="button" disabled={page === 0} onClick={() => { setPage(value => value - 1); setFocused(0); }}>←</button>
      <button type="button" disabled={page >= pages - 1} onClick={() => { setPage(value => value + 1); setFocused(0); }}>→</button>
    </div>
    <p className="tv-library-hint">{t(language, 'TXT_LIBRARY_NAV_HINT')}</p>
  </section>;
};
