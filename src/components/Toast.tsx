import React from 'react';
import { Bell } from 'lucide-react';

interface ToastProps {
  message: string | null;
  onDismiss: () => void;
}

export const Toast: React.FC<ToastProps> = ({ message }) => {
  if (!message) return null;

  return (
    <div className="fixed top-6 right-6 z-60 max-w-md animate-in slide-in-from-top-4 fade-in duration-200">
      <div className="flex items-center gap-3.5 px-4.5 py-3.5 bg-[#181820] text-white rounded-lg border-l-4 border-l-[#e50914] border border-[#2c2c3a] shadow-2xl backdrop-blur-md">
        <div className="p-1.5 rounded-full bg-[#e50914]/20 text-[#e50914] shrink-0">
          <Bell className="w-4 h-4" />
        </div>
        <div className="text-xs font-semibold leading-snug">
          {message}
        </div>
      </div>
    </div>
  );
};
