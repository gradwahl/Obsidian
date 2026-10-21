const assert = require('node:assert/strict');
const fs = require('node:fs');
const ts = require('typescript');
require.extensions['.ts'] = (module, filename) => {
  const compiled = ts.transpileModule(fs.readFileSync(filename, 'utf8'), {
    compilerOptions: { target: ts.ScriptTarget.ES2022, module: ts.ModuleKind.CommonJS }
  });
  module._compile(compiled.outputText, filename);
};
const { ResourceCache } = require('../src/resourceCache.ts');
const { fitArtwork, imageDimensions, artworkResources } = require('../src/artworkMemory.ts');
const tick = () => new Promise(resolve => setImmediate(resolve));

(async () => {
  let active = 0, peak = 0;
  const pending = new Map(), disposed = [], aborted = [];
  const cache = new ResourceCache((key, signal) => new Promise((resolve, reject) => {
    active++; peak = Math.max(peak, active);
    pending.set(key, () => { active--; resolve(key); });
    signal.addEventListener('abort', () => {
      active--; aborted.push(key); reject(new DOMException('Aborted', 'AbortError'));
    }, { once: true });
  }), 2, 2, () => 1, value => disposed.push(value));
  const a = cache.acquire('a'), shared = cache.acquire('a'), b = cache.acquire('b');
  const c = cache.acquire('c'), dropped = cache.acquire('offscreen');
  const cancelled = assert.rejects(dropped.promise, { name: 'AbortError' });
  dropped.release(); await cancelled;
  assert.equal(cache.stats.running, 2);
  assert.equal(pending.size, 2, 'Shared consumers must deduplicate work');
  a.release(); assert.deepEqual(aborted, [], 'One remaining consumer keeps request alive');
  pending.get('a')(); assert.equal(await shared.promise, 'a'); shared.release(); await tick();
  assert.ok(pending.has('c')); assert.ok(!pending.has('offscreen'));
  pending.get('b')(); await b.promise; b.release(); await tick();
  pending.get('c')(); await c.promise; c.release(); await tick();
  assert.equal(peak, 2); assert.ok(cache.stats.retained <= 2);
  assert.ok(disposed.includes('a'), 'Least-recently-used unused resource must be disposed');
  const inFlight = cache.acquire('abort');
  const interrupted = assert.rejects(inFlight.promise, { name: 'AbortError' });
  inFlight.release(); await interrupted; await tick();
  assert.ok(aborted.includes('abort'));

  assert.deepEqual(fitArtwork(4000, 6000, 'card'), [320, 480]);
  assert.deepEqual(fitArtwork(3840, 2160, 'hero'), [1280, 720]);
  assert.deepEqual(fitArtwork(50, 80, 'card'), [50, 80]);
  const png = new ArrayBuffer(24), view = new DataView(png);
  view.setUint32(0, 0x89504e47); view.setUint32(16, 4000); view.setUint32(20, 6000);
  assert.deepEqual(imageDimensions(png), [4000, 6000]);
  assert.equal(imageDimensions(new ArrayBuffer(3)), null);

  // Exercise the real resize pipeline: only a resized blob is retained, bitmap
  // and input buffers are released; failures return the original image URL.
  const revoked = [], decoded = [];
  let serial = 0, closed = 0, canvasFreed = 0;
  global.document = { createElement() {
    return { width: 0, set height(value) { if (value === 0) canvasFreed++; },
      getContext() { return { drawImage() {} }; },
      toBlob(callback) { callback(new Blob(['thumbnail'])); } };
  }};
  global.URL.createObjectURL = () => 'blob:thumbnail-' + serial++;
  global.URL.revokeObjectURL = url => revoked.push(url);
  global.createImageBitmap = async (_blob, options) => {
    decoded.push(options);
    return { width: options.resizeWidth, height: options.resizeHeight, close() { closed++; } };
  };
  global.fetch = async () => ({ ok: true, headers: new Headers(), blob: async () => new Blob([png]) });
  const image = artworkResources.acquire('card|https://example.test/cover.art');
  const result = await image.promise;
  assert.equal(result.owned, true); assert.match(result.url, /^blob:/);
  assert.deepEqual([decoded[0].resizeWidth, decoded[0].resizeHeight], [320, 480]);
  assert.ok(result.bytes < 320 * 480 * 4 + 100);
  assert.equal(closed, 1); assert.ok(canvasFreed > 0); image.release();
  global.fetch = async () => { throw new Error('CORS unavailable'); };
  const fallback = artworkResources.acquire('card|https://example.test/cors-blocked.jpg');
  assert.equal((await fallback.promise).url, 'https://example.test/cors-blocked.jpg');
  fallback.release();
  console.log('PASS: bounded shared work, cancellation, LRU disposal, thumbnail dimensions, bitmap cleanup and CORS fallback');
})().catch(error => { console.error(error); process.exitCode = 1; });
