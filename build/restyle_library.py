from pathlib import Path
import re
import json

p = Path('src/ui_netflix.c')
s = p.read_text()
start = s.index('/* Change the display matte')
end = s.index('static LibraryArtwork *load_library_artwork', start)
s = s[:start] + '''/* Native alpha defines the product bounds; hardware pixels are never keyed out. */
static SDL_Rect library_alpha_bounds(SDL_Surface *surface) {
    int left = surface->w, top = surface->h, right = -1, bottom = -1;
    if (SDL_LockSurface(surface) != 0) return (SDL_Rect){0, 0, surface->w, surface->h};
    for (int y = 0; y < surface->h; y++) {
        for (int x = 0; x < surface->w; x++) {
            const Uint8 *pixel = (const Uint8 *)surface->pixels + y * surface->pitch + x * 4;
            if (pixel[3] < 16) continue;
            left = SDL_min(left, x); top = SDL_min(top, y);
            right = SDL_max(right, x); bottom = SDL_max(bottom, y);
        }
    }
    SDL_UnlockSurface(surface);
    return right < left ? (SDL_Rect){0, 0, 0, 0} :
                         (SDL_Rect){left, top, right - left + 1, bottom - top + 1};
}

/* White marks have consistent contrast on charcoal; preserve the logo's alpha. */
static void prepare_library_logo(SDL_Surface *surface) {
    if (SDL_LockSurface(surface) != 0) return;
    for (int y = 0; y < surface->h; y++) {
        for (int x = 0; x < surface->w; x++) {
            Uint8 *pixel = (Uint8 *)surface->pixels + y * surface->pitch + x * 4;
            pixel[0] = pixel[1] = pixel[2] = 245;
        }
    }
    SDL_UnlockSurface(surface);
}

''' + s[end:]
s = s.replace('        prepare_library_artwork(surface, strstr(path, "/companies/") != NULL);',
'''        if (strstr(path, "/companies/") || strstr(path, "/logos/")) prepare_library_logo(surface);
        art->source_bounds = library_alpha_bounds(surface);''')
s = s.replace('    art->texture = SDL_CreateTextureFromSurface(app->renderer, surface);',
'''    if (!display) art->source_bounds = (SDL_Rect){0, 0, surface->w, surface->h};
    art->texture = SDL_CreateTextureFromSurface(app->renderer, surface);''')
s = s.replace('    float scale = fminf((float)bounds.w / art->width, (float)bounds.h / art->height);\n    int width = (int)(art->width * scale), height = (int)(art->height * scale);',
'''    if (art->source_bounds.w <= 0 || art->source_bounds.h <= 0) return false;
    float scale = fminf((float)bounds.w / art->source_bounds.w, (float)bounds.h / art->source_bounds.h);
    int width = (int)(art->source_bounds.w * scale), height = (int)(art->source_bounds.h * scale);''')
s = s.replace('SDL_RenderCopy(app->renderer, art->texture, NULL, &destination);',
              'SDL_RenderCopy(app->renderer, art->texture, &art->source_bounds, &destination);')
start=s.index('        fill_rect(r, tile.x, tile.y, tile.w, tile.h,', s.index('static void render_library_grid'))
end=s.index('    }\n    char position[50];',start)
s=s[:start]+'''        fill_rect(r, tile.x, tile.y, tile.w, tile.h, 24, 24, 24, 255);
        SDL_Rect image = companies ? (SDL_Rect){tile.x + 32, tile.y + 66, tile.w - 64, 144} :
                                    (SDL_Rect){tile.x + 16, tile.y + 14, tile.w - 32, 194};
        if (!render_library_artwork(app, companies ? brand->artwork_path : console->artwork_path, image)) {
            draw_fitted_text(r, app->font_row_title, companies ? brand->name : console->name,
                             image, white, image, true);
        }
        if (!companies) {
            SDL_Rect logo = {tile.x + 36, tile.y + 220, tile.w - 72, 36};
            render_library_artwork(app, console->logo_path, logo);
            SDL_Rect title = {tile.x + 16, tile.y + 264, tile.w - 32, 24};
            draw_fitted_text(r, app->font_micro, console->name, title, white, title, true);
        }
        char subtitle[100];
        snprintf(subtitle, sizeof(subtitle), TR(app, companies ? TXT_CONSOLE_COUNT : TXT_LIBRARY_GAME_COUNT),
                 companies ? brand->console_count : library_console_game_count(app, console));
        SDL_Rect footer = {tile.x + 24, tile.y + (companies ? 256 : 292), tile.w - 48, companies ? 32 : 20};
        draw_fitted_text(r, companies ? app->font_small : app->font_micro,
                         subtitle, footer, muted, footer, true);
        if (focused) draw_border(r, tile.x, tile.y, tile.w, tile.h, 3, 245, 245, 245, 255);
''' +s[end:]
p.write_text(s)

p=Path('src/library.c')
s=p.read_text()
s=re.sub(r'"assets/library/consoles/([^".]+)\.jpg"',r'"assets/library/consoles/\1.png"',s)
s=re.sub(r'(\{"[^"]+", (?:NULL|"[^"]+"), "assets/library/consoles/([^".]+)\.png")\}',
         lambda m:m[1]+', "assets/library/logos/'+m[2]+'.png"}',s)
p.write_text(s)
