import React from 'react';
import type { LibraryConsole } from '../types';
import data from '../emulatorReleases.json';

export interface EmulatorAsset {
  name: string;
  url: string;
  size: number;
  sha256: string;
}
export interface EmulatorRelease {
  release: string;
  releaseUrl: string;
  asset: EmulatorAsset;
  engine?: 'retroarch';
}

const releases = data as {
  repository: string;
  releasesUrl: string;
  platforms: Record<string, EmulatorRelease>;
  systems: Record<string, EmulatorRelease>;
  releases: Array<{ tag: string; name: string; url: string; assets: EmulatorAsset[] }>;
};
export const EMULATOR_RELEASES_URL = releases.releasesUrl;

export function getEmulatorRelease(console: LibraryConsole): EmulatorRelease | null {
  return (console.platform_id ? releases.platforms[console.platform_id] : null)
    ?? releases.systems[console.name] ?? null;
}

/** Explicit standalone or user-designated RetroArch release assets.
 * A RetroArch association does not certify core availability on PS5. */
export const EmulatorReleaseView: React.FC<{
  system: LibraryConsole;
  onBack: () => void;
}> = ({ system, onBack }) => {
  const match = getEmulatorRelease(system);
  const bytes = match ? (match.asset.size / (1024 * 1024)).toFixed(1) + ' MB' : '';
  const additionalAssets = match ? (releases.releases.find(release => release.tag === match.release)
    ?.assets.filter(asset => asset.url !== match.asset.url) ?? []) : [];
  return <section className="tv-emulator-release-view" data-layout="emulator-release">
    <button type="button" className="tv-emulator-back" onClick={onBack}>← Back</button>
    <div className="tv-emulator-release-main">
      {system.artwork_path && <img className="tv-emulator-art" src={system.artwork_path}
        alt="" loading="lazy" decoding="async" />}
      <div className="tv-emulator-release-content">
        <p className="tv-emulator-kicker">PolDrive Emulators · GitHub Releases</p>
        <h1>{system.name}</h1>
        {match ? <>
          <p>{match.engine === 'retroarch' ? 'RetroArch release' : 'Dedicated emulator release'}:
            {' '}<strong>{match.release}</strong></p>
          <p>{match.asset.name} · {bytes}</p>
          {match.engine === 'retroarch' && <p className="tv-emulator-core-note">
            Shared RetroArch package. Core availability and gameplay compatibility
            for this system have not been verified on PS5.
          </p>}
          <div className="tv-emulator-actions">
            <a href={match.asset.url} target="_blank" rel="noopener noreferrer"
              className="tv-emulator-action tv-emulator-action-primary">
              ↓ Download release asset
            </a>
          </div>
          <p className="tv-emulator-hash">SHA-256: <code>{match.asset.sha256}</code></p>
          {additionalAssets.length > 0 && <div className="tv-emulator-additional">
            <p>Other files in this release (optional components):</p>
            {additionalAssets.map(asset => <a key={asset.url} href={asset.url}
              target="_blank" rel="noopener noreferrer">{asset.name} ↗</a>)}
          </div>}
        </> : <>
          <p>No verified emulator download is linked to this system yet.</p>
        </>}
        <p className="tv-emulator-note">
          Downloads are provided by GitHub. ZIP/ELF files are not automatically
          installed or launched. PS5 compatibility depends on the actual build and firmware.
        </p>
      </div>
    </div>
  </section>;
};
