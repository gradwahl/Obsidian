"""Real HTTP + native filesystem operations confined to disposable directories."""
import http.client
import importlib.util
import json
from pathlib import Path
import socket
import subprocess
import tempfile
import time
import unittest
import zipfile
from urllib.parse import urlencode

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('embed', ROOT/'scripts/embed_web_preview.py')
embed = importlib.util.module_from_spec(spec)
spec.loader.exec_module(embed)


class FileBridge(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory(prefix='poldrive-bridge-')
        cls.folder = Path(cls.temp.name)
        cls.storage = cls.folder/'storage'
        (cls.storage/'data').mkdir(parents=True)
        preview = cls.folder/'preview'; preview.mkdir()
        (preview/'index.html').write_text('<html>static catalogue</html>')
        embed.embed(preview, cls.folder/'ps5_web_assets.h')
        with socket.socket() as sock:
            sock.bind(('127.0.0.1', 0)); cls.port = sock.getsockname()[1]
        subprocess.run(['gcc', '-D_GNU_SOURCE', '-DWOLO_FILE_BRIDGE', f'-DWOLO_WEB_PORT={cls.port}',
                        '-DWOLO_FILES_ROOT=' + json.dumps(str(cls.storage)), '-Wall', '-Wextra', '-Werror',
                        '-O2', '-I', str(cls.folder), str(ROOT/'src/web_payload.c'),
                        str(ROOT/'src/file_manager.c'), '-lz', '-pthread', '-o', str(cls.folder/'server')], check=True)
        cls.log = (cls.folder/'server.log').open('wb')
        cls.proc = subprocess.Popen([str(cls.folder/'server')], cwd=cls.folder, stderr=cls.log)
        for _ in range(100):
            try:
                status, session = cls.request('/session')
                if status == 200:
                    cls.token = session['token']; break
            except (OSError, http.client.HTTPException):
                pass
            time.sleep(.03)
        else:
            cls.proc.terminate(); cls.proc.wait(); raise RuntimeError((cls.folder/'server.log').read_text())

    @classmethod
    def tearDownClass(cls):
        cls.proc.terminate(); cls.proc.wait(timeout=5); cls.log.close(); cls.temp.cleanup()

    @classmethod
    def request(cls, suffix='', fields=None, headers=None, method=None):
        default = {'X-PolDrive-Bridge':'1'}
        if hasattr(cls, 'token'): default['X-PolDrive-Token'] = cls.token
        if fields is not None: default['Content-Type'] = 'application/x-www-form-urlencoded'
        default.update(headers or {})
        conn = http.client.HTTPConnection('127.0.0.1', cls.port, timeout=5)
        conn.request(method or ('POST' if fields is not None else 'GET'), '/api/wolodrive/files'+suffix,
                     urlencode(fields) if fields is not None else None, default)
        response = conn.getresponse(); body = response.read(); status = response.status; conn.close()
        return status, json.loads(body) if body else None

    def test_navigation_sorting_and_filename_encoding(self):
        (self.storage/'data/z-folder').mkdir(exist_ok=True)
        name = 'café "&+%.txt'
        (self.storage/'data'/name).write_text('real bytes')
        status, result = self.request('?' + urlencode({'path':'data'}))
        self.assertEqual(status, 200)
        self.assertEqual(result['entries'][0]['kind'], 'directory')
        self.assertIn(name, [entry['name'] for entry in result['entries']])
        self.assertIn('data', [item['name'] for item in self.request('?path=')[1]['entries']])
        self.assertEqual(self.request('?path=data%2Fz-folder')[1]['entries'], [])

    def test_copy_move_conflict_delete_and_unzip(self):
        (self.storage/'data/source').write_bytes(b'console source')
        args = {'action':'copy', 'source':'data/source', 'destination':'data/copied'}
        self.assertEqual(self.request(fields=args)[0], 200)
        self.assertEqual((self.storage/'data/copied').read_bytes(), b'console source')
        self.assertEqual(self.request(fields=args)[0], 400)
        self.assertEqual((self.storage/'data/source').read_bytes(), b'console source')
        args.update(action='move', source='data/copied', destination='data/moved')
        self.assertEqual(self.request(fields=args)[0], 200)
        self.assertFalse((self.storage/'data/copied').exists())
        with zipfile.ZipFile(self.storage/'data/test.zip', 'w', zipfile.ZIP_DEFLATED) as archive:
            archive.writestr('folder/content.txt', 'unzipped')
        self.assertEqual(self.request(fields={'action':'unzip','source':'data/test.zip'})[0], 200)
        self.assertEqual((self.storage/'data/test.zip.extracted/folder/content.txt').read_text(), 'unzipped')
        self.assertEqual(self.request(fields={'action':'delete','source':'data/moved'})[0], 200)
        self.assertFalse((self.storage/'data/moved').exists())

    def test_access_boundary(self):
        for headers in ({'Host':'attacker.invalid'}, {'Origin':'https://attacker.invalid'},
                        {'Origin':'null'}, {'Sec-Fetch-Site':'cross-site'}, {'X-PolDrive-Bridge':''}):
            self.assertEqual(self.request('/session', headers=headers)[0], 403)
        self.assertEqual(self.request('?path=data', headers={'X-PolDrive-Token':'wrong'})[0], 403)
        for method in ('PUT', 'OPTIONS', 'DELETE', 'POST'):
            status, _ = self.request('/session', method=method)
            self.assertNotEqual(status, 200)
        self.assertEqual(len(self.token), 64)
        # Ordinary LAN catalogue still works; its origin cannot use console files.
        conn = http.client.HTTPConnection('127.0.0.2', self.port)
        conn.request('GET','/'); response = conn.getresponse()
        self.assertEqual(response.status, 200); response.read(); conn.close()
        # Reach the server through a non-loopback interface, if available.
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
            sock.connect(('192.0.2.1', 9)); address = sock.getsockname()[0]
        if not address.startswith('127.'):
            conn = http.client.HTTPConnection(address, self.port)
            conn.request('GET','/api/wolodrive/files/session',headers={
                'Host':f'127.0.0.1:{self.port}', 'X-PolDrive-Bridge':'1'})
            response = conn.getresponse(); self.assertEqual(response.status,403); response.read(); conn.close()
            self.assertEqual(self.lan_request('/session',host=address)[0],200)


    def test_traversal_links_invalid_bodies_and_recovery(self):
        link = self.storage/'data/link'
        if not link.exists(): link.symlink_to(self.folder)
        for path in ('../', '/etc', 'data/../', 'data/link', 'data\\other', 'data\0ignored'):
            self.assertEqual(self.request('?' + urlencode({'path':path}))[0], 400, path)
        for query in ('?path=%', '?path=data&path=', '?path=%00'):
            self.assertEqual(self.request(query)[0], 400)
        for fields in ({'action':'delete','source':''}, {'action':'install','source':'data'},
                       {'action':'copy','source':'data'}, {'action':'delete','source':'../outside'}):
            self.assertEqual(self.request(fields=fields)[0],400)
        self.assertEqual(self.request(fields={'action':'delete','source':'data/link'})[0],400)
        self.assertEqual(self.request('?path=data')[0],200)

    def test_request_framing_and_capable_server_reuse(self):
        body = b'action=delete&source=data%2Fmust-not-be-deleted'
        target = self.storage/'data/must-not-be-deleted'; target.write_text('keep')
        common = (f'POST /api/wolodrive/files HTTP/1.1\r\nHost: 127.0.0.1:{self.port}\r\n'
                  f'X-PolDrive-Bridge: 1\r\nX-PolDrive-Token: {self.token}\r\n'
                  'Content-Type: application/x-www-form-urlencoded\r\n').encode()
        for framing in (b'Content-Length: -1\r\n', b'Content-Length: 999999\r\n',
                        b'Content-Length: 0\r\nContent-Length: 0\r\n',
                        b'Transfer-Encoding: chunked\r\n'):
            with socket.create_connection(('127.0.0.1',self.port),timeout=5) as sock:
                sock.sendall(common+framing+b'\r\n'+body)
                self.assertIn(b'400 File bridge error',sock.recv(2048))
            self.assertTrue(target.exists())
        result = subprocess.run([str(self.folder/'server')],cwd=self.folder,capture_output=True,timeout=5)
        self.assertEqual(result.returncode,0)
        self.assertIn(b'Reusing running server',result.stderr)
        conn = http.client.HTTPConnection('127.0.0.1',self.port)
        conn.request('GET','/api/wolodrive/health'); response = conn.getresponse()
        self.assertIn(b'wolodrive-server-v10-files-lan-open ',response.read()); conn.close()

    def lan_request(self,suffix='',body=None,headers=None,method='GET',host='127.0.0.2'):
        conn=http.client.HTTPConnection(host,self.port,timeout=5)
        conn.request(method,'/api/wolodrive/files'+suffix,body,{'X-PolDrive-Bridge':'1',**(headers or {})})
        response=conn.getresponse(); data=response.read(); result=(response.status,dict(response.getheaders()),data); conn.close()
        return result

    def test_lan_code_free_content_edits_and_binary_transfer(self):
        status,_,data=self.lan_request('/session')
        self.assertEqual(status,200)
        self.assertNotIn(b'pairCode',data)
        token=json.loads(data)['token']
        self.assertEqual(len(token),64)
        self.assertEqual(self.lan_request('?path=data')[0],403)
        headers={'X-PolDrive-Token':token}
        self.assertEqual(self.lan_request('?path=data',headers=headers)[0],200)
        target=self.storage/'data/remote.ini'; target.write_bytes(b'value=1\r\n')
        status,meta,data=self.lan_request('/content?path=data%2Fremote.ini',headers=headers)
        self.assertEqual((status,data),(200,b'value=1\r\n'))
        revision=meta['X-PolDrive-Revision']
        write_headers={**headers,'X-PolDrive-Revision':revision,'Content-Type':'application/octet-stream'}
        self.assertEqual(self.lan_request('/content?path=data%2Fremote.ini',b'value=2\r\n',write_headers,'POST')[0],200)
        self.assertEqual(target.read_bytes(),b'value=2\r\n')
        self.assertEqual(self.lan_request('/content?path=data%2Fremote.ini',b'stale',write_headers,'POST')[0],409)
        self.assertEqual(target.read_bytes(),b'value=2\r\n')
        binary=bytes(range(256))*600 # Exceeds the old 32 KiB request buffer.
        upload_headers={**write_headers,'X-PolDrive-Revision':'new'}
        self.assertEqual(self.lan_request('/content?path=data%2Fremote.bin',binary,upload_headers,'POST')[0],200)
        self.assertEqual(self.lan_request('/content?path=data%2Fremote.bin',headers=headers)[2],binary)
        self.assertEqual(self.lan_request('/content?path=data%2Fremote.bin',b'overwrite',upload_headers,'POST')[0],409)
        self.assertEqual(self.lan_request('/content?path=data%2Fremote.bin',headers=headers,method='HEAD')[2],b'')
        for path in ('../outside','data/link',''):
            self.assertNotEqual(self.lan_request('/content?'+urlencode({'path':path}),headers=headers)[0],200)
        self.assertEqual(self.lan_request('/content?path=data%2Fremote.ini',headers={**headers,'Origin':'https://evil.invalid'})[0],403)

    def test_z_no_pairing_endpoint_or_code_disclosure(self):
        for _ in range(8):
            status,_,body=self.lan_request('/session')
            self.assertEqual(status,200)
            self.assertNotIn(b'pairCode',body)
        self.assertEqual(self.lan_request('/session','code=unneeded',
            {'Content-Type':'application/x-www-form-urlencoded'},'POST')[0],405)
        self.assertEqual(self.request('/session')[0],200)


if __name__ == '__main__': unittest.main()
