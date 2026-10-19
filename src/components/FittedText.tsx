import React, { useLayoutEffect, useRef } from 'react';

export function fittedFontSize(naturalWidth: number, availableWidth: number, baseSize: number) {
  if (naturalWidth <= 0 || availableWidth <= 0) return baseSize;
  return naturalWidth > availableWidth
    ? Math.max(10, (baseSize * availableWidth) / naturalWidth)
    : baseSize;
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
      if (available <= 0) return;
      const base = parseFloat(getComputedStyle(node).fontSize) || 20;
      node.style.fontSize = (fittedFontSize(node.scrollWidth, available, base)) + 'px';
    };

    fit();
    let active = true;
    document.fonts?.ready.then(() => { if (active) fit(); });

    const observer = typeof ResizeObserver !== 'undefined' && ref.current?.parentElement
      ? new ResizeObserver(() => fit())
      : null;
    if (observer && ref.current?.parentElement) {
      observer.observe(ref.current.parentElement);
    }

    return () => {
      active = false;
      observer?.disconnect();
    };
  }, [children]);

  return <span className="tv-fitted-text" ref={ref}>{children}</span>;
}
