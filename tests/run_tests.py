"""Check UTF-8 catalogs and printf contracts, then exercise SDL in isolation."""
import json
import hashlib
import os
from pathlib import Path
import re
import subprocess
import tempfile
from translation_server import translation_server

catalog = Path("src/translations.def").read_text(encoding="utf-8")
artwork = json.loads(Path("assets/library/SOURCES.json").read_text(encoding="utf-8-sig"))
paths = {entry["path"] for entry in artwork}
bindings = set(re.findall(r'"(assets/library/[^\"]+)"', Path("src/library.c").read_text(encoding="utf-8")))
assert len(artwork) == len(paths) == 88 and paths == bindings
for entry in artwork:
    assert entry["source"].startswith(("https://commons.wikimedia.org/wiki/File:", "https://github.com/HVR88/Monochrome-Gaming-Logos/"))
    assert entry["author"] and entry["license"]
    assert hashlib.sha256(Path(entry["path"]).read_bytes()).hexdigest() == entry["sha256"], entry["path"]
print("Verified all 88 bundled company/console logos and photos, model bindings, attribution records and file hashes.", flush=True)
count = 0
for line in catalog.splitlines():
    if not line.startswith("TEXT("):
        continue
    key, values = line[5:-1].split(",", 1)
    strings = json.loads("[" + values + "]")
    assert len(strings) == 9 and all(strings), key
    signatures = [re.findall(r"%(?:[-+ #0]*\d*(?:\.\d+)?[a-zA-Z]|%)", s) for s in strings]
    assert all(signature == signatures[0] for signature in signatures), (key, signatures)
    if key.startswith("TXT_ROW_"):
        assert all(len(s.encode("utf-8")) < 64 for s in strings), key
    count += 1
assert count > 0
print(f"Checked {count} translation keys in all nine languages.", flush=True)
with tempfile.TemporaryDirectory(prefix="wolodrive-i18n-") as data_dir:
    subprocess.run(
        ["./build/test_i18n"],
        env={**os.environ, "XDG_DATA_HOME": data_dir, "SDL_VIDEODRIVER": "dummy"},
        check=True,
    )

    subprocess.run(
        ["./build/test_regressions"],
        env={**os.environ, "XDG_DATA_HOME": data_dir, "SDL_VIDEODRIVER": "dummy"},
        check=True,
    )

    with translation_server() as server:
        subprocess.run(
            ["./build/test_translation"],
            env={**os.environ, "XDG_DATA_HOME": data_dir,
                 "WOLODRIVE_TRANSLATE_URL": f"http://127.0.0.1:{server.server_port}/translate",
                 "WOLODRIVE_TRANSLATE_API_KEY": "test-key"},
            check=True,
        )
        assert all(r.get("source") == "auto" and r.get("format") == "text"
                   and r.get("api_key") == "test-key" for r in server.requests)
        source = 'Quotes: "test" & newline\n中文 Русский'
        requests = [r for r in server.requests if r["q"] == source]
        assert len(requests) == 9, "Cached translations must not issue duplicate requests"
        assert {r["target"] for r in requests} == {"en", "pl", "es", "fr", "nl", "de", "it", "zh", "ru"}
        assert not any(r["q"] == "queued old" for r in server.requests)
        assert sum(r["q"] == "error" for r in server.requests) == 1
