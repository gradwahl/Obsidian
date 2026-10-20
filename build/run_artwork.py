import os
import subprocess
import sys
import tempfile
sys.path.insert(0, "tests")
from artwork_server import artwork_server
with artwork_server() as (server, requests), tempfile.TemporaryDirectory() as root:
    result = subprocess.run(["./build/test_artwork"], env={**os.environ,
        "SDL_VIDEODRIVER": "dummy", "WOLODRIVE_ARTWORK_TEST_DIR": root,
        "WOLODRIVE_ARTWORK_TEST_URL": f"http://127.0.0.1:{server.server_port}"})
    assert "/cached-during-stall" not in requests
    sys.exit(result.returncode)
