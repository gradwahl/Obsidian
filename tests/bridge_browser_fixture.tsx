// Disposable native HTTP bridge integration fixture; not part of production.
import React from 'react';
import { createRoot } from 'react-dom/client';
import { FileBrowser } from '../src/components/FileBrowser';
import { NativeViewport } from '../src/components/NativeViewport';
import '../src/native.css';
createRoot(document.getElementById('root')!).render(<NativeViewport>
  <main className="tv-main"><FileBrowser language="en" sidebarFocused={false} onSidebar={() => {}} /></main>
</NativeViewport>);
