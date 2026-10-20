import json
import re
from pathlib import Path

assets = json.loads(Path('assets/library/SOURCES.json').read_text(encoding='utf-8-sig'))
consoles = iter(a['path'] for a in assets if a['key'].startswith('consoles/'))
brands = iter(a['path'] for a in assets if a['key'].startswith('companies/'))
p = Path('src/library.c')
s = p.read_text()
s, count = re.subn(r'\{"[^"]+", (?:NULL|"[^"]+")\}',
    lambda m: m[0][:-1] + ', "' + next(consoles) + '"}', s)
assert count == 41
s, count = re.subn(r'\{"[^"]+", \d+, \d+, \d+, \w+, COUNT\(\w+\)\}',
    lambda m: m[0][:-1] + ', "' + next(brands) + '"}', s)
assert count == 6
p.write_text(s)
