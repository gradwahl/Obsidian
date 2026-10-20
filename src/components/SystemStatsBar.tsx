import React from 'react';
import { SystemStats } from '../types';

interface SystemStatsBarProps {
  stats: SystemStats;
  syncStatus: string;
}

export const SystemStatsBar: React.FC<SystemStatsBarProps> = ({
  syncStatus
}) => {
  return <footer className="tv-status">
    <span title={syncStatus}>{syncStatus}</span>
    <span>data: null | ext0: null | CPU: null | GPU: null</span>
  </footer>;
};
