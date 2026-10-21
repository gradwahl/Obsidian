// Browser integration fixture only. Uses a private, disposable OPFS directory;
// it never accesses a user-selected folder or ships in static-preview.
import React, { useState } from 'react';
import { createRoot } from 'react-dom/client';
import { zipSync, strToU8 } from 'fflate';
import { FileBrowser } from '../src/components/FileBrowser';
import { NativeViewport } from '../src/components/NativeViewport';
import { FolderHandle } from '../src/fileManager';
import '../src/native.css';
function Fixture() {
  const [ready, setReady] = useState(false);
  const [sidebar, setSidebar] = useState(false);
  const prepare = async () => {
    const storage = await navigator.storage.getDirectory() as FolderHandle;
    try { await storage.removeEntry('wolodrive-files-test-only', { recursive: true }); } catch {}
    const folder = await storage.getDirectoryHandle('wolodrive-files-test-only', { create: true });
    await folder.getDirectoryHandle('Destination', { create: true });
    await folder.getDirectoryHandle('Folder', { create: true });
    for (const [name, content] of Object.entries({ 'hello.txt': strToU8('hello world'),
      'archive.zip': zipSync({ 'nested/data.txt': strToU8('extracted data') }) })) {
      const handle = await folder.getFileHandle(name, { create: true });
      const writer = await handle.createWritable();
      await writer.write(new Blob([content])); await writer.close();
    }
    Object.assign(window, { showDirectoryPicker: async () => folder }); setReady(true);
  };
  return <NativeViewport><div className="tv-stage-fixture" style={{ position: 'absolute', inset: 0 }}>
    {!ready ? <button onClick={prepare}>Prepare disposable file test</button> :
      <main className="tv-main"><FileBrowser language="en" sidebarFocused={sidebar} onSidebar={() => setSidebar(true)} /></main>}
    {sidebar && <button style={{ position:'absolute', left:0, top:0 }} onClick={() => setSidebar(false)}>Return to files</button>}
  </div></NativeViewport>;
}
createRoot(document.getElementById('root')!).render(<Fixture />);
