import React from 'react';
import { SystemStats } from '../types';
import { HardDrive, Activity, Wifi } from 'lucide-react';

interface SystemStatsBarProps {
  stats: SystemStats;
  syncStatus: string;
}

export const SystemStatsBar: React.FC<SystemStatsBarProps> = ({
  stats,
  syncStatus
}) => {
  return (
    <footer className="h-10 shrink-0 bg-[#0c0c0e] border-t border-[#1a1a20] px-6 flex items-center justify-between text-[11px] font-mono text-[#8a8a94] select-none z-20">
      {/* Storage & Temps */}
      <div className="flex items-center gap-6">
        <div className="flex items-center gap-2">
          <HardDrive className="w-3.5 h-3.5 text-[#e50914]" />
          <span>
            /data:{' '}
            <strong className="text-white font-semibold">
              {stats.data_free_gb.toFixed(1)} GB
            </strong>{' '}
            / {stats.data_total_gb.toFixed(1)} GB
          </span>
        </div>

        <div className="hidden sm:flex items-center gap-2">
          <HardDrive className="w-3.5 h-3.5 text-[#505060]" />
          <span>
            /mnt/ext0:{' '}
            <strong className="text-white font-semibold">
              {stats.ext0_free_gb.toFixed(1)} GB
            </strong>{' '}
            / {stats.ext0_total_gb.toFixed(1)} GB
          </span>
        </div>

        <div className="hidden md:flex items-center gap-2">
          <Activity className="w-3.5 h-3.5 text-[#4ade80]" />
          <span>
            CPU: <strong className="text-white font-semibold">{stats.cpu_temp_c}°C</strong> | GPU:{' '}
            <strong className="text-white font-semibold">{stats.gpu_temp_c}°C</strong>
          </span>
        </div>
      </div>

      {/* Sync status & Network Badge */}
      <div className="flex items-center gap-4">
        <div className="hidden lg:block text-[#94949c] truncate max-w-sm">
          {syncStatus}
        </div>

        <div className="flex items-center gap-1.5 px-2.5 py-0.5 rounded bg-[#181820] border border-[#262632] text-white">
          <Wifi className="w-3 h-3 text-[#22c55e]" />
          <span className="font-bold text-[10px] tracking-wide">{stats.ip_address}</span>
        </div>
      </div>
    </footer>
  );
};
