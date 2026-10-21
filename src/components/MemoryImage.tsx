import React, { useEffect, useState } from 'react';
import { ArtworkSize, artworkResources } from '../artworkMemory';

/** Only mounted images acquire resized artwork; release on source change/unmount. */
export function MemoryImage({ src, artworkSize = 'card', ...props }:
  React.ImgHTMLAttributes<HTMLImageElement> & { src: string; artworkSize?: ArtworkSize }) {
  const key = artworkSize + '|' + src;
  const [ready, setReady] = useState<{ key: string; url: string } | null>(null);
  useEffect(() => {
    let active = true;
    const lease = artworkResources.acquire(key);
    void lease.promise.then(value => { if (active) setReady({ key, url: value.url }); }, () => {});
    return () => { active = false; lease.release(); };
  }, [key]);
  return <img {...props} src={ready?.key === key ? ready.url : typeof window === 'undefined' ? src : undefined}
    decoding="async" />;
}
