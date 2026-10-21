// Logical coordinates from feature/game-information's SDL layout.
export const TV_LAYOUT = {
  width: 1920, height: 1080, rail: 92, contentX: 130,
  cardsY: 576, cardWidth: 216, cardHeight: 300, cardGap: 16,
  cardTitle: 28, cardFooter: 40
} as const;

export function fitViewport(width: number, height: number) {
  const scale = Math.max(0, Math.min(width / TV_LAYOUT.width, height / TV_LAYOUT.height));
  return { scale, left: (width - TV_LAYOUT.width * scale) / 2,
    top: (height - TV_LAYOUT.height * scale) / 2 };
}

/** Keep TV controls at their native aspect ratio while allowing the stage to
 *  extend horizontally on ultrawide browser screens. Unlike letterboxing,
 *  this keeps the sidebar at x=0 and lets the artwork reach the right edge. */
export function fitEdgeToEdgeViewport(width: number, height: number) {
  const fit = fitViewport(width, height);
  const stageWidth = Math.max(TV_LAYOUT.width,
    fit.scale > 0 ? width / fit.scale : TV_LAYOUT.width);
  return { scale: fit.scale, left: 0, top: fit.top, stageWidth };
}

export function platformLabel(id: string) {
  if (/^PPSA/i.test(id)) return 'PS5';
  if (/^CUSA/i.test(id)) return 'PS4';
  return '';
}

// Mirrored by GAME_LIST_* in ui_game_list.h (logical 1920 x 1080).
export const GAME_LIST = { top: 220, rowHeight: 64, rowGap: 8, pageSize: 10, columnWeights: [17, 39, 32, 12] } as const;

// Public Home navigation order. Native HOME_TABS mirrors these two categories.
export const HOME_TABS = [
  { id: 'newest', titleKey: 'TXT_CAT_NEWEST' },
  { id: 'popular', titleKey: 'TXT_CAT_POPULAR' }
] as const;
