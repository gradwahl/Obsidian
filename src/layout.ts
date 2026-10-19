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

export function platformLabel(id: string) {
  if (/^PPSA/i.test(id)) return 'PS5';
  if (/^CUSA/i.test(id)) return 'PS4';
  return '';
}
