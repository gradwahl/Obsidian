import React, { useState } from 'react';
import { GameItem, LanguageCode, LibraryCompany, LibraryConsole } from '../types';
import { COMPANIES } from '../libraryData';
import { t } from '../translations';
import { GameCard } from './GameCard';
import { ArrowLeft } from 'lucide-react';
import { matchesFilters } from '../catalog';

interface LibraryBrowserProps {
  isEmulatorsView: boolean;
  catalog: GameItem[];
  language: LanguageCode;
  onOpenGame: (game: GameItem) => void;
  filters: any;
}

export const LibraryBrowser: React.FC<LibraryBrowserProps> = ({
  isEmulatorsView,
  catalog,
  language,
  onOpenGame,
  filters
}) => {
  const [selectedCompany, setSelectedCompany] = useState<LibraryCompany | null>(null);
  const [selectedConsole, setSelectedConsole] = useState<LibraryConsole | null>(null);
  const [selectedGameIdx, setSelectedGameIdx] = useState(0);

  // Back button handler
  const handleBack = () => {
    if (selectedConsole) {
      setSelectedConsole(null);
    } else if (selectedCompany) {
      setSelectedCompany(null);
    }
  };

  // Section title
  const sectionTitle = isEmulatorsView
    ? t(language, 'TXT_NAV_EMULATORS')
    : t(language, 'TXT_NAV_GAMES');

  // If inside a console:
  if (selectedConsole && selectedCompany) {
    const isPS4orPS5 =
      selectedConsole.title_id_prefix === 'CUSA' ||
      selectedConsole.title_id_prefix === 'PPSA';

    const matchingGames = catalog.filter((item) => {
      if (!matchesFilters(item, filters)) return false;
      if (!selectedConsole.title_id_prefix) return false;
      return item.id_code
        .toUpperCase()
        .startsWith(selectedConsole.title_id_prefix.toUpperCase());
    });

    return (
      <div className="flex-1 h-full overflow-y-auto p-8 select-none">
        {/* Navigation Breadcrumb & Back */}
        <div className="flex items-center gap-4 mb-6">
          <button
            onClick={handleBack}
            className="flex items-center gap-1.5 px-3 py-1.5 rounded bg-[#1e1e26] hover:bg-white hover:text-black text-xs font-bold text-white transition-all border border-[#2e2e3a]"
          >
            <ArrowLeft className="w-3.5 h-3.5" />
            <span>{t(language, 'TXT_LIBRARY_BACK')}</span>
          </button>
          <div className="text-xs font-semibold text-[#8a8a94]">
            {sectionTitle} &gt; {selectedCompany.name} &gt;{' '}
            <span className="text-white font-bold">{selectedConsole.name}</span>
          </div>
        </div>

        {/* Emulators view always reports authentic empty state */}
        {isEmulatorsView ? (
          <div className="py-20 text-center max-w-lg mx-auto bg-[#141418] border border-[#242430] rounded-xl p-8">
            <div className="w-16 h-16 mx-auto mb-4 rounded-full bg-[#1e1e26] flex items-center justify-center text-white/50 text-2xl font-bold">
              0
            </div>
            <div className="text-sm font-medium text-[#c0c0cc] leading-relaxed">
              {t(language, 'TXT_EMULATOR_NOT_CONNECTED')}
            </div>
          </div>
        ) : isPS4orPS5 ? (
          <div>
            <div className="flex items-center justify-between mb-4">
              <h3 className="text-lg font-bold text-white">
                {selectedConsole.name} ({matchingGames.length})
              </h3>
              <span className="text-xs font-mono text-[#8a8a94]">
                {t(language, 'TXT_LIBRARY_GAME_COUNT', matchingGames.length)}
              </span>
            </div>

            {matchingGames.length === 0 ? (
              <div className="py-16 text-center text-[#8a8a94] text-sm bg-[#121216] rounded-xl border border-[#202028]">
                {t(language, 'TXT_NO_PACKAGES')}
              </div>
            ) : (
              <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 xl:grid-cols-4 gap-4">
                {matchingGames.map((game, idx) => (
                  <GameCard
                    key={game.id_code + '_' + idx}
                    game={game}
                    isSelected={selectedGameIdx === idx}
                    onSelect={() => setSelectedGameIdx(idx)}
                    onOpen={() => onOpenGame(game)}
                    language={language}
                  />
                ))}
              </div>
            )}
          </div>
        ) : (
          <div className="py-20 text-center max-w-lg mx-auto bg-[#141418] border border-[#242430] rounded-xl p-8">
            <div className="text-sm font-medium text-[#c0c0cc] leading-relaxed">
              {t(language, 'TXT_CONSOLE_NOT_CONNECTED')}
            </div>
          </div>
        )}
      </div>
    );
  }

  // If inside a Company: show Consoles grid
  if (selectedCompany) {
    return (
      <div className="flex-1 h-full overflow-y-auto p-8 select-none">
        {/* Navigation Breadcrumb & Back */}
        <div className="flex items-center gap-4 mb-6">
          <button
            onClick={handleBack}
            className="flex items-center gap-1.5 px-3 py-1.5 rounded bg-[#1e1e26] hover:bg-white hover:text-black text-xs font-bold text-white transition-all border border-[#2e2e3a]"
          >
            <ArrowLeft className="w-3.5 h-3.5" />
            <span>{t(language, 'TXT_LIBRARY_BACK')}</span>
          </button>
          <div className="text-xs font-semibold text-[#8a8a94]">
            {sectionTitle} &gt;{' '}
            <span className="text-white font-bold">{selectedCompany.name}</span>
          </div>
        </div>

        <h3 className="text-xl font-black text-white tracking-tight mb-5">
          {t(language, 'TXT_CHOOSE_CONSOLE')} ({selectedCompany.consoles.length})
        </h3>

        {/* Consoles Grid */}
        <div className="grid grid-cols-2 sm:grid-cols-3 lg:grid-cols-4 gap-5 max-w-6xl">
          {selectedCompany.consoles.map((console) => (
            <div
              key={console.name}
              onClick={() => setSelectedConsole(console)}
              className="group relative h-64 rounded-xl bg-[#18181b] border border-[#262632] hover:border-white p-4 flex flex-col justify-between cursor-pointer transition-all hover:scale-[1.02] shadow-lg overflow-hidden"
            >
              {/* Hardware Photo / Cutout */}
              <div className="relative flex-1 flex items-center justify-center p-2">
                {console.artwork_path ? (
                  <img
                    src={console.artwork_path}
                    alt={console.name}
                    className="max-h-36 max-w-full object-contain filter drop-shadow-md group-hover:scale-105 transition-transform duration-300"
                    onError={(e) => {
                      (e.target as HTMLElement).style.display = 'none';
                    }}
                  />
                ) : console.logo_path ? (
                  <img
                    src={console.logo_path}
                    alt={console.name}
                    className="max-h-24 max-w-full object-contain brightness-0 invert opacity-90"
                  />
                ) : null}
              </div>

              {/* Console name & badge */}
              <div className="pt-2 border-t border-[#242430]">
                <div className="text-sm font-extrabold text-white truncate">
                  {console.name}
                </div>
                <div className="text-[11px] text-[#8a8a94] mt-0.5 font-mono">
                  {isEmulatorsView
                    ? t(language, 'TXT_LIBRARY_EMULATOR_COUNT', 0)
                    : console.title_id_prefix
                    ? `${console.title_id_prefix} PS`
                    : 'Retro platform'}
                </div>
              </div>
            </div>
          ))}
        </div>
      </div>
    );
  }

  // Top Level: Companies Grid (10 companies)
  return (
    <div className="flex-1 h-full overflow-y-auto p-8 select-none">
      <div className="max-w-6xl mb-6">
        <h2 className="text-2xl font-black text-white tracking-tight">
          {sectionTitle}
        </h2>
        <p className="text-xs text-[#8a8a94] mt-1">
          {t(language, 'TXT_CHOOSE_COMPANY')} • 58 systems
        </p>
      </div>

      <div className="grid grid-cols-2 sm:grid-cols-3 lg:grid-cols-4 gap-5 max-w-6xl">
        {COMPANIES.map((company) => (
          <div
            key={company.name}
            onClick={() => setSelectedCompany(company)}
            className="group relative h-64 rounded-xl bg-[#18181b] border border-[#262632] hover:border-white p-5 flex flex-col justify-between cursor-pointer transition-all hover:scale-[1.02] shadow-lg overflow-hidden"
          >
            {/* Color accent glow in background */}
            <div
              className="absolute -top-12 -right-12 w-32 h-32 rounded-full opacity-15 filter blur-2xl transition-opacity group-hover:opacity-30"
              style={{
                backgroundColor: `rgb(${company.r}, ${company.g}, ${company.b})`
              }}
            />

            {/* Company Logo in center */}
            <div className="relative flex-1 flex items-center justify-center p-4">
              {company.artwork_path ? (
                <img
                  src={company.artwork_path}
                  alt={company.name}
                  className="max-h-24 max-w-full object-contain filter drop-shadow group-hover:scale-105 transition-transform duration-300"
                  onError={(e) => {
                    (e.target as HTMLElement).style.display = 'none';
                  }}
                />
              ) : (
                <div className="text-2xl font-black text-white tracking-wider text-center">
                  {company.name}
                </div>
              )}
            </div>

            {/* Company name & console count footer */}
            <div className="pt-2 border-t border-[#23232c] flex items-center justify-between">
              <span className="text-sm font-extrabold text-white">
                {company.name}
              </span>
              <span className="text-xs font-mono text-[#8a8a94]">
                {t(language, 'TXT_CONSOLE_COUNT', company.consoles.length)}
              </span>
            </div>
          </div>
        ))}
      </div>
    </div>
  );
};
