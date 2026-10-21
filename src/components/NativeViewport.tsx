import React, { useEffect, useState } from 'react';
import { fitEdgeToEdgeViewport, TV_LAYOUT } from '../layout';

export function NativeViewport({ children }: { children: React.ReactNode }) {
  const [viewport, setViewport] = useState(() => ({
    width: typeof window === 'undefined' ? TV_LAYOUT.width : window.innerWidth,
    height: typeof window === 'undefined' ? TV_LAYOUT.height : window.innerHeight
  }));
  useEffect(() => {
    const resize = () => setViewport({ width: window.innerWidth, height: window.innerHeight });
    window.addEventListener('resize', resize);
    resize();
    return () => window.removeEventListener('resize', resize);
  }, []);
  const fit = fitEdgeToEdgeViewport(viewport.width, viewport.height);
  // A phone must not shrink 44px TV controls to ~9px. Reflow the same React
  // screens and actions at native CSS-pixel size; preserve the TV stage on PC.
  const mobile = viewport.width <= 1024;
  return <div className="tv-viewport">
    <div className={`tv-stage${mobile ? ' tv-stage-mobile' : ''}`} style={{
      width: mobile ? viewport.width : fit.stageWidth,
      height: mobile ? viewport.height : TV_LAYOUT.height,
      left: mobile ? 0 : fit.left,
      top: mobile ? 0 : fit.top,
      transform: mobile ? 'none' : `scale(${fit.scale})`,
      '--tv-bleed-width': `${mobile ? viewport.width : fit.stageWidth}px`
    } as React.CSSProperties}>
      {children}
    </div>
  </div>;
}
