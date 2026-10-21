import type { ClipboardItem, EntryHandle, FolderHandle } from './fileManager';

interface BridgeEntry { name: string; kind: 'directory' | 'file'; size: number }
const ENDPOINT = '/api/wolodrive/files';
const metadata = new WeakMap<object, { session: ConsoleFileBridge; path: string; kind: BridgeEntry['kind'] }>();
function childPath(parent: string, name: string) {
  if (!name || name === '.' || name === '..' || /[\/\\\x00]/.test(name)) throw new Error('Unsupported filename.');
  return parent ? `${parent}/${name}` : name;
}

export class ConsoleFileBridge {
  constructor(private token: string) {}
  private async request(query: string, fields?: Record<string, string>) {
    const response = await fetch(ENDPOINT + query, {
      method: fields ? 'POST' : 'GET', credentials: 'omit', cache: 'no-store', redirect: 'error',
      headers: { 'X-Obsidian-Bridge': '1', 'X-Obsidian-Token': this.token,
        ...(fields ? { 'Content-Type': 'application/x-www-form-urlencoded' } : {}) },
      body: fields ? new URLSearchParams(fields).toString() : undefined,
    });
    const result = await response.json();
    if (!response.ok) throw new Error(typeof result.error === 'string' ? result.error : 'Console file operation failed.');
    return result;
  }
  async list(path: string): Promise<BridgeEntry[]> {
    const result = await this.request('?path=' + encodeURIComponent(path));
    if (!Array.isArray(result.entries) || result.entries.length > 4096) throw new Error('Invalid console folder listing.');
    for (const item of result.entries) {
      if (!item || typeof item.name !== 'string' || !['directory', 'file'].includes(item.kind) ||
          typeof item.size !== 'number') throw new Error('Invalid console file entry.');
      childPath(path, item.name);
    }
    return result.entries;
  }
  async operation(action: string, source: string, destination = '') {
    const result = await this.request('', { action, source, destination });
    if (result.ok !== true) throw new Error('Console did not confirm the file operation.');
  }
  async readFile(handle: EntryHandle, head = false) {
    const info = metadata.get(handle);
    if (info?.session !== this || info.kind !== 'file') throw new Error('Select a console file.');
    const response = await fetch(ENDPOINT + '/content?path=' + encodeURIComponent(info.path), {
      method: head ? 'HEAD' : 'GET', credentials: 'omit', cache: 'no-store', redirect: 'error',
      headers: {'X-Obsidian-Bridge':'1','X-Obsidian-Token':this.token},
    });
    if (!response.ok) {
      const result = head ? null : await response.json();
      throw new Error(result?.error || 'Cannot read this console file (8 MiB limit).');
    }
    const revision = response.headers.get('X-Obsidian-Revision');
    if (!revision || !/^[a-f0-9-]+$/.test(revision)) throw new Error('Missing file revision. Reload Obsidian.');
    return {bytes: head ? new ArrayBuffer(0) : await response.arrayBuffer(), revision};
  }
  async writeFile(handle: EntryHandle, bytes: Uint8Array, revision: string) {
    const info = metadata.get(handle);
    if (info?.session !== this || info.kind !== 'file') throw new Error('Select a console file.');
    if (bytes.byteLength > 8 * 1024 * 1024) throw new Error('File transfer limit is 8 MiB.');
    const response = await fetch(ENDPOINT + '/content?path=' + encodeURIComponent(info.path), {
      method:'POST', credentials:'omit',cache:'no-store',redirect:'error',
      headers:{'X-Obsidian-Bridge':'1','X-Obsidian-Token':this.token,
        'X-Obsidian-Revision':revision,'Content-Type':'application/octet-stream'},
      body:new Blob([bytes as Uint8Array<ArrayBuffer>]),
    });
    const result = await response.json();
    if (!response.ok || result.ok !== true) throw new Error(result.error || 'Console did not confirm the save.');
  }
  childFile(folder: FolderHandle, name: string) {
    const info=metadata.get(folder);
    if (info?.session!==this || info.kind!=='directory') throw new Error('Select a console folder.');
    return this.handle(childPath(info.path,name),'file');
  }
  handle(path: string, kind: BridgeEntry['kind'] = 'directory'): EntryHandle {
    const session = this;
    const handle = {
      kind, name: path.split('/').pop() || '/',
      async isSameEntry(other: EntryHandle) {
        const info = metadata.get(other);
        return info?.session === session && info.path === path && info.kind === kind;
      },
      async *values() {
        for (const item of await session.list(path)) yield session.handle(childPath(path, item.name), item.kind);
      },
      async getDirectoryHandle(name: string) { return getChild(name, 'directory'); },
      async getFileHandle(name: string) { return getChild(name, 'file'); },
      async removeEntry(name: string) { await session.operation('delete', childPath(path, name)); },
      async resolve(other: EntryHandle) {
        const info = metadata.get(other);
        if (info?.session !== session) return null;
        if (info.path === path) return [];
        const prefix = path ? path + '/' : '';
        return info.path.startsWith(prefix) ? info.path.slice(prefix.length).split('/') : null;
      },
    };
    async function getChild(name: string, expected: BridgeEntry['kind']) {
      const child = childPath(path, name);
      const entry = (await session.list(path)).find(item => item.name === name);
      if (!entry) throw new DOMException('Entry no longer exists. Refresh the folder.', 'NotFoundError');
      if (entry.kind !== expected) throw new DOMException('Entry type changed. Refresh the folder.', 'TypeMismatchError');
      return session.handle(child, expected);
    }
    // These are console references, not browser File System Access handles.
    // Byte operations are routed below to the native backend, never emulated.
    metadata.set(handle, { session, path, kind });
    return handle as unknown as EntryHandle;
  }
  initialTrail(): FolderHandle[] { return [this.handle(''), this.handle('data')] as FolderHandle[]; }
}

export function consoleFilePath(handle: EntryHandle | undefined) {
  const info = handle && metadata.get(handle);
  return info ? '/' + info.path : undefined;
}
export async function pasteConsoleEntry(clip: ClipboardItem, destination: FolderHandle): Promise<boolean> {
  const source = metadata.get(clip.handle), target = metadata.get(destination);
  if (!source && !target) return false;
  if (!source || !target || source.session !== target.session) throw new Error('Cannot mix console and browser folders.');
  await source.session.operation(clip.move ? 'move' : 'copy', source.path, childPath(target.path, clip.handle.name));
  return true;
}
export async function extractConsoleZip(handle: FileSystemFileHandle): Promise<boolean> {
  const info = metadata.get(handle);
  if (!info) return false;
  await info.session.operation('unzip', info.path);
  return true;
}
export function expectsConsoleBridge() {
  return typeof window !== 'undefined' && window.location.hostname === '127.0.0.1' &&
    ['#native-unavailable', '#ps5-files'].includes(window.location.hash);
}
export async function connectConsoleFiles(): Promise<ConsoleFileBridge | null> {
  if (!expectsConsoleBridge()) {
    if (typeof window === 'undefined' ||
        !/^(10\.|192\.168\.|172\.(1[6-9]|2\d|3[01])\.|169\.254\.|127\.(?!0\.0\.1$))/.test(window.location.hostname)) return null;
    const probe = await fetch('/', {method: 'HEAD', credentials: 'omit', cache: 'no-store', redirect: 'error'});
    if (probe.headers.get('X-Obsidian-Files') !== '3') return null;
  }
  const response = await fetch(ENDPOINT + '/session', {
    method: 'GET', credentials: 'omit', cache: 'no-store', redirect: 'error',
    headers: {'X-Obsidian-Bridge': '1'},
  });
  const result = await response.json();
  if (!response.ok) throw new Error(result.error || 'Console file bridge is unavailable. Relaunch the updated payload.');
  if (result.version !== 3 || typeof result.token !== 'string' || !/^[a-f0-9]{64}$/.test(result.token))
    throw new Error('Unsupported console file bridge. Relaunch the updated payload.');
  return new ConsoleFileBridge(result.token);
}
