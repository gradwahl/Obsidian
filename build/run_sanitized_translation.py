import sys, os, subprocess
sys.path.insert(0, "tests")
from translation_server import translation_server
with translation_server() as server:
    subprocess.run(["./build/test_translation_sanitized"], env={**os.environ, "WOLODRIVE_TRANSLATE_URL":f"http://127.0.0.1:{server.server_port}/translate", "WOLODRIVE_TRANSLATE_API_KEY":"test-key"}, check=True)
