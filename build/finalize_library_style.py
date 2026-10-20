from pathlib import Path
import json

p=Path('tests/test_regressions.c')
s=p.read_text()
start=s.index('static void test_black_artwork_background')
end=s.index('static void test_latest_catalog_games',start)
s=s[:start]+'''static void test_native_artwork_bounds(void) {
    SDL_Surface *image = SDL_CreateRGBSurfaceWithFormat(0, 5, 5, 32, SDL_PIXELFORMAT_RGBA32);
    assert(image);
    SDL_FillRect(image, NULL, SDL_MapRGBA(image->format, 0, 0, 0, 0));
    SDL_Rect body = {1, 1, 3, 3}, panel_rect = {2, 2, 1, 1};
    SDL_FillRect(image, &body, SDL_MapRGBA(image->format, 0, 0, 0, 255));
    SDL_FillRect(image, &panel_rect, SDL_MapRGBA(image->format, 255, 255, 255, 255));
    SDL_Rect bounds = library_alpha_bounds(image);
    assert(bounds.x == 1 && bounds.y == 1 && bounds.w == 3 && bounds.h == 3);
    Uint8 *white_panel = (Uint8 *)image->pixels + 2 * image->pitch + 2 * 4;
    Uint8 *black_body = (Uint8 *)image->pixels + image->pitch + 4;
    assert(white_panel[0] == 255 && white_panel[3] == 255);
    assert(black_body[0] == 0 && black_body[3] == 255);
    prepare_library_logo(image);
    assert(black_body[0] == 245 && black_body[3] == 255);
    assert(((Uint8 *)image->pixels)[3] == 0);
    SDL_FreeSurface(image);
    puts("Native-alpha fitting preserves white and black hardware; monochrome logos retain their alpha.");
}

'''+s[end:]
s=s.replace('test_black_artwork_background();','test_native_artwork_bounds();')
p.write_text(s)

assets=json.loads(Path('assets/library/SOURCES.json').read_text(encoding='utf-8-sig'))
lines=['# Library artwork','',
       'Company logos, console logos and native-alpha console hardware photos are bundled for offline use.',
       'Original authors and brand owners retain their respective rights. Individual asset licenses apply independently of application code.',
       '',
       'Hardware images are displayed using their supplied alpha channels. WoloDrive does not remove white pixels or alter hardware colors at runtime.',
       'The 32X and Switch 2 photos have background-extracted variants prepared with OpenAI imagegen from the credited source photos. Other replacement cutouts are sourced as transparent PNGs; upstream preparation is recorded in SOURCES.json.',
       'Logos are rendered in white while preserving their original alpha. Transparent padding is ignored for consistent visual sizing.',
       '',
       'Console logo collection: [HVR88 / Monochrome Gaming Logos](https://github.com/HVR88/Monochrome-Gaming-Logos). See its [usage disclaimer](https://github.com/HVR88/Monochrome-Gaming-Logos#usage-disclaimer); these marks identify their respective brands and platforms.',
       '', '| Asset | Author | License |', '| --- | --- | --- |']
for a in assets:
    name=a['path'].replace('assets/library/','')
    author=a['author'].replace('|','/').replace('\n',' ')
    license=a['license']
    if a.get('license_url'): license=f'[{license}]({a["license_url"]})'
    lines.append(f'| [{name}]({a["source"]}) | {author} | {license} |')
lines+=['','Per-file source URLs, changes and SHA-256 hashes are recorded in `SOURCES.json`.']
Path('assets/library/CREDITS.md').write_text('\n'.join(lines)+'\n',encoding='utf-8')
