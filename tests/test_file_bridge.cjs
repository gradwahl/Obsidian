const assert = require('node:assert/strict');
const fs = require('node:fs');
const ts = require('typescript');
require.extensions['.ts'] = (module, filename) => module._compile(ts.transpileModule(
  fs.readFileSync(filename, 'utf8'), { compilerOptions: { target: ts.ScriptTarget.ES2022,
    module: ts.ModuleKind.CommonJS, esModuleInterop: true } }).outputText, filename);
const { connectConsoleFiles, consoleFilePath } = require('../src/fileBridge.ts');
const { listFolder, pasteEntry, extractZip } = require('../src/fileManager.ts');
(async () => {
  let calls = [];
  const token = 'a'.repeat(64);
  global.fetch = async (url, options) => {
    calls.push({url, options});
    if (url.endsWith('/session')) return { ok: true, json: async () => ({version: 3, token}) };
    assert.equal(options.headers['X-Obsidian-Token'], token);
    if (options.method === 'POST') return { ok: true, json: async () => ({ok: true}) };
    return { ok: true, json: async () => ({entries: [
      {name:'Destination', kind:'directory', size:0}, {name:'hello.txt', kind:'file', size:3},
      {name:'archive.zip', kind:'file', size:8}]}) };
  };
  assert.equal(await connectConsoleFiles(), null);
  global.window = {location: {hostname:'preview.example', hash:'#native-unavailable'}};
  assert.equal(await connectConsoleFiles(), null);
  global.window.location = {hostname:'127.0.0.1', hash:''};
  assert.equal(await connectConsoleFiles(), null);
  assert.equal(calls.length, 0, 'Static previews require no bridge calls');
  global.window.location.hash = '#native-unavailable';
  const bridge = await connectConsoleFiles();
  assert.equal(calls[0].options.headers['X-Obsidian-Bridge'], '1');
  const trail = bridge.initialTrail();
  assert.deepEqual(trail.map(consoleFilePath), ['/', '/data']);
  const folder = trail[1], entries = await listFolder(folder);
  const file = entries.find(item => item.name === 'hello.txt');
  const destination = entries.find(item => item.name === 'Destination');
  await pasteEntry({parent:folder,handle:file,move:false}, destination);
  assert.deepEqual(Object.fromEntries(new URLSearchParams(calls.at(-1).options.body)),
    {action:'copy',source:'data/hello.txt',destination:'data/Destination/hello.txt'});
  await pasteEntry({parent:folder,handle:file,move:true}, destination);
  assert.equal(new URLSearchParams(calls.at(-1).options.body).get('action'), 'move');
  await extractZip(folder, entries.find(item => item.name === 'archive.zip'));
  assert.equal(new URLSearchParams(calls.at(-1).options.body).get('action'), 'unzip');
  await folder.removeEntry('hello.txt', {recursive:true});
  assert.equal(new URLSearchParams(calls.at(-1).options.body).get('action'), 'delete');
  await assert.rejects(() => folder.removeEntry('../escape'), /Unsupported filename/);
  await assert.rejects(() => pasteEntry({parent:folder,handle:file,move:false}, {}), /Cannot mix/);
  global.fetch = async () => ({ok:false,json:async()=>({error:'Permission denied'})});
  await assert.rejects(() => listFolder(folder), /Permission denied/);
  global.fetch = async () => ({ok:true,json:async()=>({version:3,token:'invalid'})});
  await assert.rejects(connectConsoleFiles, /Unsupported console file bridge/);
  // LAN clients connect without a code but still get a random session token.
  global.window.location={hostname:'192.168.1.20',hash:''};
  calls=[];
  global.fetch=async (url,options)=> {
    calls.push({url,options});
    if (url==='/') return {headers:new Headers({'X-Obsidian-Files':'3'})};
    if (url.endsWith('/session')) {
      assert.equal(options.method,'GET');
      assert.equal(options.headers['X-Obsidian-Bridge'],'1');
      return {ok:true,json:async()=>({version:3,token})};
    }
    throw new Error('Unexpected LAN request');
  };
  const remote=await connectConsoleFiles();
  assert.ok(remote,'A same-origin LAN client connects without entering a code');
  const remoteFolder=remote.initialTrail()[1];
  const remoteFile=remote.childFile(remoteFolder,'settings.ini');
  const binary=new Uint8Array([0,1,255,10]);
  global.fetch=async (url,options)=> {
    calls.push({url,options});
    if (options.method==='POST') return {ok:true,json:async()=>({ok:true})};
    return {ok:true,headers:new Headers({'X-Obsidian-Revision':'1-2-abc'}),arrayBuffer:async()=>binary.buffer};
  };
  assert.deepEqual(new Uint8Array((await remote.readFile(remoteFile)).bytes),binary);
  await remote.writeFile(remoteFile,binary,'1-2-abc');
  assert.equal(calls.at(-1).options.headers['X-Obsidian-Revision'],'1-2-abc');
  assert.deepEqual(new Uint8Array(await calls.at(-1).options.body.arrayBuffer()),binary);
  await assert.rejects(()=>remote.writeFile(remoteFile,new Uint8Array(8*1024*1024+1),'new'),/8 MiB/);
  global.fetch=async()=>({ok:false,json:async()=>({error:'File changed on console; reload before saving'})});
  await assert.rejects(()=>remote.writeFile(remoteFile,binary,'1-2-abc'),/File changed/);
  console.log('PASS: console bridge discovery, native operations, root navigation and static independence');
})().catch(error => { console.error(error); process.exitCode=1; });
