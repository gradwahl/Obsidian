import React, { useEffect, useRef, useState } from 'react';
import '../files.css';
import { FittedText } from './FittedText';
import { LanguageCode } from '../types';
import { t } from '../translations';
import { ClipboardItem, EntryHandle, FILE_ACTIONS, FolderHandle, directoryPicker, extractZip, listFolder, pasteEntry } from '../fileManager';
import { ConsoleFileBridge, connectConsoleFiles, consoleFilePath, expectsConsoleBridge } from '../fileBridge';

export function FileBrowser({ language, sidebarFocused, onSidebar }: {
  language: LanguageCode; sidebarFocused: boolean; onSidebar: () => void;
}) {
  const [trail, setTrail] = useState<FolderHandle[]>([]);
  const [entries, setEntries] = useState<EntryHandle[]>([]);
  const [selected, setSelected] = useState(0);
  const [action, setAction] = useState(0);
  const [inToolbar, setInToolbar] = useState(true);
  const [clipboard, setClipboard] = useState<ClipboardItem | null>(null);
  const [busy, setBusy] = useState(false);
  const busyRef = useRef(false);
  const [status, setStatus] = useState('');
  const [deleting, setDeleting] = useState<EntryHandle | null>(null);
  const [confirm, setConfirm] = useState(false);
  const [bridge, setBridge] = useState<ConsoleFileBridge | null>(null);
  const [checkingBridge, setCheckingBridge] = useState(true);
  const [editor, setEditor] = useState<{ handle: EntryHandle; text: string; original: string; revision: string; newline: string; bom: boolean } | null>(null);
  const [upload, setUpload] = useState<{handle: EntryHandle; bytes: Uint8Array; revision: string} | null>(null);
  const fileInput = useRef<HTMLInputElement>(null);
  const uploadTarget = useRef<{folder: FolderHandle; replace?: EntryHandle} | null>(null);
  const consoleMode = expectsConsoleBridge();
  const folder = trail.at(-1), entry = entries[selected];
  const page = Math.floor(selected / 8) * 8;
  const label = (key: string) => t(language, 'TXT_FILES_' + key);
  useEffect(() => {
    let active = true;
    void connectConsoleFiles().then(session => {
      if (active) setBridge(session);
    }).catch(error => {
      if (!active) return;
      setStatus(String(error instanceof Error ? error.message : error));
    }).finally(() => { if (active) setCheckingBridge(false); });
    return () => { active = false; };
  }, []);
  const refresh = async (next = folder) => {
    if (!next) return;
    // Never leave stale entries actionable after a failed refresh/permission loss.
    setEntries([]); setSelected(0);
    setEntries(await listFolder(next));
  };
  const run = async (operation: () => Promise<void>) => {
    if (busyRef.current) return;
    busyRef.current = true; setBusy(true); setStatus('');
    try { await operation(); setStatus(label('DONE')); }
    catch (error) {
      if (!(error instanceof DOMException && error.name === 'AbortError'))
        setStatus(label('ERROR') + ': ' + (error instanceof Error ? error.message : String(error)));
    } finally { busyRef.current = false; setBusy(false); }
  };
  const up = () => {
    if (trail.length <= 1) { onSidebar(); return; }
    void run(async () => { const next = trail.slice(0, -1); setTrail(next); await refresh(next.at(-1)); });
  };
  const canOpen = !checkingBridge && (!!bridge || (!consoleMode && !!directoryPicker()));
  const enabled = [canOpen, trail.length > 1, !!folder, !!entry, !!entry,
    !!folder && !!clipboard, !!entry, entry?.kind === 'file' && /\.zip$/i.test(entry.name)];
  const activate = (index: number) => {
    if (busyRef.current || !enabled[index]) return;
    setAction(index);
    if (index === 6 && entry) { setDeleting(entry); setConfirm(false); return; }
    void run(async () => {
      if (index === 0) {
        if (bridge) {
          const next = bridge.initialTrail(); setTrail(next); await refresh(next.at(-1)); return;
        }
        const picker = directoryPicker(); if (!picker) return;
        const root = await picker.call(window, { mode: 'readwrite' });
        setTrail([root]); await refresh(root);
      } else if (index === 1) {
        const next = trail.slice(0, -1); setTrail(next); await refresh(next.at(-1));
      } else if (index === 2) await refresh();
      else if ((index === 3 || index === 4) && folder && entry) setClipboard({ parent: folder, handle: entry, move: index === 4 });
      else if (index === 5 && clipboard && folder) {
        await pasteEntry(clipboard, folder); if (clipboard.move) setClipboard(null); await refresh();
      } else if (index === 7 && folder && entry?.kind === 'file') {
        await extractZip(folder, entry); await refresh();
      }
    });
  };
  const deleteSelected = () => {
    const target = deleting; setDeleting(null);
    if (!target || !folder) return;
    void run(async () => {
      const current = target.kind === 'directory'
        ? await folder.getDirectoryHandle(target.name) : await folder.getFileHandle(target.name);
      if (!await current.isSameEntry(target)) throw new Error('The selected entry changed. Refresh and try again.');
      await folder.removeEntry(target.name, { recursive: true });
      if (clipboard && await clipboard.handle.isSameEntry(target)) setClipboard(null);
      await refresh();
    });
  };
  const openEntry = (index: number) => {
    setSelected(index); setInToolbar(false);
    const item = entries[index];
    if (item?.kind === 'directory') void run(async () => {
      const next = [...trail, item as FolderHandle]; setTrail(next); await refresh(item as FolderHandle);
    });
  };
  const remote = !!bridge && !consoleMode;
  const modal = !!deleting || !!editor || !!upload;
  const saveEditor = () => {
    if (!editor || !bridge) return;
    const target = editor;
    void run(async () => {
      const originalDisplay=target.original.replace(/\r\n/g,'\n');
      const text=target.text===originalDisplay ? target.original : target.text.replace(/\r\n|\r|\n/g,target.newline);
      const bytes=new TextEncoder().encode((target.bom ? '\ufeff' : '')+text);
      if (bytes.length>1024*1024) throw new Error(label('LIMITS'));
      await bridge.writeFile(target.handle,bytes,target.revision);
      setEditor(null); await refresh();
    });
  };
  const editFile = () => {
    if (!bridge || entry?.kind!=='file') return;
    const target=entry;
    void run(async () => {
      const {bytes,revision}=await bridge.readFile(target);
      if (bytes.byteLength>1024*1024) throw new Error(label('LIMITS'));
      let text: string;
      try {
        text=new TextDecoder('utf-8',{fatal:true,ignoreBOM:true}).decode(bytes);
        if (/[\x00-\x08\x0b\x0c\x0e-\x1f]/.test(text)) throw new Error();
      } catch { throw new Error(label('TEXT_ONLY')); }
      const bom=text.startsWith('\ufeff');
      if (bom) text=text.slice(1);
      const newline=text.includes('\r\n') && !text.replace(/\r\n/g,'').includes('\n') ? '\r\n' : '\n';
      setEditor({handle:target,text:text.replace(/\r\n/g,'\n'),original:text,revision,newline,bom});
    });
  };
  const downloadFile = () => {
    if (!bridge || entry?.kind!=='file') return;
    const target=entry;
    void run(async () => {
      const {bytes}=await bridge.readFile(target);
      const url=URL.createObjectURL(new Blob([bytes]));
      const anchor=document.createElement('a'); anchor.href=url; anchor.download=target.name;
      document.body.appendChild(anchor); anchor.click(); anchor.remove();
      window.setTimeout(() => URL.revokeObjectURL(url),60000);
    });
  };
  const chooseUpload = (replace: boolean) => {
    if (!folder || busyRef.current || (replace && entry?.kind!=='file')) return;
    uploadTarget.current={folder,replace:replace ? entry : undefined}; fileInput.current?.click();
  };
  const uploadFile = (file: File | undefined) => {
    const target=uploadTarget.current;
    if (!file || !target || !bridge) return;
    void run(async () => {
      if (file.size>8*1024*1024) throw new Error(label('LIMITS'));
      const bytes=new Uint8Array(await file.arrayBuffer());
      if (target.replace) {
        const {revision}=await bridge.readFile(target.replace,true);
        setUpload({handle:target.replace,bytes,revision});
      } else {
        await bridge.writeFile(bridge.childFile(target.folder,file.name),bytes,'new'); await refresh(target.folder);
      }
    });
  };
  useEffect(() => {
    const keydown = (event: KeyboardEvent) => {
      if (sidebarFocused) return;
      if (editor || upload) {
        if (event.key==='Escape' && !busyRef.current) {
          event.preventDefault(); event.stopImmediatePropagation(); setEditor(null); setUpload(null);
        } else if (editor && (event.ctrlKey || event.metaKey) && event.key.toLowerCase()==='s') {
          event.preventDefault(); event.stopImmediatePropagation(); saveEditor();
        }
        return;
      }
      if (event.target instanceof HTMLElement && event.target.closest('[data-files-controls]')) return;
      const keys = ['ArrowUp', 'ArrowDown', 'ArrowLeft', 'ArrowRight', 'Enter', ' ', 'Escape', 'Backspace', 'r', 'R', ...(deleting ? ['Tab', 'l', 'L'] : [])];
      if (!keys.includes(event.key)) return;
      event.preventDefault(); event.stopImmediatePropagation();
      if (busyRef.current) return;
      if (deleting) {
        if (['Escape', 'Backspace'].includes(event.key)) setDeleting(null);
        else if (['ArrowLeft', 'ArrowRight', 'ArrowUp', 'ArrowDown', 'Tab'].includes(event.key)) {
          setConfirm(value => !value);
          const buttons = document.querySelectorAll<HTMLButtonElement>('.tv-files-dialog button');
          buttons[confirm ? 0 : 1]?.focus();
        }
        else if (['Enter', ' '].includes(event.key)) { if (confirm) deleteSelected(); else setDeleting(null); }
        return;
      }
      if (event.key === 'Escape' || event.key === 'Backspace') { up(); return; }
      if (event.key.toLowerCase() === 'r') { activate(2); return; }
      if (event.key === 'Enter' || event.key === ' ') { if (inToolbar) activate(action); else openEntry(selected); return; }
      if (inToolbar) {
        if (event.key === 'ArrowLeft') { if (action === 0) onSidebar(); else setAction(action - 1); }
        if (event.key === 'ArrowRight') setAction(Math.min(7, action + 1));
        if (event.key === 'ArrowDown' && entries.length) setInToolbar(false);
      } else {
        if (event.key === 'ArrowLeft') onSidebar();
        if (event.key === 'ArrowRight' || (event.key === 'ArrowUp' && selected === 0)) setInToolbar(true);
        if (event.key === 'ArrowUp') setSelected(Math.max(0, selected - 1));
        if (event.key === 'ArrowDown') setSelected(Math.min(entries.length - 1, selected + 1));
      }
    };
    window.addEventListener('keydown', keydown, true);
    return () => window.removeEventListener('keydown', keydown, true);
  });
  const focused = !sidebarFocused;
  return <section className="tv-files" aria-label={t(language, 'TXT_NAV_FILES')} aria-busy={busy}>
    <h1>{t(language, 'TXT_NAV_FILES')}</h1>
    <p className="tv-files-description">{label('DESCRIPTION')}</p>
    <p className="tv-files-path" title={consoleFilePath(folder) ?? trail.map(handle => handle.name).join('/')}>
      {consoleFilePath(folder) ?? (folder ? trail.map(handle => handle.name).join(' / ') : label('CHOOSE'))}</p>
    <div className="tv-files-toolbar" inert={modal ? true : undefined}>
      {FILE_ACTIONS.map((name, i) => <button key={name} type="button" disabled={busy || !enabled[i]}
        className={focused && inToolbar && action === i ? 'tv-files-focus' : ''}
        onFocus={() => { setInToolbar(true); setAction(i); }} onClick={() => activate(i)}><FittedText>{label(name)}</FittedText></button>)}
    </div>
    <div className="tv-files-list" inert={modal ? true : undefined}>
      {entries.slice(page, page + 8).map((item, i) => <button type="button" key={item.name} disabled={busy}
        className={selected === page + i ? (focused && !inToolbar ? 'tv-files-focus tv-files-selected' : 'tv-files-selected') : ''}
        onFocus={() => { setSelected(page + i); setInToolbar(false); }}
        onClick={() => { setSelected(page + i); setInToolbar(false); }} onDoubleClick={() => openEntry(page + i)}>
        <span>{item.kind === 'directory' ? '[DIR]' : '[FILE]'}</span><strong>{item.name}</strong>
        {item.kind === 'directory' && <span>→</span>}
      </button>)}
      {!entries.length && <p>{!canOpen ? label('UNAVAILABLE') : folder ? label('EMPTY') : label('CHOOSE')}</p>}
    </div>
    <div className="tv-files-footer">
      <span>{entries.length ? `${selected + 1} / ${entries.length}` : '0 / 0'}</span>
      <button type="button" disabled={busy || page === 0 || modal} onClick={() => { setSelected(Math.max(0, page - 8)); setInToolbar(false); }}>←</button>
      <button type="button" disabled={busy || page + 8 >= entries.length || modal} onClick={() => { setSelected(page + 8); setInToolbar(false); }}>→</button>
      <span>{clipboard ? `${label(clipboard.move ? 'MOVE' : 'COPY')}: ${clipboard.handle.name}` : ''}</span>
    </div>
    <p className="tv-files-status" role="status">{busy ? label('BUSY') : status}</p>
    <p className="tv-files-hint">{label('HINT')}</p>
    {remote && <div className="tv-files-lan" data-files-controls inert={modal ? true : undefined} onKeyDown={event => event.stopPropagation()}>
      {remote && <>
        <button disabled={busy || entry?.kind!=='file'} onClick={downloadFile}>{label('DOWNLOAD')}</button>
        <button disabled={busy || entry?.kind!=='file'} onClick={editFile}>{label('EDIT')}</button>
        <button disabled={busy || !folder} onClick={() => chooseUpload(false)}>{label('UPLOAD')}</button>
        <button disabled={busy || entry?.kind!=='file'} onClick={() => chooseUpload(true)}>{label('REPLACE')}</button>
        <span>{label('LIMITS')}</span>
        <input ref={fileInput} type="file" hidden onChange={event => { uploadFile(event.target.files?.[0]); event.target.value=''; }} />
      </>}
    </div>}
    {editor && <div className="tv-files-shade" onKeyDown={event => event.stopPropagation()}><div className="tv-files-editor" role="dialog" aria-modal="true" aria-labelledby="files-edit-title">
      <h2 id="files-edit-title">{label('EDIT')}: {consoleFilePath(editor.handle)}</h2>
      <textarea aria-label={label('CONTENTS')} autoFocus spellCheck={false} disabled={busy} value={editor.text} onChange={event => setEditor({...editor,text:event.target.value})} />
      <div role="status">{busy ? label('BUSY') : status}</div>
      <button disabled={busy} onClick={() => setEditor(null)}>{label('CANCEL')}</button>
      <button disabled={busy} onClick={saveEditor}>{label('SAVE')}</button>
    </div></div>}
    {upload && <div className="tv-files-shade" onKeyDown={event => event.stopPropagation()}><div className="tv-files-dialog" role="dialog" aria-modal="true" aria-labelledby="files-replace-title">
      <h2 id="files-replace-title">{label('REPLACE_CONFIRM')}</h2><p>{consoleFilePath(upload.handle)}</p>
      <div role="status">{busy ? label('BUSY') : status===label('DONE') ? '' : status}</div>
      <button disabled={busy} autoFocus onClick={() => setUpload(null)}>{label('CANCEL')}</button>
      <button disabled={busy} onClick={() => { if (bridge) void run(async () => {
        await bridge.writeFile(upload.handle,upload.bytes,upload.revision); setUpload(null); await refresh();
      }); }}>{label('REPLACE')}</button>
    </div></div>}
    {deleting && <div className="tv-files-shade"><div className="tv-files-dialog" role="dialog" aria-modal="true" aria-labelledby="files-delete-title">
      <h2 id="files-delete-title"><FittedText>{label('CONFIRM')}</FittedText></h2><p>{deleting.name}</p>
      <button type="button" className={!confirm ? 'tv-files-focus' : ''} autoFocus onFocus={() => setConfirm(false)} onClick={() => setDeleting(null)}>{label('CANCEL')}</button>
      <button type="button" className={confirm ? 'tv-files-focus' : ''} onFocus={() => setConfirm(true)} onClick={deleteSelected}>{label('DELETE')}</button>
    </div></div>}
  </section>;
}
