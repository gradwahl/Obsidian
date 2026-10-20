import json,re,hashlib
from pathlib import Path
from PIL import Image

records=json.loads(Path('build/expanded-records.json').read_text())
p=next(p for p in json.loads(Path('build/more-commons.json').read_text(encoding='utf-8-sig'))['query']['pages'].values() if 'wController' in p['title'])
info=p['imageinfo'][0];meta=info['extmetadata']
records.append(dict(key='consoles/other-scv',path='assets/library/consoles/other-scv.png',title=p['title'],source='https://commons.wikimedia.org/wiki/File:Super-Cassette-Vision-wController-L.jpg',original=info['url'],downloaded=info['thumburl'].split('?')[0],author='Evan-Amos',license=meta['LicenseShortName']['value'],license_url=meta.get('LicenseUrl',{}).get('value'),changes='Background-extracted variant prepared with OpenAI imagegen from the attributed hardware photo; preserves native output alpha.'))
bindings=set(re.findall(r'"(assets/library/[^\"]+)"',Path('src/library.c').read_text(encoding='utf8')))
unused=[r for r in records if r['path'] not in bindings]
records=[r for r in records if r['path'] in bindings]
assert len(records)==len(bindings)
for r in records:
    r['sha256']=hashlib.sha256(Path(r['path']).read_bytes()).hexdigest()
    image=Image.open(r['path']).convert('RGBA')
    if r['key'].startswith('consoles/'):
        assert image.getchannel('A').getextrema()==(0,255),r['path']
Path('assets/library/SOURCES.json').write_text(json.dumps(records,ensure_ascii=False,indent=2)+'\n',encoding='utf8')
for r in unused:
    path=Path(r['path']).resolve()
    assert path.is_relative_to(Path('assets/library').resolve())
    path.unlink()
credits=Path('assets/library/CREDITS.md').read_text()
credits=credits[:credits.index('| Asset |')]
credits=credits.replace('The 32X and Switch 2 photos','The 32X and Super Cassette Vision photos')
credits+='Arcade cards use system logos rather than console hardware photos. The Other consoles category uses a text label; Super Cassette Vision uses its hardware photo and name without a separate logo.\n\n'
credits+='| Asset | Author | License |\n| --- | --- | --- |\n'
for r in records:
    credits+=f'| [{r["key"]}.png]({r["source"]}) | {r["author"]} | {r["license"]} |\n'
Path('assets/library/CREDITS.md').write_text(credits,encoding='utf8')
print(f'{len(records)} attributed artwork files; removed {len(unused)} unused assets')
