const assert = require('node:assert/strict');
const fs = require('node:fs');
const ts = require('typescript');
const { zipSync, strToU8 } = require('fflate');
require.extensions['.ts'] = (module, filename) => module._compile(ts.transpileModule(
  fs.readFileSync(filename, 'utf8'), { compilerOptions: { target: ts.ScriptTarget.ES2022,
    module: ts.ModuleKind.CommonJS, esModuleInterop: true } }).outputText, filename);
const { listFolder, pasteEntry, extractZip, inspectZip, safeZipPath } = require('../src/fileManager.ts');
class Handle {
  constructor(name, kind, parent = null, bytes = new Uint8Array()) {
    Object.assign(this, { name, kind, parent, bytes, children: new Map() });
  }
  async isSameEntry(other) { return this === other; }
  async resolve(other) {
    const parts = [];
    while (other && other !== this) { parts.unshift(other.name); other = other.parent; }
    return other === this ? parts : null;
  }
  async *values() { yield* this.children.values(); }
  async getDirectoryHandle(name, opts) { return this.get(name, 'directory', opts); }
  async getFileHandle(name, opts) { return this.get(name, 'file', opts); }
  get(name, kind, opts) {
    if (this.failWrite && opts?.create) throw new DOMException('Denied', 'NotAllowedError');
    let entry = this.children.get(name);
    if (!entry && opts?.create) { entry = new Handle(name, kind, this); this.children.set(name, entry); }
    if (!entry) throw new DOMException('Missing', 'NotFoundError');
    if (entry.kind !== kind) throw new DOMException('Wrong kind', 'TypeMismatchError');
    return entry;
  }
  async getFile() { return new File([this.bytes], this.name); }
  async createWritable() {
    const target = this, parts = [];
    const stream = new WritableStream({ write(part) { parts.push(part); },
      async close() { target.bytes = new Uint8Array(await new Blob(parts).arrayBuffer()); } });
    // The real browser API also exposes write/close convenience methods.
    stream.write = async data => { const writer = stream.getWriter(); await writer.write(data); writer.releaseLock(); };
    stream.close = async () => { const writer = stream.getWriter(); await writer.close(); writer.releaseLock(); };
    return stream;
  }
  async removeEntry(name) {
    if (this.failRemove) throw new DOMException('Denied', 'NotAllowedError');
    if (!this.children.delete(name)) throw new DOMException('Missing', 'NotFoundError');
  }
}
(async () => {
  const root = new Handle('root', 'directory');
  const source = await root.getDirectoryHandle('source', { create: true });
  const nested = await source.getDirectoryHandle('sub', { create: true });
  const item = await nested.getFileHandle('data.txt', { create: true }); item.bytes = strToU8('original');
  const target = await root.getDirectoryHandle('target', { create: true });
  const clip = { parent: root, handle: source, move: false };
  await pasteEntry(clip, target);
  assert.equal(new TextDecoder().decode((await (await target.getDirectoryHandle('source')).getDirectoryHandle('sub')).children.get('data.txt').bytes), 'original');
  await assert.rejects(pasteEntry(clip, target), /exists/);
  await assert.rejects(pasteEntry(clip, nested), /children/);
  await assert.rejects(pasteEntry(clip, root), /different/);
  const moved = await root.getDirectoryHandle('moved', { create: true });
  await pasteEntry({ ...clip, move: true }, moved); assert.ok(!root.children.has('source'));
  const first = await root.getFileHandle('z.txt', { create: true }); first.bytes = strToU8('unchanged');
  root.failRemove = true;
  await assert.rejects(pasteEntry({ parent: root, handle: first, move: true }, target), /Review both locations/);
  assert.ok(root.children.has('z.txt') && target.children.has('z.txt')); root.failRemove = false;
  const bad = await root.getDirectoryHandle('bad', { create: true }); bad.failWrite = true;
  await assert.rejects(pasteEntry({ parent: root, handle: first, move: false }, bad));
  assert.equal(bad.children.size, 0);
  assert.equal((await listFolder(root)).at(-1).name, 'z.txt');
  for (const path of ['../escape', '/absolute', 'a/../../escape', 'a\\b', 'C:/a', 'a//b', './a', 'a\0b'])
    assert.throws(() => safeZipPath(path), /unsafe/);
  const zip = await root.getFileHandle('pack.zip', { create: true });
  zip.bytes = zipSync({ 'nested/': new Uint8Array(), 'nested/data.txt': strToU8('hello') });
  assert.equal(inspectZip(zip.bytes).size, 2);
  await extractZip(root, zip);
  const output = await root.getDirectoryHandle('pack.zip.extracted');
  assert.equal(new TextDecoder().decode((await (await output.getDirectoryHandle('nested')).getFileHandle('data.txt')).bytes), 'hello');
  await assert.rejects(extractZip(root, zip), /exists/);
  await root.removeEntry('pack.zip.extracted');
  for (const name of ['../escape', '/absolute', 'a\\escape']) {
    zip.bytes = zipSync({ [name]: strToU8('evil') });
    await assert.rejects(extractZip(root, zip), /unsafe/);
    assert.ok(!root.children.has('pack.zip.extracted'));
  }
  zip.bytes = zipSync({ 'file.txt': strToU8('hello') });
  const signature = Buffer.from(zip.bytes).indexOf(Buffer.from('PK\x01\x02', 'binary'));
  zip.bytes[signature + 16] ^= 1;
  await assert.rejects(extractZip(root, zip), /integrity/);
  assert.ok(!root.children.has('pack.zip.extracted'));
  assert.throws(() => inspectZip(zip.bytes.subarray(0, 10)));
  zip.bytes = zipSync({ 'file.txt': strToU8('not empty') });
  const central = Buffer.from(zip.bytes).indexOf(Buffer.from('PK\x01\x02', 'binary'));
  zip.bytes.fill(0, central + 16, central + 20); zip.bytes.fill(0, central + 24, central + 28);
  await assert.rejects(extractZip(root, zip), /declared size/);
  assert.ok(!root.children.has('pack.zip.extracted'));
  console.log('PASS: browser folder copy/move, conflicts, denied writes, ZIP integrity and unsafe paths');
})().catch(error => { console.error(error); process.exitCode = 1; });
