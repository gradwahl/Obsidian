"""Check UTF-8 catalogs and printf contracts, then exercise SDL in isolation."""
import json
import os
from pathlib import Path
import re
import subprocess
import tempfile

catalog = Path("src/translations.def").read_text(encoding="utf-8")
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
