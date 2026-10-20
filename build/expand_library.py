import json, re, hashlib, html, urllib.request
from pathlib import Path
from concurrent.futures import ThreadPoolExecutor

root=Path('.')
records=json.loads(Path('assets/library/SOURCES.json').read_text(encoding='utf-8-sig'))
jobs=[]
def add(key,url,source,author,license,changes,license_url=None,title=None):
    path='assets/library/'+key+'.png'
    jobs.append((path,url))
    records.append(dict(key=key,path=path,title=title or key,source=source,original=url,downloaded=url,author=author,license=license,license_url=license_url,changes=changes))
def mono(key,name):
    url=f'https://raw.githubusercontent.com/HVR88/Monochrome-Gaming-Logos/main/png/{name}.png'
    add(key,url,f'https://github.com/HVR88/Monochrome-Gaming-Logos/blob/main/png/{name}.png','HVR88 / respective brand owner','Identification artwork; source usage disclaimer','Unmodified transparent logo, fitted proportionally in the UI.','https://github.com/HVR88/Monochrome-Gaming-Logos#usage-disclaimer')
credits=json.loads(Path('build/spela-photo-credits.json').read_text(encoding='utf-8-sig'))['photos']
photos={'nintendo-pokemini':'pkmn','sega-sg1000':'sg1k','nec-pce':'pce','nec-pcecd':'pcecd','nec-supergrafx':'sgx','nec-pcfx':'pcfx','bandai-wscolor':'ws','other-3do':'3do','other-cdi':'cdi','other-coleco':'cv','other-intellivision':'intv','other-odyssey2':'o2','other-channelf':'chaf'}
for key,console in photos.items():
    c=next(c for c in credits if c['console']==console)
    url=f'https://raw.githubusercontent.com/mattias800/spela/master/server/internal/api/static/console-photos/{c["file"]}'
    add('consoles/'+key,url,c['source'],c['author'],c['license'],'Upstream transparent PNG cutout supplied by Spela; no further changes.',title=c['title'])
logos={'sega-sg1000':'sega_sg1000','nec-pce':'nec_turbografx16','nec-pcecd':'nec_turbografxcd','nec-supergrafx':'nec_pcengine_supergrafx','nec-pcfx':'nec_pcfx','bandai-ws':'bandai_wonderswan','bandai-wscolor':'bandai_wonderswan_color','other-3do':'3do','other-cdi':'philips_cdi','other-coleco':'coleco_vision','other-intellivision':'mattel_intellivision','other-odyssey2':'magnavox_odyssey2','other-channelf':'fairchild_channelf','arcade-naomi':'sega_naomi','arcade-naomi2':'sega_naomi2','arcade-atomiswave':'sammy_atomiswave','arcade-classic':'arcade'}
for key,name in logos.items(): mono('logos/'+key,name)
mono('companies/arcade','arcade')
pages=[]
for path in ['build/new-commons.json','build/more-commons.json','build/bandai-commons.json']:
    pages+=list(json.loads(Path(path).read_text(encoding='utf-8-sig'))['query']['pages'].values())
commons={'consoles/bandai-ws':'WonderSwan-Black-Left.png','consoles/other-gx4000':'Amstrad-GX4000-Console-Set.png','logos/other-gx4000':'Amstrad GX4000 logo.svg','logos/nintendo-pokemini':'Pokemon mini logo.svg','companies/nec':'NEC logo.svg','companies/bandai':'Logo Bandai.svg'}
for key,title in commons.items():
    p=next(p for p in pages if p['title']=='File:'+title and 'imageinfo' in p)
    info=p['imageinfo'][0]; m=info['extmetadata']
    clean=lambda s:html.unescape(re.sub('<[^>]*>','',s))
    url=info.get('thumburl',info['url']).split('?')[0]
    add(key,url,'https://commons.wikimedia.org/wiki/File:'+title.replace(' ','_'),clean(m['Artist']['value']),m['LicenseShortName']['value'],'Wikimedia-generated thumbnail; no other changes.',m.get('LicenseUrl',{}).get('value'),title)
def download(job):
    path,url=job
    request=urllib.request.Request(url,headers={'User-Agent':'WoloDrive-artwork/1.0'})
    with urllib.request.urlopen(request,timeout=60) as response: data=response.read()
    Path(path).parent.mkdir(parents=True,exist_ok=True); Path(path).write_bytes(data)
    return path
with ThreadPoolExecutor(max_workers=4) as pool:
    for path in pool.map(download,jobs): print(path,flush=True)
Path('build/expanded-records.json').write_text(json.dumps(records,ensure_ascii=False,indent=2),encoding='utf8')

# Preserve Sony's existing PS4/PS5 positions and their catalog title-ID mappings.
text=Path('src/library.c').read_text()
for key in ['sony-vita','microsoft-xboxone','microsoft-series','nintendo-switch2']:
    text=re.sub(r'^.*assets/library/consoles/'+key+r'\.png.*\n','',text,flags=re.M)
text=text.replace('{"PlayStation",','{"PlayStation 1",').replace('{"PlayStation 3",','{"PlayStation 3 (experimental)",').replace('{"PSP",','{"PlayStation Portable (PSP)",').replace('{"Xbox",','{"Original Xbox",').replace('{"Xbox 360",','{"Xbox 360 (experimental)",').replace('{"Super Nintendo",','{"SNES / Super Famicom",').replace('{"Sega CD / Mega-CD",','{"Mega-CD / Sega CD",').replace('{"32X",','{"Sega 32X",').replace('{"Saturn",','{"Sega Saturn",').replace('{"Neo Geo AES / MVS",','{"Neo Geo AES",')
def entry(name,key,photo=True,logo=True):
    art='"assets/library/consoles/'+key+'.png"' if photo else 'NULL'
    mark='"assets/library/logos/'+key+'.png"' if logo else 'NULL'
    return '    {"'+name+'", NULL, '+art+', '+mark+'}'
text=text.replace('static const LibraryConsole sega[] = {','static const LibraryConsole sega[] = {\n'+entry('SG-1000','sega-sg1000')+',')
text=text.replace('static const LibraryConsole atari[] = {','static const LibraryConsole atari[] = {')
end=text.index('\n};',text.index('static const LibraryConsole nintendo'))
text=text[:end].rstrip()+',\n'+entry('Pokémon Mini','nintendo-pokemini')+text[end:]
groups={
 'nec':[('PC Engine / TurboGrafx-16','nec-pce'),('PC Engine CD / TurboGrafx-CD','nec-pcecd'),('SuperGrafx','nec-supergrafx'),('PC-FX','nec-pcfx')],
 'bandai':[('WonderSwan','bandai-ws'),('WonderSwan Color','bandai-wscolor')],
 'other':[('Panasonic 3DO','other-3do'),('Philips CD-i','other-cdi'),('ColecoVision','other-coleco'),('Intellivision','other-intellivision'),('Magnavox Odyssey 2','other-odyssey2'),('Fairchild Channel F','other-channelf'),('Epoch Super Cassette Vision','other-scv'),('Amstrad GX4000','other-gx4000')],
 'arcade':[('Sega NAOMI','arcade-naomi'),('Sega NAOMI 2','arcade-naomi2'),('Sega Atomiswave','arcade-atomiswave'),('Neo Geo MVS','snk-neogeo'),('Various classic arcade systems','arcade-classic')]
}
arrays=''
for group,items in groups.items():
    arrays+='static const LibraryConsole '+group+'[] = {\n'+',\n'.join(entry(n,k,group!='arcade',k!='other-scv') for n,k in items)+'\n};\n'
text=text.replace('static const LibraryCompany companies[] = {',arrays+'static const LibraryCompany companies[] = {')
text=text.replace('    {"SNK", 0, 143, 208, snk, COUNT(snk), "assets/library/companies/snk.png"}','    {"SNK", 0, 143, 208, snk, COUNT(snk), "assets/library/companies/snk.png"},\n    {"NEC", 0, 75, 160, nec, COUNT(nec), "assets/library/companies/nec.png"},\n    {"Bandai", 220, 0, 35, bandai, COUNT(bandai), "assets/library/companies/bandai.png"},\n    {"Other consoles", 140, 140, 140, other, COUNT(other), NULL},\n    {"Arcade systems", 140, 140, 140, arcade, COUNT(arcade), "assets/library/companies/arcade.png"}')
Path('src/library.c').write_text(text,encoding='utf8')
print('Updated library model',flush=True)
