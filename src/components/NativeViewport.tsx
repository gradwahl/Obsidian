import React, { useEffect, useState } from 'react';
import { fitViewport, TV_LAYOUT } from '../layout';

export function NativeViewport({ children }: { children: React.ReactNode }) {
  const [fit, setFit] = useState(() => fitViewport(
    typeof window === 'undefined' ? TV_LAYOUT.width : window.innerWidth,
    typeof window === 'undefined' ? TV_LAYOUT.height : window.innerHeight
  ));
  useEffect(() => {
    const resize = () => setFit(fitViewport(window.innerWidth, window.innerHeight));
    window.addEventListener('resize', resize);
    resize();
    return () => window.removeEventListener('resize', resize);
  }, []);
  return <div className="tv-viewport">
    <div className="tv-stage" style={{ width: TV_LAYOUT.width, height: TV_LAYOUT.height,
      left: fit.left, top: fit.top, transform: `scale(${fit.scale})` }}>
      {children}
    </div>
  </div>;
}
