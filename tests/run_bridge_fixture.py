"""Build and run the C bridge against a disposable build/ directory only."""
import json
from pathlib import Path
import subprocess
import zipfile

ROOT = Path(__file__).resolve().parents[1]
folder = ROOT/'build/bridge-fixture'
storage = folder/'storage'
for name in ('Destination', 'Folder'):
    (storage/'data'/name).mkdir(parents=True, exist_ok=True)
(storage/'data/hello.txt').write_text('hello world')
(storage/'data/settings.ini').write_bytes(b'\xef\xbb\xbfoption=original\r\n')
(storage/'data/binary.bin').write_bytes(bytes(range(256)))
with zipfile.ZipFile(storage/'data/archive.zip','w',zipfile.ZIP_DEFLATED) as archive:
    archive.writestr('nested/data.txt','extracted data')
subprocess.run(['python3', str(ROOT/'scripts/embed_web_preview.py'), str(folder/'preview'),
                str(folder/'ps5_web_assets.h')],check=True)
subprocess.run(['gcc','-D_GNU_SOURCE','-DWOLO_FILE_BRIDGE','-DWOLO_WEB_PORT=3077',
                '-DWOLO_FILES_ROOT='+json.dumps(str(storage)), '-Wall','-Wextra','-Werror','-O2',
                '-I',str(folder),str(ROOT/'src/web_payload.c'),str(ROOT/'src/file_manager.c'),
                '-lz','-pthread','-o',str(folder/'server')],check=True)
subprocess.run([str(folder/'server')], cwd=folder, check=True)
