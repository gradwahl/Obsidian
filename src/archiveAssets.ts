/** Public image origin. No credentials or binary image files ship with Obsidian. */
export const ARCHIVE_ASSET_BASE =
  'https://raw.githubusercontent.com/gradwahl/Obsidian-archive/main/';

/** Only public company, console and emulator logo assets from the archive. */
export function archiveLibraryUrl(path: string | null | undefined): string | null {
  if (!path) return null;
  const normalized = path.replace(/^\/+/, '');
  if (!/^assets\/library\/(companies|consoles|logos)\/[a-z0-9-]+\.png$/.test(normalized)) {
    return null;
  }
  return ARCHIVE_ASSET_BASE + normalized;
}
