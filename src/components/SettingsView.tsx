import React from 'react';
import { LanguageCode, CatalogFilters, PackageFormat } from '../types';
import { t } from '../translations';
import { SOURCE_WEBSITES } from '../catalog';
import { Check, RotateCcw } from 'lucide-react';

interface SettingsViewProps {
  filters: CatalogFilters;
  onChangeFilters: (filters: CatalogFilters) => void;
  availableSources: string[];
  language: LanguageCode;
  uiTheme: 'netflix' | 'ubiquity';
  gamepadThemeIndex: number;
  onChangeTheme: (theme: 'netflix' | 'ubiquity') => void;
}

export const SettingsView: React.FC<SettingsViewProps> = ({
  filters,
  onChangeFilters,
  availableSources,
  language,
  uiTheme,
  gamepadThemeIndex,
  onChangeTheme
}) => {
  // Checkbox state for "All sources"
  const allSourcesChecked = !filters.sources_limited;
  const partialSources = filters.sources_limited && filters.sources.length > 0;

  const toggleAllSources = () => {
    if (filters.sources_limited) {
      onChangeFilters({
        ...filters,
        sources_limited: false,
        sources: []
      });
    } else {
      onChangeFilters({
        ...filters,
        sources_limited: true,
        sources: []
      });
    }
  };

  const toggleSource = (source: string) => {
    if (!filters.sources_limited) {
      // Switch from all to all except this one
      const remaining = availableSources.filter((s) => s !== source);
      onChangeFilters({
        ...filters,
        sources_limited: true,
        sources: remaining
      });
    } else {
      const exists = filters.sources.includes(source);
      let updated: string[];
      if (exists) {
        updated = filters.sources.filter((s) => s !== source);
      } else {
        updated = [...filters.sources, source];
      }
      onChangeFilters({
        ...filters,
        sources_limited: true,
        sources: updated
      });
    }
  };

  // Format toggles
  const formats: { id: PackageFormat; label: string }[] = [
    { id: 'exfat', label: '.exfat (ShadowMountPlus)' },
    { id: 'ffpkg', label: t(language, 'TXT_FORMAT_FOLDER') },
    { id: 'pkg', label: '.pkg (PS4 FPKG)' },
    { id: 'catalog', label: 'Catalog metadata (website only)' }
  ];

  const allFormatsChecked = filters.disabled_formats.length === 0;
  const partialFormats =
    filters.disabled_formats.length > 0 && filters.disabled_formats.length < formats.length;

  const toggleAllFormats = () => {
    if (allFormatsChecked) {
      onChangeFilters({
        ...filters,
        disabled_formats: ['exfat', 'ffpkg', 'pkg', 'catalog']
      });
    } else {
      onChangeFilters({
        ...filters,
        disabled_formats: []
      });
    }
  };

  const toggleFormat = (fmt: PackageFormat) => {
    const isCurrentlyDisabled = filters.disabled_formats.includes(fmt);
    let updated: PackageFormat[];
    if (isCurrentlyDisabled) {
      updated = filters.disabled_formats.filter((f) => f !== fmt);
    } else {
      updated = [...filters.disabled_formats, fmt];
    }
    onChangeFilters({
      ...filters,
      disabled_formats: updated
    });
  };

  const resetFilters = () => {
    onChangeFilters({
      sources_limited: false,
      sources: [],
      disabled_formats: []
    });
  };

  return (
    <div className="tv-settings tv-page">
      <div className="mb-6">
        <h2 className="text-2xl font-black text-white tracking-tight">
          {t(language, 'TXT_NAV_SETTINGS')}
        </h2>
        <p className="text-xs text-[#8a8a94] mt-1.5 leading-relaxed">
          {t(language, 'TXT_SETTINGS_HELP')}
        </p>
      </div>

      <div className="grid grid-cols-1 md:grid-cols-2 gap-8">
        {/* Left Column: Download Sources */}
        <div className="bg-[#121216] border border-[#202028] rounded-xl p-5">
          <h3 className="text-sm font-extrabold text-white uppercase tracking-wider mb-4">
            {t(language, 'TXT_SETTINGS_SOURCE')}
          </h3>

          <div className="space-y-2">
            {/* All sources option */}
            <div
              onClick={toggleAllSources}
              className="flex items-center gap-3 p-3 rounded-lg bg-[#18181f] hover:bg-[#22222a] border border-[#282834] cursor-pointer transition-colors"
            >
              <div
                className={`w-5 h-5 rounded border flex items-center justify-center transition-colors ${
                  allSourcesChecked
                    ? 'bg-[#e50914] border-[#e50914]'
                    : partialSources
                    ? 'bg-[#2a2a34] border-white/50'
                    : 'border-[#444455] bg-transparent'
                }`}
              >
                {allSourcesChecked ? (
                  <Check className="w-3.5 h-3.5 text-white" />
                ) : partialSources ? (
                  <div className="w-2.5 h-0.5 bg-white" />
                ) : null}
              </div>
              <span className="text-xs font-bold text-white">
                {t(language, 'TXT_SETTINGS_ALL_SOURCES')}
              </span>
            </div>

            {/* Source items */}
            {availableSources.map((source) => {
              const checked = !filters.sources_limited || filters.sources.includes(source);
              return (
                <div
                  key={source}
                  onClick={() => toggleSource(source)}
                  className="flex items-center gap-3 p-3 rounded-lg bg-[#16161b] hover:bg-[#202028] border border-[#22222c] cursor-pointer transition-colors"
                >
                  <div
                    className={`w-5 h-5 rounded border flex items-center justify-center transition-colors ${
                      checked
                        ? 'bg-[#e50914] border-[#e50914]'
                        : 'border-[#444455] bg-transparent'
                    }`}
                  >
                    {checked && <Check className="w-3.5 h-3.5 text-white" />}
                  </div>
                  <span className="text-xs font-medium text-[#c8c8d2] truncate">
                    {source}
                  </span>
                  {SOURCE_WEBSITES[source] && (
                    <a href={SOURCE_WEBSITES[source]} onClick={e => e.stopPropagation()}
                      target="_blank" rel="noopener noreferrer" aria-label={`Open ${source} website`}
                      className="ml-auto text-[11px] text-[#9d9df0] hover:text-white shrink-0">
                      Website ↗
                    </a>
                  )}
                </div>
              );
            })}
          </div>
        </div>

        {/* Right Column: Package Formats */}
        <div className="bg-[#121216] border border-[#202028] rounded-xl p-5 flex flex-col justify-between">
          <div>
            <h3 className="text-sm font-extrabold text-white uppercase tracking-wider mb-4">
              {t(language, 'TXT_SETTINGS_FORMAT')}
            </h3>

            <div className="space-y-2">
              {/* All formats option */}
              <div
                onClick={toggleAllFormats}
                className="flex items-center gap-3 p-3 rounded-lg bg-[#18181f] hover:bg-[#22222a] border border-[#282834] cursor-pointer transition-colors"
              >
                <div
                  className={`w-5 h-5 rounded border flex items-center justify-center transition-colors ${
                    allFormatsChecked
                      ? 'bg-[#e50914] border-[#e50914]'
                      : partialFormats
                      ? 'bg-[#2a2a34] border-white/50'
                      : 'border-[#444455] bg-transparent'
                  }`}
                >
                  {allFormatsChecked ? (
                    <Check className="w-3.5 h-3.5 text-white" />
                  ) : partialFormats ? (
                    <div className="w-2.5 h-0.5 bg-white" />
                  ) : null}
                </div>
                <span className="text-xs font-bold text-white">
                  {t(language, 'TXT_SETTINGS_ALL_FORMATS')}
                </span>
              </div>

              {/* Individual format options */}
              {formats.map((fmt) => {
                const checked = !filters.disabled_formats.includes(fmt.id);
                return (
                  <div
                    key={fmt.id}
                    onClick={() => toggleFormat(fmt.id)}
                    className="flex items-center gap-3 p-3 rounded-lg bg-[#16161b] hover:bg-[#202028] border border-[#22222c] cursor-pointer transition-colors"
                  >
                    <div
                      className={`w-5 h-5 rounded border flex items-center justify-center transition-colors ${
                        checked
                          ? 'bg-[#e50914] border-[#e50914]'
                          : 'border-[#444455] bg-transparent'
                      }`}
                    >
                      {checked && <Check className="w-3.5 h-3.5 text-white" />}
                    </div>
                    <span className="text-xs font-medium text-[#c8c8d2]">
                      {fmt.label}
                    </span>
                  </div>
                );
              })}
            </div>
          </div>

          <div className="tv-theme-settings" role="group" aria-label="Appearance">
            <h3 className="tv-theme-title">Appearance / Wygląd</h3>
            <div className="tv-theme-choices">
              <button type="button"
                className={'tv-theme-option ' + (uiTheme === 'netflix' ? 'tv-theme-option-active' : '') + (gamepadThemeIndex === 0 ? ' tv-theme-option-gamepad' : '')}
                aria-pressed={uiTheme === 'netflix'} onClick={() => onChangeTheme('netflix')}>
                <span>Netflix / Glass</span><span>{uiTheme === 'netflix' ? '●' : '○'}</span>
              </button>
              <button type="button"
                className={'tv-theme-option ' + (uiTheme === 'ubiquity' ? 'tv-theme-option-active' : '') + (gamepadThemeIndex === 1 ? ' tv-theme-option-gamepad' : '')}
                aria-pressed={uiTheme === 'ubiquity'} onClick={() => onChangeTheme('ubiquity')}>
                <span>(u)biquity</span><span>{uiTheme === 'ubiquity' ? '●' : '○'}</span>
              </button>
            </div>
          </div>

          {/* Reset Filters Button */}
          <div className="mt-8 pt-5 border-t border-[#202028]">
            <button
              onClick={resetFilters}
              className="w-full flex items-center justify-center gap-2 py-3 px-4 rounded-lg bg-[#24242e] hover:bg-white hover:text-black text-xs font-bold text-white transition-all active:scale-98 border border-[#343444]"
            >
              <RotateCcw className="w-3.5 h-3.5" />
              <span>{t(language, 'TXT_SETTINGS_RESET')}</span>
            </button>
          </div>
        </div>
      </div>

      <div className="mt-6 text-[11px] text-[#71717a] font-mono">
        {t(language, 'TXT_SETTINGS_HINT')}
      </div>
    </div>
  );
};
