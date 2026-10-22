import React from 'react';
import { ALVRO_COLLECTIONS, ARCHIVE_ORG_LOGIN } from '../archiveOrgAccount';

/**
 * Browser-first access through archive.org itself. An external first-party tab
 * holds the user's session; this static UI neither receives nor stores cookies.
 */
export function ArchiveOrgAccount() {
  return (
    <section aria-label="Archive.org account access"
      className="rounded-[18px] border border-white/10 bg-[#14141c]/85 p-4 text-white">
      <h3 className="text-sm font-bold">Archive.org account</h3>
      <p className="mt-2 text-xs leading-relaxed text-[#aaaabb]">
        Sign in on Archive.org, then open an AlvRo collection in the same browser.
        Some files are visible to signed-in visitors but restricted anonymously.
        This app cannot verify your login or guarantee access to a particular file.
      </p>
      <a href={ARCHIVE_ORG_LOGIN} target="_blank" rel="noopener noreferrer"
        className="mt-3 inline-flex rounded-lg border border-white/20 bg-[#252535] px-3 py-2 text-xs font-bold text-white hover:bg-[#35354b]">
        Sign in at Archive.org ↗
      </a>
      <details className="mt-3 text-xs">
        <summary className="cursor-pointer text-[#c8c8dd]">
          Browse AlvRo's {ALVRO_COLLECTIONS.length} collections
        </summary>
        <ul className="mt-2 space-y-2">
          {ALVRO_COLLECTIONS.map(({ identifier, url }) => (
            <li key={identifier}>
              <a href={url} target="_blank" rel="noopener noreferrer"
                className="break-all text-[#a5b4fc] hover:text-white">
                {identifier} ↗
              </a>
            </li>
          ))}
        </ul>
      </details>
      <p className="mt-2 text-[11px] leading-relaxed text-[#9292a7]">
        Credentials stay with Archive.org, not Obsidian. Cookies are not copied
        between browsers or devices; PS5 embedded-browser support is unverified.
      </p>
    </section>
  );
}
