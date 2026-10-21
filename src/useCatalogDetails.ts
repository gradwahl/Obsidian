import { useEffect, useState } from 'react';
import { GameItem } from './types';
import { loadCatalogDetails } from './sourceCatalogues';

export function useCatalogDetails<T extends GameItem | null>(game: T): T {
  const [loaded, setLoaded] = useState<{ input: GameItem; game: GameItem } | null>(null);
  useEffect(() => {
    if (!game) return;
    const controller = new AbortController();
    void loadCatalogDetails(game, controller.signal).then(full => {
      if (!controller.signal.aborted) setLoaded({ input: game, game: full });
    }, () => { /* Keep the navigable summary and normal image fallback offline. */ });
    return () => controller.abort();
  }, [game]);
  return (loaded?.input === game ? loaded.game : game) as T;
}
