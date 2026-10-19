import React from 'react';
import { GameItem, LanguageCode } from '../types';

interface GameCardProps {
  game: GameItem;
  isSelected: boolean;
  onSelect: () => void;
  onOpen: () => void;
  language: LanguageCode;
}

export const GameCard: React.FC<GameCardProps> = ({
  game,
  isSelected,
  onSelect,
  onOpen
}) => {
  const coverSrc = game.cover_local_path || game.cover_url || '';
  const formatBadge =
    game.format === 'exfat' ? 'EXFAT' : game.format === 'ffpkg' ? 'FFPKG' : 'FPKG';

  return (
    <div
      onClick={() => {
        onSelect();
        onOpen();
      }}
      className={`relative w-[340px] shrink-0 rounded-md overflow-hidden bg-[#18181b] cursor-pointer transition-all duration-200 select-none ${
        isSelected
          ? 'ring-4 ring-white shadow-2xl scale-[1.03] z-20'
          : 'hover:scale-[1.01] hover:ring-2 hover:ring-[#444455] opacity-90 hover:opacity-100 z-10'
      }`}
    >
      {/* Landscape Image container (340x192) */}
      <div className="relative h-[192px] w-full bg-[#121214] overflow-hidden">
        {coverSrc ? (
          <img
            src={coverSrc}
            alt={game.title}
            className="w-full h-full object-cover object-center transition-transform duration-300"
            loading="lazy"
            onError={(e) => {
              (e.target as HTMLElement).style.display = 'none';
            }}
          />
        ) : null}

        {/* Elegant Wolodrive Gradient Fallback Placeholder */}
        <div
          className="absolute inset-0 flex flex-col justify-between p-3.5"
          style={{
            background: `linear-gradient(135deg, rgb(${game.cover_r}, ${game.cover_g}, ${game.cover_b}) 0%, rgb(${Math.floor(
              game.cover_r / 3
            )}, ${Math.floor(game.cover_g / 3)}, ${Math.floor(game.cover_b / 3)}) 100%)`,
            zIndex: -1
          }}
        >
          <div className="text-[11px] font-black tracking-widest text-white/50">WOLODRIVE</div>
          <div className="text-sm font-bold text-white/90 line-clamp-2 drop-shadow">
            {game.title}
          </div>
        </div>

        {/* Format badge tag at top-right */}
        <div className="absolute top-2.5 right-2.5">
          <span className="px-2 py-0.5 rounded text-[10px] font-black tracking-wider bg-black/80 text-white border border-white/20 backdrop-blur-sm">
            {formatBadge}
          </span>
        </div>

        {/* Title ID tag at top-left */}
        <div className="absolute top-2.5 left-2.5">
          <span className="px-2 py-0.5 rounded text-[10px] font-mono font-bold bg-black/80 text-[#e0e0e8] border border-white/20 backdrop-blur-sm">
            {game.id_code}
          </span>
        </div>
      </div>

      {/* Card Action Footer */}
      <div className="px-3.5 py-2.5 bg-[#141416] border-t border-[#232328] flex items-center justify-between">
        <div className="truncate text-xs font-semibold text-white/95 max-w-[220px]">
          {game.title}
        </div>
        <div className="text-[11px] font-mono text-[#8a8a94] shrink-0">
          {game.size_gb.toFixed(1)} GB
        </div>
      </div>
    </div>
  );
};
