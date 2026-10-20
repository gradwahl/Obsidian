import React from 'react';
import { SystemStats, LanguageCode, SidebarView } from '../types';
import { t } from '../translations';

interface SystemStatsBarProps {
  stats: SystemStats;
  syncStatus: string;
  language?: LanguageCode;
  currentView?: SidebarView;
  onRefresh?: () => void;
  syncInProgress?: boolean;
}

export const SystemStatsBar: React.FC<SystemStatsBarProps> = ({
  syncStatus,
  language = 'pl',
  currentView = 'home',
  onRefresh,
  syncInProgress = false
}) => {
  const showKeybinds = currentView !== 'search' && currentView !== 'settings';
  const cleanSync = syncStatus.startsWith('Pegasus-DL')
    ? `Katalogi${syncStatus.slice(10)}`
    : syncStatus;

  return (
    <footer className="tv-status" role="contentinfo">
      <div className="tv-footer-left">
        {showKeybinds && (
          <div className="tv-footer-keybinds">
            <span className="tv-footer-hint">[←→↑↓] Nawigacja  [X] Wybierz</span>
            <span className="tv-footer-hint">{t(language, 'TXT_BACK_HINT')}</span>
            <button
              type="button"
              className="tv-footer-hint tv-footer-btn"
              disabled={syncInProgress}
              onClick={onRefresh}
            >
              {t(language, 'TXT_REFRESH_HINT')}
            </button>
          </div>
        )}
        <span className="tv-footer-sync" title={cleanSync}>
          {cleanSync}
        </span>
      </div>

      <div className="tv-footer-right">
        <span className="tv-footer-telemetry">
          data: null | ext0: null | CPU: null | GPU: null
        </span>
        <div className="tv-footer-logo" aria-label="Poldrive">
          <strong>P</strong>
          <span>oldrive</span>
        </div>
      </div>
    </footer>
  );
};
