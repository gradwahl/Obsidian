const fs = require('node:fs');
const path = require('node:path');
const root = path.resolve(__dirname, '..');
const output = path.join(root, 'build/bridge-fixture/preview');
fs.mkdirSync(output, {recursive:true});
require('esbuild').buildSync({entryPoints:[path.join(__dirname,'bridge_browser_fixture.tsx')],
  bundle:true,outfile:path.join(output,'fixture.js'),platform:'browser',jsx:'automatic',
  loader:{'.png':'dataurl'},define:{'process.env.NODE_ENV':'"production"'}});
fs.writeFileSync(path.join(output,'index.html'), '<!doctype html><html><head><title>Disposable PS5 file bridge test</title><link rel="stylesheet" href="/fixture.css"></head><body style="margin:0;background:black"><div id="root"></div><script src="/fixture.js"></script></body></html>');
