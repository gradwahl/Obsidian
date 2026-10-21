import { ResourceCache } from './resourceCache';

export type ArtworkSize = 'card' | 'hero';
interface ArtworkResource { url: string; bytes: number; owned: boolean; }
export const ARTWORK_LIMITS = { card: [320, 480], hero: [1280, 720] } as const;
const direct = (url: string): ArtworkResource => ({ url, bytes: 256 * 1024, owned: false });

// Read dimensions before decode so createImageBitmap can resize directly.
// Unknown formats use the canvas fallback; no assumption based on .art suffix.
export function imageDimensions(data: ArrayBuffer): [number, number] | null {
  const bytes = new Uint8Array(data), view = new DataView(data);
  if (bytes.length >= 24 && view.getUint32(0) === 0x89504e47)
    return [view.getUint32(16), view.getUint32(20)];
  if (bytes[0] === 255 && bytes[1] === 216) {
    let i = 2;
    while (i + 8 < bytes.length) {
      if (bytes[i++] !== 255) break;
      while (bytes[i] === 255) i++;
      const marker = bytes[i++];
      if (marker === 217 || marker === 218) break;
      const length = view.getUint16(i);
      if (length < 2) break;
      if ([192, 193, 194, 195, 197, 198, 199, 201, 202, 203, 205, 206, 207].includes(marker))
        return [view.getUint16(i + 5), view.getUint16(i + 3)];
      i += length;
    }
  }
  return null;
}

export function fitArtwork(width: number, height: number, size: ArtworkSize): [number, number] {
  const [maxWidth, maxHeight] = ARTWORK_LIMITS[size];
  const scale = Math.min(1, maxWidth / width, maxHeight / height);
  return [Math.max(1, Math.round(width * scale)), Math.max(1, Math.round(height * scale))];
}

async function resizeArtwork(key: string, signal: AbortSignal): Promise<ArtworkResource> {
  const split = key.indexOf('|');
  const size = key.slice(0, split) as ArtworkSize, url = key.slice(split + 1);
  const canvas = document.createElement('canvas');
  const context = canvas.getContext('2d');
  if (!context || !canvas.toBlob || !URL.createObjectURL) return direct(url);
  let bitmap: ImageBitmap | null = null;
  let image: HTMLImageElement | null = null;
  let inputUrl = '';
  const request = new AbortController();
  const cancel = () => request.abort();
  signal.addEventListener('abort', cancel, { once: true });
  if (signal.aborted) cancel();
  // A stalled remote host must not occupy one of both queue slots indefinitely.
  const deadline = setTimeout(cancel, 6000);
  try {
    // CORS is optional: an unavailable remote image still follows the existing
    // direct <img> fallback. No proxy, credentials, private service or disk cache.
    const response = await fetch(url, { signal: request.signal, credentials: url.startsWith('/') ? 'same-origin' : 'omit' });
    if (!response.ok || Number(response.headers.get('Content-Length')) > 8 * 1024 * 1024)
      return direct(url);
    const blob = await response.blob();
    if (blob.size > 8 * 1024 * 1024) return direct(url);
    const dimensions = imageDimensions(await blob.slice(0, 65536).arrayBuffer());
    let width: number, height: number;
    if (dimensions && typeof createImageBitmap === 'function') {
      [width, height] = fitArtwork(...dimensions, size);
      try {
        bitmap = await createImageBitmap(blob, { resizeWidth: width, resizeHeight: height, resizeQuality: 'high' });
      } catch { /* Older engines may support canvas but not bitmap resizing. */ }
    }
    if (!bitmap) {
      image = new Image(); inputUrl = URL.createObjectURL(blob);
      await new Promise<void>((resolve, reject) => {
        const abort = () => { image!.removeAttribute('src'); reject(new DOMException('Aborted', 'AbortError')); };
        image!.onload = () => { signal.removeEventListener('abort', abort); resolve(); };
        image!.onerror = () => { signal.removeEventListener('abort', abort); reject(new Error('Image decode failed')); };
        signal.addEventListener('abort', abort, { once: true });
        if (signal.aborted) { abort(); return; }
        image!.src = inputUrl;
      });
    }
    if (signal.aborted) throw new DOMException('Aborted', 'AbortError');
    [width, height] = fitArtwork(bitmap?.width || image!.naturalWidth, bitmap?.height || image!.naturalHeight, size);
    canvas.width = width; canvas.height = height;
    context.drawImage(bitmap || image!, 0, 0, width, height);
    const result = await new Promise<Blob | null>(resolve => canvas.toBlob(resolve, 'image/png'));
    if (!result || signal.aborted) throw new Error('Artwork resize cancelled');
    return { url: URL.createObjectURL(result), bytes: width * height * 4 + result.size, owned: true };
  } catch (error) {
    if (signal.aborted) throw error;
    return direct(url);
  } finally {
    clearTimeout(deadline); signal.removeEventListener('abort', cancel);
    bitmap?.close();
    if (image) { image.onload = null; image.onerror = null; image.removeAttribute('src'); }
    if (inputUrl) URL.revokeObjectURL(inputUrl);
    canvas.width = canvas.height = 0;
  }
}

// Two decodes at a time, 24 MiB idle/live-thumbnail target. Active leases remain
// valid; offscreen images release them, allowing LRU eviction + URL revocation.
export const artworkResources = new ResourceCache(resizeArtwork, 24 * 1024 * 1024, 2,
  value => value.bytes, value => { if (value.owned) URL.revokeObjectURL(value.url); });
