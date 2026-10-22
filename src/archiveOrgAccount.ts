/** Archive.org authentication is first-party: Obsidian never handles credentials. */
import additionalSources from '../config/additional_sources.json';

export const ARCHIVE_ORG_LOGIN = 'https://archive.org/account/login.php';

const identifierPattern = /^[a-zA-Z0-9][a-zA-Z0-9_.-]{4,99}$/;
const alvro = additionalSources.find(source => source.id === 'alvro');
const identifiers: string[] = alvro && 'archive_identifiers' in alvro
  ? alvro.archive_identifiers as string[] : [];

/** These are directory listings; never URLs to hosted package files. */
export const ALVRO_COLLECTIONS = Object.freeze(identifiers
  .filter(identifier => identifierPattern.test(identifier))
  .map(identifier => Object.freeze({
    identifier,
    label: identifier.replace(/^httpsarchive\.orgdetails/, '').replace(/_/g, ' '),
    url: 'https://archive.org/download/' + encodeURIComponent(identifier)
  })));

const knownIds = new Set(ALVRO_COLLECTIONS.map(item => item.identifier));

/** Return a known collection's directory, never construct a URL from untrusted metadata. */
export function alvroDirectoryForGame(sourceName: string, sourceUrl?: string): string | null {
  if (sourceName !== 'Archive.org / AlvRo' || !sourceUrl) return null;
  const match = /^https:\/\/archive\.org\/details\/([a-zA-Z0-9_.-]+)\/?$/.exec(sourceUrl);
  if (!match || !knownIds.has(match[1])) return null;
  return 'https://archive.org/download/' + encodeURIComponent(match[1]);
}
