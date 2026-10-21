import React, { useState, useEffect } from 'react';
import { LanguageCode, CatalogFilters, PackageFormat } from '../types';
import { t } from '../translations';
import { SOURCE_WEBSITES } from '../catalog';
import { Check, RotateCcw, ShieldCheck, Cpu, HardDrive, Layers, Globe, Radio, Sparkles } from 'lucide-react';

interface SettingsViewProps {
  filters: CatalogFilters;
  onChangeFilters: (filters: CatalogFilters) => void;
  availableSources: string[];
  language: LanguageCode;
  catalogCount?: number;
}

export const SettingsView: React.FC<SettingsViewProps> = ({
  filters,
  onChangeFilters,
  availableSources,
  language,
  catalogCount = 1248
}) => {
  const [expandedSection, setExpandedSection] = useState<'sources' | 'formats' | null>(null);
  const [isOnline, setIsOnline] = useState<boolean>(
    typeof navigator !== 'undefined' ? navigator.onLine : true
  );

  useEffect(() => {
    const handleOnline = () => setIsOnline(true);
    const handleOffline = () => setIsOnline(false);

    window.addEventListener('online', handleOnline);
    window.addEventListener('offline', handleOffline);

    return () => {
      window.removeEventListener('online', handleOnline);
      window.removeEventListener('offline', handleOffline);
    };
  }, []);

  const allSourcesChecked = !filters.sources_limited;

  const toggleAllSources = () => {
    if (filters.sources_limited) {
      onChangeFilters({ ...filters, sources_limited: false, sources: [] });
    } else {
      onChangeFilters({ ...filters, sources_limited: true, sources: [] });
    }
  };

  const toggleSource = (source: string) => {
    if (!filters.sources_limited) {
      const remaining = availableSources.filter((s) => s !== source);
      onChangeFilters({ ...filters, sources_limited: true, sources: remaining });
    } else {
      const exists = filters.sources.includes(source);
      const updated = exists
        ? filters.sources.filter((s) => s !== source)
        : [...filters.sources, source];
      onChangeFilters({ ...filters, sources_limited: true, sources: updated });
    }
  };

  const formats: { id: PackageFormat; label: string }[] = [
    { id: 'exfat', label: '.exfat (ShadowMountPlus)' },
    { id: 'ffpkg', label: t(language, 'TXT_FORMAT_FOLDER') },
    { id: 'pkg', label: '.pkg (PS4 FPKG)' },
    { id: 'catalog', label: 'Catalog metadata (website only)' }
  ];

  const toggleFormat = (fmt: PackageFormat) => {
    const isCurrentlyDisabled = filters.disabled_formats.includes(fmt);
    const updated = isCurrentlyDisabled
      ? filters.disabled_formats.filter((f) => f !== fmt)
      : [...filters.disabled_formats, fmt];
    onChangeFilters({ ...filters, disabled_formats: updated });
  };

  const resetFilters = () => {
    onChangeFilters({ sources_limited: false, sources: [], disabled_formats: [] });
  };

  const activeSourcesCount = allSourcesChecked
    ? availableSources.length || 10
    : filters.sources.length;

  const activeFormatsCount = formats.length - filters.disabled_formats.length;

  return (
    <div className="tv-settings tv-page px-6 pt-4 pb-12 text-white">
      {/* View Header */}
      <div className="mb-6">
        <h2 className="text-2xl font-black text-white tracking-tight flex items-center gap-3">
          <ShieldCheck className="w-6 h-6 text-[#e50914]" />
          <span>{t(language, 'TXT_NAV_SETTINGS')}</span>
        </h2>
        <p className="text-xs text-[#8a8a94] mt-1 leading-relaxed">
          {t(language, 'TXT_SETTINGS_HELP')}
        </p>
      </div>

      {/* Modern 2-Column PS5 Dashboard Layout */}
      <div className="grid grid-cols-1 lg:grid-cols-12 gap-8">
        {/* Left Column: Option Rows with Colored Status Capsules (7 Cols) */}
        <div className="lg:col-span-7 space-y-3">
          {/* Row 1: Network Status */}
          <div className="bg-[#14141c]/85 backdrop-blur-md border border-white/10 rounded-[18px] p-4 flex items-center justify-between transition-all hover:border-white/20">
            <div className="flex items-center gap-3">
              <div className="w-9 h-9 rounded-full bg-[#1c1c28] border border-white/10 flex items-center justify-center text-[#e50914]">
                <Globe className="w-4 h-4" />
              </div>
              <div>
                <div className="text-sm font-bold text-white">{t(language, 'TXT_NET_CONNECTION')}</div>
                <div className="text-[11px] text-[#8a8a9a]">{t(language, 'TXT_NET_STATUS_SUB')}</div>
              </div>
            </div>
            <span
              className={`px-3.5 py-1.5 rounded-full text-xs font-extrabold border ${
                isOnline
                  ? 'bg-[#162e21] text-[#22c55e] border-[#22c55e]/40 shadow-[0_0_10px_rgba(34,197,94,0.2)]'
                  : 'bg-[#2a1a20] text-[#ef4444] border-[#ef4444]/40 shadow-[0_0_10px_rgba(239,68,68,0.2)]'
              }`}
            >
              {isOnline ? t(language, 'TXT_CONNECTED') : t(language, 'TXT_DISCONNECTED')}
            </span>
          </div>

          {/* Row 2: Target Storage Location */}
          <div className="bg-[#14141c]/85 backdrop-blur-md border border-white/10 rounded-[18px] p-4 flex items-center justify-between transition-all hover:border-white/20">
            <div className="flex items-center gap-3">
              <div className="w-9 h-9 rounded-full bg-[#1c1c28] border border-white/10 flex items-center justify-center text-[#e50914]">
                <HardDrive className="w-4 h-4" />
              </div>
              <div>
                <div className="text-sm font-bold text-white">{t(language, 'TXT_STORAGE_LOCATION')}</div>
                <div className="text-[11px] text-[#8a8a9a]">{t(language, 'TXT_STORAGE_LOCATION_SUB')}</div>
              </div>
            </div>
            <span className="px-3.5 py-1.5 rounded-full text-xs font-extrabold bg-[#20202e] text-[#a0a0b2] border border-white/15">
              {t(language, 'TXT_CONSOLE_ONLY')}
            </span>
          </div>

          {/* Row 3: ShadowMountPlus Driver */}
          <div className="bg-[#14141c]/85 backdrop-blur-md border border-white/10 rounded-[18px] p-4 flex items-center justify-between transition-all hover:border-white/20">
            <div className="flex items-center gap-3">
              <div className="w-9 h-9 rounded-full bg-[#1c1c28] border border-white/10 flex items-center justify-center text-[#e50914]">
                <Radio className="w-4 h-4" />
              </div>
              <div>
                <div className="text-sm font-bold text-white">ShadowMountPlus</div>
                <div className="text-[11px] text-[#8a8a9a]">{t(language, 'TXT_SHADOWMOUNT_SUB')}</div>
              </div>
            </div>
            <span className="px-3.5 py-1.5 rounded-full text-xs font-extrabold bg-[#20202e] text-[#a0a0b2] border border-white/15">
              {t(language, 'TXT_UNAVAILABLE')}
            </span>
          </div>

          {/* Row 4: App Version Status */}
          <div className="bg-[#14141c]/85 backdrop-blur-md border border-white/10 rounded-[18px] p-4 flex items-center justify-between transition-all hover:border-white/20">
            <div className="flex items-center gap-3">
              <div className="w-9 h-9 rounded-full bg-[#1c1c28] border border-white/10 flex items-center justify-center text-[#e50914]">
                <Cpu className="w-4 h-4" />
              </div>
              <div>
                <div className="text-sm font-bold text-white">{t(language, 'TXT_POLDERIVE_VER')}</div>
                <div className="text-[11px] text-[#8a8a9a]">{t(language, 'TXT_POLDERIVE_VER_SUB')}</div>
              </div>
            </div>
            <span className="px-3.5 py-1.5 rounded-full text-xs font-extrabold bg-[#1f1e2e] text-[#a0a0ff] border border-[#6366f1]/40">
              {t(language, 'TXT_STATUS_CURRENT')}
            </span>
          </div>

          {/* Row 5: Sources Selector Expandable Row */}
          <div className="bg-[#14141c]/85 backdrop-blur-md border border-white/10 rounded-[18px] p-4 transition-all hover:border-white/20">
            <div role="button" tabIndex={0}
              onKeyDown={(e) => { if (e.key === 'Enter' || e.key === ' ') { e.preventDefault(); e.currentTarget.click(); } }}
              onClick={() => setExpandedSection(expandedSection === 'sources' ? null : 'sources')}
              className="flex items-center justify-between cursor-pointer"
            >
              <div className="flex items-center gap-3">
                <div className="w-9 h-9 rounded-full bg-[#1c1c28] border border-white/10 flex items-center justify-center text-[#e50914]">
                  <Layers className="w-4 h-4" />
                </div>
                <div>
                  <div className="text-sm font-bold text-white">{t(language, 'TXT_SETTINGS_SOURCE')}</div>
                  <div className="text-[11px] text-[#8a8a9a]">{t(language, 'TXT_SELECT_SOURCES_SUB')}</div>
                </div>
              </div>
              <span className="px-3.5 py-1.5 rounded-full text-xs font-extrabold bg-[#2a1a20] text-[#ff3b45] border border-[#e50914]/40 shadow-[0_0_10px_rgba(229,9,20,0.2)]">
                {allSourcesChecked
                  ? t(language, 'TXT_ALL_COUNT', availableSources.length || 10)
                  : t(language, 'TXT_SELECTED_COUNT', activeSourcesCount)}
              </span>
            </div>

            {/* Expandable Source Checkboxes */}
            {expandedSection === 'sources' && (
              <div className="mt-4 pt-3 border-t border-white/10 space-y-2">
                <div
                  role="checkbox" tabIndex={0} aria-checked={allSourcesChecked}
                  onKeyDown={(e) => { if (e.key === 'Enter' || e.key === ' ') { e.preventDefault(); e.currentTarget.click(); } }}
                  onClick={toggleAllSources}
                  className="flex items-center gap-3 p-2.5 rounded-xl bg-[#1a1a24] border border-white/10 cursor-pointer hover:bg-[#222230]"
                >
                  <div
                    className={`w-4 h-4 rounded border flex items-center justify-center ${
                      allSourcesChecked ? 'bg-[#e50914] border-[#e50914]' : 'border-white/40'
                    }`}
                  >
                    {allSourcesChecked && <Check className="w-3 h-3 text-white" />}
                  </div>
                  <span className="text-xs font-bold text-white">
                    {t(language, 'TXT_SETTINGS_ALL_SOURCES')}
                  </span>
                </div>

                {availableSources.map((source) => {
                  const checked = !filters.sources_limited || filters.sources.includes(source);
                  return (
                    <div
                      key={source}
                      role="checkbox" tabIndex={0} aria-checked={checked}
                      onKeyDown={(e) => { if (e.key === 'Enter' || e.key === ' ') { e.preventDefault(); e.currentTarget.click(); } }}
                      onClick={() => toggleSource(source)}
                      className="flex items-center gap-3 p-2.5 rounded-xl bg-[#161620] border border-white/5 cursor-pointer hover:bg-[#1f1f2c]"
                    >
                      <div
                        className={`w-4 h-4 rounded border flex items-center justify-center ${
                          checked ? 'bg-[#e50914] border-[#e50914]' : 'border-white/30'
                        }`}
                      >
                        {checked && <Check className="w-3 h-3 text-white" />}
                      </div>
                      <span className="text-xs text-[#c8c8d8]">{source}</span>
                      {SOURCE_WEBSITES[source] && (
                        <a
                          href={SOURCE_WEBSITES[source]}
                          onClick={(e) => e.stopPropagation()}
                          target="_blank"
                          rel="noopener noreferrer"
                          className="ml-auto text-[11px] text-[#818cf8] hover:text-white"
                        >
                          Website ↗
                        </a>
                      )}
                    </div>
                  );
                })}
              </div>
            )}
          </div>

          {/* Row 6: Formats Selector Expandable Row */}
          <div className="bg-[#14141c]/85 backdrop-blur-md border border-white/10 rounded-[18px] p-4 transition-all hover:border-white/20">
            <div role="button" tabIndex={0}
              onKeyDown={(e) => { if (e.key === 'Enter' || e.key === ' ') { e.preventDefault(); e.currentTarget.click(); } }}
              onClick={() => setExpandedSection(expandedSection === 'formats' ? null : 'formats')}
              className="flex items-center justify-between cursor-pointer"
            >
              <div className="flex items-center gap-3">
                <div className="w-9 h-9 rounded-full bg-[#1c1c28] border border-white/10 flex items-center justify-center text-[#e50914]">
                  <Sparkles className="w-4 h-4" />
                </div>
                <div>
                  <div className="text-sm font-bold text-white">{t(language, 'TXT_SETTINGS_FORMAT')}</div>
                  <div className="text-[11px] text-[#8a8a9a]">{t(language, 'TXT_ACTIVE_FORMATS_SUB')}</div>
                </div>
              </div>
              <span className="px-3.5 py-1.5 rounded-full text-xs font-extrabold bg-[#1f1e2e] text-[#a0a0ff] border border-[#6366f1]/40">
                {t(language, 'TXT_ACTIVE_COUNT_FORMATS', activeFormatsCount, formats.length)}
              </span>
            </div>

            {/* Expandable Formats Checkboxes */}
            {expandedSection === 'formats' && (
              <div className="mt-4 pt-3 border-t border-white/10 space-y-2">
                {formats.map((fmt) => {
                  const checked = !filters.disabled_formats.includes(fmt.id);
                  return (
                    <div
                      key={fmt.id}
                      role="checkbox" tabIndex={0} aria-checked={checked}
                      onKeyDown={(e) => { if (e.key === 'Enter' || e.key === ' ') { e.preventDefault(); e.currentTarget.click(); } }}
                      onClick={() => toggleFormat(fmt.id)}
                      className="flex items-center gap-3 p-2.5 rounded-xl bg-[#161620] border border-white/5 cursor-pointer hover:bg-[#1f1f2c]"
                    >
                      <div
                        className={`w-4 h-4 rounded border flex items-center justify-center ${
                          checked ? 'bg-[#e50914] border-[#e50914]' : 'border-white/30'
                        }`}
                      >
                        {checked && <Check className="w-3 h-3 text-white" />}
                      </div>
                      <span className="text-xs text-[#c8c8d8]">{fmt.label}</span>
                    </div>
                  );
                })}
              </div>
            )}
          </div>

          {/* Reset Action Button */}
          <button
            type="button"
            onClick={resetFilters}
            className="w-full flex items-center justify-center gap-2 py-3 px-4 rounded-[18px] bg-[#1c1c28] hover:bg-[#e50914] text-xs font-bold text-white border border-white/15 hover:border-[#e50914] transition-all shadow-md active:scale-98"
          >
            <RotateCcw className="w-4 h-4" />
            <span>{t(language, 'TXT_SETTINGS_RESET')}</span>
          </button>
        </div>

        {/* Right Column: Glass Info Card "Obsidian System" (5 Cols) */}
        <div className="lg:col-span-5">
          <div className="bg-[#14141c]/85 backdrop-blur-md border border-white/12 rounded-[18px] p-6 shadow-xl relative overflow-hidden">
            {/* Ambient Background Glow */}
            <div className="absolute -top-10 -right-10 w-40 h-40 bg-[#e50914]/15 rounded-full blur-2xl pointer-events-none" />

            {/* Card Header */}
            <div className="flex items-center justify-between pb-4 border-b border-white/10 mb-5">
              <div className="flex items-center gap-3">
                <div className="w-10 h-10 rounded-xl bg-gradient-to-br from-[#ff1e27] to-[#c80812] flex items-center justify-center text-white font-black text-lg shadow-[0_0_14px_rgba(229,9,20,0.5)]">
                  P
                </div>
                <div>
                  <h3 className="text-base font-black text-white tracking-tight">Obsidian System</h3>
                  <div className="text-xs text-[#a0a0b2] font-mono">{t(language, 'TXT_SYSTEM_STATUS')}</div>
                </div>
              </div>
              <span className="px-3 py-1 rounded-full text-[11px] font-extrabold bg-[#162e21] text-[#22c55e] border border-[#22c55e]/40 shadow-[0_0_8px_rgba(34,197,94,0.3)]">
                SYSTEM OK
              </span>
            </div>

            {/* System Details List */}
            <div className="space-y-4 font-mono text-xs">
              <div className="flex items-center justify-between py-1 border-b border-white/5">
                <span className="text-[#a0a0b2]">{t(language, 'TXT_SYS_VERSION')}</span>
                <span className="font-bold text-white">v2.0.0-PRO (Build 2026.10)</span>
              </div>

              <div className="flex items-center justify-between py-1 border-b border-white/5">
                <span className="text-[#a0a0b2]">Firmware PS5</span>
                <span className="font-bold text-[#22c55e]">{t(language, 'TXT_SYS_COMPATIBLE')}</span>
              </div>

              <div className="flex items-center justify-between py-1 border-b border-white/5">
                <span className="text-[#a0a0b2]">ShadowMountPlus</span>
                <span className="font-bold text-[#9090a0]">{t(language, 'TXT_UNAVAILABLE')}</span>
              </div>

              <div className="flex items-center justify-between py-1 border-b border-white/5">
                <span className="text-[#a0a0b2]">{t(language, 'TXT_SYS_ACTIVE_CATS')}</span>
                <span className="font-bold text-[#ff3b45]">
                  {t(language, 'TXT_SYS_SOURCES_GAMES', activeSourcesCount, catalogCount.toLocaleString())}
                </span>
              </div>

              <div className="flex items-center justify-between py-1 border-b border-white/5">
                <span className="text-[#a0a0b2]">{t(language, 'TXT_SYS_KERNEL')}</span>
                <span className="font-bold text-white">Web React Engine</span>
              </div>

              <div className="flex items-center justify-between py-1">
                <span className="text-[#a0a0b2]">{t(language, 'TXT_SYS_STORAGE')}</span>
                <span className="font-bold text-[#9090a0]">{t(language, 'TXT_CONSOLE_ONLY')}</span>
              </div>
            </div>

            {/* Footer Summary Note */}
            <div className="mt-6 pt-4 border-t border-white/10 text-[11px] text-[#8a8a9a] leading-relaxed">
              {t(language, 'TXT_SYS_FOOTER')}
            </div>
          </div>
        </div>
      </div>
    </div>
  );
};
