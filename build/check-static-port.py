import pathlib
import subprocess
import sys
import time
import urllib.request

root = pathlib.Path(sys.argv[1]).resolve()
process = subprocess.Popen(
    [sys.executable, '-m', 'http.server', '3000', '--bind', '0.0.0.0', '--directory', str(root)],
    stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL
)
try:
    for attempt in range(30):
        assert process.poll() is None, 'Server could not bind port 3000'
        try:
            with urllib.request.urlopen('http://127.0.0.1:3000/', timeout=1) as response:
                assert response.status == 200
                assert b'/src/main.tsx' not in response.read()
            print('PASS: Python static server on 0.0.0.0:3000')
            break
        except OSError:
            time.sleep(0.1)
    else:
        raise AssertionError('Server did not start')
finally:
    process.terminate()
    process.wait(timeout=5)
