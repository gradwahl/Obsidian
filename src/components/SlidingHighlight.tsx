import React, { useLayoutEffect, useRef } from 'react';
import { GlassLiquid } from './GlassLiquid';

// Coordinates are in the logical native stage, not scaled browser pixels.
export function SlidingHighlight({ activeKey, axis = 'y', className = '', motionEnabled = true }: {
  activeKey: string; axis?: 'x' | 'y'; className?: string; motionEnabled?: boolean;
}) {
  const ref = useRef<HTMLDivElement>(null);
  const previous = useRef<DOMRect | null>(null);
  const flight = useRef<Animation | null>(null);
  const glowFlight = useRef<Animation | null>(null);
  const key = useRef(activeKey);

  useLayoutEffect(() => {
    const node = ref.current;
    const parent = node?.parentElement;
    if (!node || !parent) return;
    let disposed = false;
    const measure = (animate: boolean) => {
      if (disposed) return;
      const target = Array.from(parent.children).find(child =>
        (child as HTMLElement).dataset.highlightKey === activeKey) as HTMLElement | undefined;
      if (!target || !parent.offsetWidth || !parent.offsetHeight) {
        flight.current?.cancel(); previous.current = null; node.style.opacity = '0'; return;
      }
      const dest = new DOMRect(target.offsetLeft, target.offsetTop, target.offsetWidth, target.offsetHeight);
      // Capture the interpolated edges before cancelling an interrupted flight.
      const rect = node.getBoundingClientRect();
      const bounds = parent.getBoundingClientRect();
      const scale = bounds.width / parent.offsetWidth || 1;
      const old = previous.current && new DOMRect((rect.x - bounds.x) / scale,
        (rect.y - bounds.y) / scale, rect.width / scale, rect.height / scale);
      flight.current?.cancel();
      glowFlight.current?.cancel();
      const transform = (r: DOMRect) => `translate(${r.x}px, ${r.y}px) scale(${r.width / dest.width}, ${r.height / dest.height})`;
      Object.assign(node.style, { width: `${dest.width}px`, height: `${dest.height}px`,
        transform: transform(dest), opacity: '1', willChange: 'auto' });
      previous.current = dest;
      if (!animate || !motionEnabled || !old || typeof node.animate !== 'function') return;
      const reduced = window.matchMedia('(prefers-reduced-motion: reduce)').matches;
      const union = axis === 'x'
        ? new DOMRect(Math.min(old.x, dest.x), dest.y, Math.max(old.right, dest.right) - Math.min(old.x, dest.x), old.height)
        : new DOMRect(dest.x, Math.min(old.y, dest.y), dest.width, Math.max(old.bottom, dest.bottom) - Math.min(old.y, dest.y));
      const duration = Math.min(520, 280 + Math.hypot(dest.x - old.x, dest.y - old.y) * .25);
      node.style.willChange = 'transform, opacity';
      flight.current = node.animate(reduced ? [
        { transform: transform(old), opacity: 1, offset: 0 },
        { transform: transform(old), opacity: 0, offset: .49 },
        { transform: transform(dest), opacity: 0, offset: .5 },
        { transform: transform(dest), opacity: 1, offset: 1 }
      ] : [
        { transform: transform(old), offset: 0 },
        { transform: transform(union), offset: .45 },
        { transform: transform(dest), offset: 1 }
      ], { duration: reduced ? 150 : duration, easing: 'cubic-bezier(0.22, 1, 0.36, 1)' });
      flight.current.onfinish = () => { node.style.willChange = 'auto'; };
      glowFlight.current = node.querySelector('.highlight-glow')?.animate(
        [{ opacity: 1 }, { opacity: .7, offset: .45 }, { opacity: 1 }],
        { duration: reduced ? 150 : duration }) || null;
    };
    measure(key.current !== activeKey);
    key.current = activeKey;
    let parentWidth = parent.offsetWidth, parentHeight = parent.offsetHeight;
    const resize = () => {
      const target = Array.from(parent.children).find(child => (child as HTMLElement).dataset.highlightKey === activeKey) as HTMLElement | undefined;
      const old = previous.current;
      if (!target || !old || parentWidth !== parent.offsetWidth || parentHeight !== parent.offsetHeight ||
          old.x !== target.offsetLeft || old.y !== target.offsetTop || old.width !== target.offsetWidth || old.height !== target.offsetHeight) {
        measure(parentWidth === parent.offsetWidth && parentHeight === parent.offsetHeight);
      }
      parentWidth = parent.offsetWidth; parentHeight = parent.offsetHeight;
    };
    const observer = typeof ResizeObserver === 'function' ? new ResizeObserver(resize) : null;
    observer?.observe(parent);
    Array.from(parent.children).forEach(child => { if (child !== node) observer?.observe(child); });
    const windowResize = () => measure(false);
    window.addEventListener('resize', windowResize);
    document.fonts?.ready.then(() => measure(false));
    return () => { disposed = true; observer?.disconnect(); window.removeEventListener('resize', windowResize); };
  }, [activeKey, axis, motionEnabled]);
  useLayoutEffect(() => () => { flight.current?.cancel(); glowFlight.current?.cancel(); }, []);
  return <div ref={ref} aria-hidden="true" className={`glass-pill glass-pill--active sliding-highlight ${className}`}><GlassLiquid /><span className="highlight-glow" /></div>;
}
