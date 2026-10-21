import { Unzip, UnzipInflate } from 'fflate';
import { extractConsoleZip, pasteConsoleEntry } from './fileBridge';

// Deliberately optional: static preview needs no server or filesystem bridge.
export interface FolderHandle extends FileSystemDirectoryHandle {
  values(): AsyncIterableIterator<FolderHandle | FileSystemFileHandle>;
  getDirectoryHandle(name: string, options?: FileSystemGetDirectoryOptions): Promise<FolderHandle>;
}
export type EntryHandle = FolderHandle | FileSystemFileHandle;
export interface ClipboardItem { parent: FolderHandle; handle: EntryHandle; move: boolean }
export const FILE_ACTIONS = ['OPEN', 'UP', 'REFRESH', 'COPY', 'MOVE', 'PASTE', 'DELETE', 'UNZIP'] as const;
export const ZIP_INPUT_LIMIT = 128 * 1024 * 1024;
export const ZIP_OUTPUT_LIMIT = 512 * 1024 * 1024;
export const FILE_LIMIT = 4096;
export function directoryPicker() {
  if (typeof window === 'undefined') return undefined;
  return (window as unknown as { showDirectoryPicker?: (options: { mode: 'readwrite' }) => Promise<FolderHandle> }).showDirectoryPicker;
}
export async function listFolder(folder: FolderHandle): Promise<EntryHandle[]> {
  const entries: EntryHandle[] = [];
  for await (const handle of folder.values()) {
    if (entries.length === FILE_LIMIT) throw new Error('Folder exceeds 4096 entries.');
    entries.push(handle);
  }
  // Match native bytewise UTF-8 order, directories first.
  const encoder = new TextEncoder();
  return entries.sort((a, b) => {
    if (a.kind !== b.kind) return a.kind === 'directory' ? -1 : 1;
    const x = encoder.encode(a.name), y = encoder.encode(b.name);
    for (let i = 0; i < Math.min(x.length, y.length); i++) if (x[i] !== y[i]) return x[i] - y[i];
    return x.length - y.length;
  });
}
async function requireAbsent(parent: FolderHandle, name: string) {
  try { await parent.getFileHandle(name); }
  catch (error) {
    if (error instanceof DOMException && error.name === 'NotFoundError') return;
    if (!(error instanceof DOMException) || error.name !== 'TypeMismatchError') throw error;
  }
  throw new Error('Destination already exists. Nothing was overwritten.');
}
async function copyEntry(handle: EntryHandle, parent: FolderHandle, depth = 0): Promise<void> {
  if (depth > 64) throw new Error('Folder nesting exceeds 64 levels.');
  await requireAbsent(parent, handle.name);
  let created = false;
  try {
    if (handle.kind === 'directory') {
      const output = await parent.getDirectoryHandle(handle.name, { create: true }); created = true;
      for await (const entry of (handle as FolderHandle).values()) await copyEntry(entry, output, depth + 1);
    } else {
      const output = await parent.getFileHandle(handle.name, { create: true }); created = true;
      const writer = await output.createWritable();
      try { await (await handle.getFile()).stream().pipeTo(writer); }
      catch (error) { try { await writer.abort(); } catch {} throw error; }
    }
  } catch (error) {
    if (created) {
      try { await parent.removeEntry(handle.name, { recursive: true }); }
      catch { throw new Error('Copy failed; an incomplete destination could not be removed.'); }
    }
    throw error;
  }
}
export async function pasteEntry(clip: ClipboardItem, destination: FolderHandle) {
  if (await pasteConsoleEntry(clip, destination)) return;
  if (await clip.parent.isSameEntry(destination)) throw new Error('Choose a different destination folder.');
  if (clip.handle.kind === 'directory' && await (clip.handle as FolderHandle).resolve(destination) !== null)
    throw new Error('A folder cannot be pasted into itself or its children.');
  await copyEntry(clip.handle, destination);
  if (clip.move) {
    // Do not remove a replacement that appeared at the source during the copy.
    const current = clip.handle.kind === 'directory'
      ? await clip.parent.getDirectoryHandle(clip.handle.name) : await clip.parent.getFileHandle(clip.handle.name);
    if (!await current.isSameEntry(clip.handle)) throw new Error('Copied, but the source changed; it was not removed.');
    try { await clip.parent.removeEntry(clip.handle.name, { recursive: true }); }
    catch { throw new Error('Copied, but source removal failed and may be partial. Review both locations before retrying.'); }
  }
}
export function safeZipPath(name: string): string[] {
  const path = name.endsWith('/') ? name.slice(0, -1) : name;
  const parts = path.split('/');
  if (!path || /[\\:\x00-\x1f]/.test(path) || parts.length > 64 || parts.some(p => !p || p === '.' || p === '..'))
    throw new Error('Archive contains an unsafe path.');
  return parts;
}
function crc32(bytes: Uint8Array, previous = 0) {
  let crc = previous ^ -1;
  for (const byte of bytes) {
    crc ^= byte;
    for (let bit = 0; bit < 8; bit++) crc = (crc >>> 1) ^ ((crc & 1) ? 0xedb88320 : 0);
  }
  return (crc ^ -1) >>> 0;
}
/** Preflight the central directory before starting any writes/decompression. */
export function inspectZip(bytes: Uint8Array) {
  const view = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
  const u16 = (p: number) => view.getUint16(p, true), u32 = (p: number) => view.getUint32(p, true);
  if (bytes.length < 22 || bytes.length > ZIP_INPUT_LIMIT) throw new Error('ZIP input limit is 128 MiB.');
  let end = bytes.length - 22;
  while (end >= Math.max(0, bytes.length - 65557) && (u32(end) !== 0x06054b50 || end + 22 + u16(end + 20) !== bytes.length)) end--;
  if (end < Math.max(0, bytes.length - 65557)) throw new Error('Invalid ZIP archive.');
  const count = u16(end + 10), records = new Map<string, { size: number; crc: number }>();
  if (u16(end + 4) || u16(end + 6) || u16(end + 8) !== count || count > FILE_LIMIT) throw new Error('Unsupported ZIP archive.');
  let offset = u32(end + 16), total = 0;
  if (offset + u32(end + 12) !== end) throw new Error('Invalid ZIP directory.');
  for (let i = 0; i < count; i++) {
    if (offset + 46 > end || u32(offset) !== 0x02014b50) throw new Error('Invalid ZIP entry.');
    const len = u16(offset + 28), next = offset + 46 + len + u16(offset + 30) + u16(offset + 32);
    const method = u16(offset + 10), size = u32(offset + 24);
    if (next > end || (u16(offset + 8) & 1) || ![0, 8].includes(method) || ((u32(offset + 38) >>> 16) & 0xf000) === 0xa000)
      throw new Error('Encrypted, linked or unsupported ZIP entry.');
    const rawName = bytes.subarray(offset + 46, offset + 46 + len);
    // ZIP names without UTF-8 flags are restricted to ASCII on both platforms.
    if (!(u16(offset + 8) & 2048) && rawName.some(byte => byte > 127)) throw new Error('ZIP filenames must use UTF-8 or ASCII.');
    const name = new TextDecoder('utf-8', { fatal: true }).decode(rawName);
    safeZipPath(name);
    if (records.has(name)) throw new Error('Duplicate ZIP entry.');
    total += size;
    if (total > ZIP_OUTPUT_LIMIT) throw new Error('ZIP output limit is 512 MiB.');
    records.set(name, { size, crc: u32(offset + 16) }); offset = next;
  }
  if (offset !== end) throw new Error('Invalid ZIP directory size.');
  return records;
}
async function decodeZip(bytes: Uint8Array, records: ReturnType<typeof inspectZip>) {
  const files = new Map<string, Uint8Array[]>(), completed = new Set<string>();
  // Incremental inflation measures actual output, including when an archive
  // lies about its uncompressed sizes. No worker/blob/CSP support is required.
  const decoder = new Unzip(file => {
    const record = records.get(file.name);
    if (!record || files.has(file.name)) throw new Error('ZIP entry mismatch.');
    const chunks: Uint8Array[] = []; files.set(file.name, chunks);
    let size = 0, crc = 0;
    file.ondata = (error, chunk, final) => {
      if (error) throw error;
      size += chunk.length;
      if (size > record.size) throw new Error('ZIP output exceeds its declared size.');
      crc = crc32(chunk, crc); chunks.push(chunk);
      if (final) {
        if (size !== record.size || crc !== record.crc) throw new Error('ZIP integrity check failed.');
        completed.add(file.name);
      }
    };
    file.start();
  });
  decoder.register(UnzipInflate);
  for (let offset = 0; offset < bytes.length; offset += 65536) {
    decoder.push(bytes.subarray(offset, offset + 65536), offset + 65536 >= bytes.length);
    if (offset + 65536 < bytes.length) await new Promise(resolve => setTimeout(resolve, 0));
  }
  if (completed.size !== records.size) throw new Error('Incomplete ZIP archive.');
  return files;
}
export async function extractZip(parent: FolderHandle, handle: FileSystemFileHandle) {
  if (await extractConsoleZip(handle)) return;
  const file = await handle.getFile();
  if (file.size > ZIP_INPUT_LIMIT) throw new Error('ZIP input limit is 128 MiB.');
  const bytes = new Uint8Array(await file.arrayBuffer()), records = inspectZip(bytes);
  const files = await decodeZip(bytes, records);
  const name = handle.name + '.extracted';
  await requireAbsent(parent, name);
  const output = await parent.getDirectoryHandle(name, { create: true });
  try {
    for (const [path, chunks] of files) {
      const parts = safeZipPath(path); let folder = output;
      for (const part of parts.slice(0, path.endsWith('/') ? undefined : -1))
        folder = await folder.getDirectoryHandle(part, { create: true });
      if (path.endsWith('/')) continue;
      await requireAbsent(folder, parts[parts.length - 1]);
      const entry = await folder.getFileHandle(parts[parts.length - 1], { create: true });
      const writer = await entry.createWritable();
      try { await writer.write(new Blob(chunks as Uint8Array<ArrayBuffer>[])); await writer.close(); }
      catch (error) { try { await writer.abort(); } catch {} throw error; }
    }
  } catch (error) {
    try { await parent.removeEntry(name, { recursive: true }); }
    catch { throw new Error('Extraction failed; an incomplete destination could not be removed.'); }
    throw error;
  }
}
