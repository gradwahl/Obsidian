import React, { useLayoutEffect, useRef } from 'react';

export function fittedFontSize(naturalWidth: number, availableWidth: number, baseSize: number) {
  return naturalWidth > availableWidth && naturalWidth > 0
    ? baseSize * Math.max(0, availableWidth) / naturalWidth : baseSize;
}

// The native renderer shrinks translated labels to fit rather than truncating them.
export function FittedText({ children }: { children: string }) {
  const ref = useRef<HTMLSpanElement>(null);
  useLayoutEffect(() => {
    const fit = () => {
      const node = ref.current;
      if (!node?.isConnected || !node.parentElement) return;
      node.style.fontSize = '';
      const parent = node.parentElement;
      const padding = getComputedStyle(parent);
      const available = parent.clientWidth - parseFloat(padding.paddingLeft) - parseFloat(padding.paddingRight);
      const base = parseFloat(getComputedStyle(node).fontSize);
      node.style.fontSize = `${fittedFontSize(node.scrollWidth, available, base)}px`;
    };
    fit();
    let active = true;
    document.fonts?.ready.then(() => { if (active) fit(); });
    return () => { active = false; };
  }, [children]);
  return <span className="tv-fitted-text" ref={ref}>{children}</span>;
}
