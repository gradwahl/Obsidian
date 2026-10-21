import React, { useEffect, useRef } from 'react';

/** Decorative light lives below the label. The parent controls its size,
 * selected state and input semantics; no geometry or click handlers change.
 * Suspend moving patches offscreen and on low-power devices. */
export function GlassLiquid() {
  const ref = useRef<HTMLSpanElement>(null);
  useEffect(() => {
    const pill = ref.current?.parentElement;
    if (!pill) return;
    const memory = (navigator as Navigator & { deviceMemory?: number }).deviceMemory;
    const cores = navigator.hardwareConcurrency;
    const lightweight = (memory !== undefined && memory <= 2) || (cores !== undefined && cores <= 2);
    if (lightweight || typeof IntersectionObserver !== 'function') {
      pill.dataset.glassMotion = lightweight ? 'static' : 'full';
      return () => { delete pill.dataset.glassMotion; };
    }
    pill.dataset.glassMotion = 'rest';
    const observer = new IntersectionObserver(([entry]) => {
      pill.dataset.glassMotion = entry.isIntersecting ? 'full' : 'rest';
    }, { threshold: 0.01 });
    observer.observe(pill);
    return () => { observer.disconnect(); delete pill.dataset.glassMotion; };
  }, []);
  return <span ref={ref} className="glass-liquid" aria-hidden="true">
    <span className="glass-drift glass-drift--a" />
    <span className="glass-drift glass-drift--b" />
    <span className="glass-drift glass-drift--c" />
    <span className="glass-specks">
      <i /><i /><i /><i />
    </span>
  </span>;
}
