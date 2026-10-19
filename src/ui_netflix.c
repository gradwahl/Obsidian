#include "wolodrive.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>

static const char *FONT_CANDIDATES_BOLD[] = {
    "assets/font_bold.ttf",
    "assets/font_cjk.ttc",
    "/data/homebrew/wolodrive/assets/font_bold.ttf",
    "/data/homebrew/wolodrive/assets/font_cjk.ttc",
    "/usr/share/fonts/opentype/noto/NotoSansCJK-Bold.ttc",
    "/usr/share/fonts/noto-cjk/NotoSansCJK-Bold.ttc",
    "C:/Windows/Fonts/msyhbd.ttc",
    "/mnt/c/Windows/Fonts/msyhbd.ttc",
    "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
    "/usr/share/fonts/TTF/DejaVuSans-Bold.ttf",
    "/usr/share/fonts/truetype/ubuntu/Ubuntu-B.ttf",
    "/usr/share/fonts/truetype/liberation/LiberationSans-Bold.ttf",
    "/system_ex/app/NPXS40028/psm/Application/resource/font.ttf",
    NULL
};

static const char *FONT_CANDIDATES_REGULAR[] = {
    "assets/font.ttf",
    "assets/font_cjk.ttc",
    "/data/homebrew/wolodrive/assets/font.ttf",
    "/data/homebrew/wolodrive/assets/font_cjk.ttc",
    "/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc",
    "/usr/share/fonts/noto-cjk/NotoSansCJK-Regular.ttc",
    "C:/Windows/Fonts/msyh.ttc",
    "/mnt/c/Windows/Fonts/msyh.ttc",
    "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
    "/usr/share/fonts/TTF/DejaVuSans.ttf",
    "/usr/share/fonts/truetype/ubuntu/Ubuntu-R.ttf",
    "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
    "/system_ex/app/NPXS40028/psm/Application/resource/font.ttf",
    NULL
};

static TTF_Font *open_first_available_font(const char **candidates, int ptsize) {
    /* Prefer one font covering Latin, Cyrillic and Chinese so switching needs no reload. */
    for (int require_cjk = 1; require_cjk >= 0; require_cjk--) {
        for (int i = 0; candidates[i] != NULL; i++) {
            TTF_Font *f = TTF_OpenFont(candidates[i], ptsize);
            if (!f) continue;
            if (!require_cjk || (TTF_GlyphIsProvided(f, 0x4e2d) && TTF_GlyphIsProvided(f, 0x042f))) return f;
            TTF_CloseFont(f);
        }
    }
    return NULL;
}

/* Menu order is independent of persisted language codes and enum values. */
static const Language LANGUAGE_ORDER[LANG_COUNT] = {
    LANG_PL, LANG_EN, LANG_FR, LANG_NL, LANG_DE, LANG_ES, LANG_IT, LANG_ZH, LANG_RU
};
enum { RAIL_WIDTH = 272, CONTENT_X = RAIL_WIDTH + 38, CARDS_Y = 600,
       CARD_WIDTH = 350, CARD_HEIGHT = 198, CARD_GAP = 12, CARD_FOOTER = 44 };
static const SDL_Rect LANGUAGE_BUTTON = {0, 24, RAIL_WIDTH, 90};
static const SDL_Rect LANGUAGE_MENU = {14, 122, RAIL_WIDTH - 28, LANG_COUNT * 44 + 16};
static const SDL_Rect DESCRIPTION_AREA = {CONTENT_X, 320, 690, 142};
static const SDL_Rect DESCRIPTION_BUTTON = {CONTENT_X, 478, 330, 42};
enum { LIBRARY_COLUMNS = 4, LIBRARY_PAGE_SIZE = 8, LIBRARY_TILE = 320,
       LIBRARY_GAP = 30, LIBRARY_GRID_Y = 220 };
static const SDL_Rect LIBRARY_BACK_BUTTON = {CONTENT_X, 24, 180, 50};

static SDL_Rect library_tile_target(int index) {
    int slot = index % LIBRARY_PAGE_SIZE;
    return (SDL_Rect){CONTENT_X + slot % LIBRARY_COLUMNS * (LIBRARY_TILE + LIBRARY_GAP),
                      LIBRARY_GRID_Y + slot / LIBRARY_COLUMNS * (LIBRARY_TILE + LIBRARY_GAP),
                      LIBRARY_TILE, LIBRARY_TILE};
}

static bool library_matches_console(WolodriveApp *app, const GameItem *item) {
    const LibraryConsole *console = library_console_at(app->library_company, app->library_console_selection);
    return item->is_game && library_console_matches_id(console, item->id_code);
}

static bool is_library_view(int view) {
    return view == VIEW_GAMES || view == VIEW_EMULATORS;
}

static const char *library_section_title(WolodriveApp *app) {
    return TR(app, app->active_row == VIEW_EMULATORS ? TXT_NAV_EMULATORS : TXT_NAV_GAMES);
}

static void select_sidebar_view(WolodriveApp *app, int view) {
    app->active_row = view;
    if (is_library_view(view)) {
        app->library_depth = LIBRARY_COMPANIES;
        app->library_sidebar_focus = true;
        ui_rebuild_carousels(app);
    }
}

static SDL_Rect sidebar_target(int row) {
    return (SDL_Rect){0, 240 + row * 90, RAIL_WIDTH, 90};
}

static SDL_Rect card_target(WolodriveApp *app, int col) {
    return (SDL_Rect){CONTENT_X + col * (CARD_WIDTH + CARD_GAP) -
                     (int)app->rows[app->active_row].scroll_x,
                     CARDS_Y, CARD_WIDTH, CARD_HEIGHT + CARD_FOOTER};
}

static bool contains_point(SDL_Rect rect, int x, int y) {
    SDL_Point point = {x, y};
    return SDL_PointInRect(&point, &rect);
}

static void toggle_language_menu(WolodriveApp *app) {
    app->language_menu_open = !app->language_menu_open;
    if (app->language_menu_open) {
        app->description_hovered = false;
        for (int i = 0; i < LANG_COUNT; i++) {
            if ((int)LANGUAGE_ORDER[i] == SDL_AtomicGet(&app->language)) {
                app->language_menu_index = i;
                break;
            }
        }
    }
}

static void select_menu_language(WolodriveApp *app) {
    Language language = LANGUAGE_ORDER[app->language_menu_index];
    app->language_menu_open = false;
    sys_set_language(app, language);
}

/* SDL mouse events already use renderer logical coordinates, including after resizing. */
static bool handle_language_input(WolodriveApp *app, const SDL_Event *ev) {
    if (ev->type == SDL_KEYDOWN && ev->key.keysym.sym == SDLK_l) {
        if (!ev->key.repeat) toggle_language_menu(app);
        return true;
    }
    if (ev->type == SDL_CONTROLLERBUTTONDOWN && ev->cbutton.button == SDL_CONTROLLER_BUTTON_X) {
        toggle_language_menu(app);
        return true;
    }
    if (ev->type == SDL_MOUSEBUTTONDOWN && ev->button.button == SDL_BUTTON_LEFT) {
        if (contains_point(LANGUAGE_BUTTON, ev->button.x, ev->button.y)) {
            toggle_language_menu(app);
            return true;
        }
        if (app->language_menu_open) {
            int index = (ev->button.y - LANGUAGE_MENU.y - 8) / 44;
            if (contains_point(LANGUAGE_MENU, ev->button.x, ev->button.y) &&
                ev->button.y >= LANGUAGE_MENU.y + 8 &&
                ev->button.y < LANGUAGE_MENU.y + 8 + LANG_COUNT * 44) {
                app->language_menu_index = index;
                select_menu_language(app);
            } else {
                app->language_menu_open = false;
            }
            return true;
        }
    }
    if (!app->language_menu_open) return false;
    if (ev->type == SDL_MOUSEMOTION && contains_point(LANGUAGE_MENU, ev->motion.x, ev->motion.y)) {
        int y = ev->motion.y - LANGUAGE_MENU.y - 8;
        if (y >= 0 && y < LANG_COUNT * 44) app->language_menu_index = y / 44;
    }
    if (ev->type == SDL_KEYDOWN) {
        switch (ev->key.keysym.sym) {
            case SDLK_UP: case SDLK_w:
                app->language_menu_index = (app->language_menu_index + LANG_COUNT - 1) % LANG_COUNT;
                break;
            case SDLK_DOWN: case SDLK_s:
                app->language_menu_index = (app->language_menu_index + 1) % LANG_COUNT;
                break;
            case SDLK_RETURN: case SDLK_SPACE:
                select_menu_language(app);
                break;
            case SDLK_ESCAPE: case SDLK_BACKSPACE: case SDLK_b:
                app->language_menu_open = false;
                break;
            default: break;
        }
    } else if (ev->type == SDL_CONTROLLERBUTTONDOWN) {
        switch (ev->cbutton.button) {
            case SDL_CONTROLLER_BUTTON_DPAD_UP:
                app->language_menu_index = (app->language_menu_index + LANG_COUNT - 1) % LANG_COUNT;
                break;
            case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
                app->language_menu_index = (app->language_menu_index + 1) % LANG_COUNT;
                break;
            case SDL_CONTROLLER_BUTTON_A:
                select_menu_language(app);
                break;
            case SDL_CONTROLLER_BUTTON_B:
                app->language_menu_open = false;
                break;
            default: break;
        }
    }
    return true; /* Keep catalog actions out of the open menu. */
}

static void fill_rect(SDL_Renderer *r, int x, int y, int w, int h,
                      uint8_t cr, uint8_t cg, uint8_t cb, uint8_t ca) {
    if (w <= 0 || h <= 0 || ca == 0) return;
    SDL_SetRenderDrawBlendMode(r, (ca == 255) ? SDL_BLENDMODE_NONE : SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, cr, cg, cb, ca);
    SDL_Rect rect = { x, y, w, h };
    SDL_RenderFillRect(r, &rect);
}

static void draw_border(SDL_Renderer *r, int x, int y, int w, int h, int thickness,
                        uint8_t cr, uint8_t cg, uint8_t cb, uint8_t ca) {
    fill_rect(r, x, y, w, thickness, cr, cg, cb, ca);
    fill_rect(r, x, y + h - thickness, w, thickness, cr, cg, cb, ca);
    fill_rect(r, x, y + thickness, thickness, h - 2 * thickness, cr, cg, cb, ca);
    fill_rect(r, x + w - thickness, y + thickness, thickness, h - 2 * thickness, cr, cg, cb, ca);
}

/* Pionowy gradient z obsługą przezroczystości (Glassmorphism / Cieniowanie okładki) */
static void fill_vertical_gradient(SDL_Renderer *r, int x, int y, int w, int h,
                                   uint8_t r1, uint8_t g1, uint8_t b1, uint8_t a1,
                                   uint8_t r2, uint8_t g2, uint8_t b2, uint8_t a2, int steps) {
    if (steps <= 0 || h <= 0 || w <= 0) return;
    /* Separate integer slices leave gaps when the renderer scales them. Draw
       one continuous surface so every pixel is covered exactly once. */
#if SDL_VERSION_ATLEAST(2, 0, 18)
    SDL_Color top = {r1, g1, b1, a1}, bottom = {r2, g2, b2, a2};
    SDL_Vertex vertices[] = {
        {{(float)x, (float)y}, top, {0, 0}},
        {{(float)(x + w), (float)y}, top, {0, 0}},
        {{(float)(x + w), (float)(y + h)}, bottom, {0, 0}},
        {{(float)x, (float)(y + h)}, bottom, {0, 0}}
    };
    const int indices[] = {0, 1, 2, 0, 2, 3};
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    if (SDL_RenderGeometry(r, NULL, vertices, 4, indices, 6) == 0) return;
#endif
    /* Compatibility path for older SDL builds or backends without geometry. */
    SDL_Surface *surface = SDL_CreateRGBSurfaceWithFormat(0, 1, h, 32, SDL_PIXELFORMAT_RGBA32);
    if (!surface) return;
    for (int i = 0; i < h; i++) {
        float t = h > 1 ? (float)i / (float)(h - 1) : 0.0f;
        Uint32 pixel = SDL_MapRGBA(surface->format,
            (Uint8)(r1 + (r2 - r1) * t), (Uint8)(g1 + (g2 - g1) * t),
            (Uint8)(b1 + (b2 - b1) * t), (Uint8)(a1 + (a2 - a1) * t));
        *(Uint32 *)((Uint8 *)surface->pixels + i * surface->pitch) = pixel;
    }
    SDL_Texture *texture = SDL_CreateTextureFromSurface(r, surface);
    SDL_FreeSurface(surface);
    if (!texture) return;
    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
    SDL_Rect destination = {x, y, w, h};
    SDL_RenderCopy(r, texture, NULL, &destination);
    SDL_DestroyTexture(texture);
}

/* Wielowarstwowy, miękki cień (Drop Shadow) pod kafelkiem */
static void draw_drop_shadow(SDL_Renderer *r, int x, int y, int w, int h,
                             int layers, int y_offset, uint8_t max_alpha) {
    for (int i = layers; i >= 1; i--) {
        float ratio = (float)(layers - i + 1) / (float)layers;
        uint8_t alpha = (uint8_t)(max_alpha * ratio * ratio * 0.45f);
        int spread = i * 2;
        fill_rect(r,
                  x - spread,
                  y + y_offset - spread / 2,
                  w + spread * 2,
                  h + spread * 2,
                  0, 0, 0, alpha);
    }
}

static int draw_text(SDL_Renderer *renderer, TTF_Font *font, const char *text,
                     int x, int y, SDL_Color color, int max_width) {
    if (!font || !text || text[0] == '\0') return 0;

    SDL_Surface *surf = NULL;
    if (max_width > 0) {
        surf = TTF_RenderUTF8_Blended_Wrapped(font, text, color, (Uint32)max_width);
    } else {
        surf = TTF_RenderUTF8_Blended(font, text, color);
    }
    if (!surf) return 0;

    int drawn_w = surf->w;
    SDL_Texture *tex = SDL_CreateTextureFromSurface(renderer, surf);
    if (tex) {
        SDL_Rect dst = { x, y, surf->w, surf->h };
        SDL_RenderCopy(renderer, tex, NULL, &dst);
        SDL_DestroyTexture(tex);
    }
    SDL_FreeSurface(surf);
    return drawn_w;
}

static void draw_single_line(SDL_Renderer *r, TTF_Font *font, const char *text,
                             int x, int y, SDL_Color color, SDL_Rect clip) {
    SDL_Rect previous;
    bool clipped = SDL_RenderIsClipEnabled(r);
    SDL_RenderGetClipRect(r, &previous);
    SDL_RenderSetClipRect(r, &clip);
    draw_text(r, font, text, x, y, color, 0);
    SDL_RenderSetClipRect(r, clipped ? &previous : NULL);
}

/* Keep translated navigation and action labels complete within their bounds. */
static void draw_fitted_text(SDL_Renderer *r, TTF_Font *font, const char *text,
                             SDL_Rect bounds, SDL_Color color, SDL_Rect clip, bool centered) {
    SDL_Surface *surface = TTF_RenderUTF8_Blended(font, text, color);
    if (!surface) return;
    SDL_Texture *texture = SDL_CreateTextureFromSurface(r, surface);
    if (texture) {
        float scale = fminf(1.0f, fminf((float)bounds.w / surface->w,
                                      (float)bounds.h / surface->h));
        int w = (int)(surface->w * scale), h = (int)(surface->h * scale);
        SDL_Rect destination = {bounds.x + (centered ? (bounds.w - w) / 2 : 0),
                                bounds.y + (bounds.h - h) / 2, w, h};
        SDL_Rect previous;
        bool clipped = SDL_RenderIsClipEnabled(r);
        SDL_RenderGetClipRect(r, &previous);
        SDL_RenderSetClipRect(r, &clip);
        SDL_RenderCopy(r, texture, NULL, &destination);
        SDL_RenderSetClipRect(r, clipped ? &previous : NULL);
        SDL_DestroyTexture(texture);
    }
    SDL_FreeSurface(surface);
}

static void ensure_cover_texture_loaded(WolodriveApp *app, GameItem *it) {
    if (!it || it->cover_tex != NULL || !it->cover_ready_on_disk || it->cover_load_attempted) {
        return;
    }
    it->cover_load_attempted = true;

    SDL_Surface *surf = IMG_Load(it->cover_local_path);
    if (surf) {
        it->cover_tex = SDL_CreateTextureFromSurface(app->renderer, surf);
        it->cover_w   = surf->w;
        it->cover_h   = surf->h;
        SDL_FreeSurface(surf);
    }
}

/* Renderuje teksturę okładki z zachowaniem proporcji (Center-Crop Cover Fit) lub stylowy Placeholder Wolodrive */
static void render_cover_or_placeholder(WolodriveApp *app, GameItem *it,
                                        int x, int y, int w, int h, bool is_hero) {
    SDL_Renderer *r = app->renderer;
    ensure_cover_texture_loaded(app, it);

    if (it->cover_tex && it->cover_w > 0 && it->cover_h > 0) {
        float dst_aspect = (float)w / (float)h;
        float src_aspect = (float)it->cover_w / (float)it->cover_h;
        SDL_Rect src_rect;

        if (src_aspect > dst_aspect) {
            /* Obraz szerszy niż kafelek – przytnij boki */
            src_rect.h = it->cover_h;
            src_rect.w = (int)(it->cover_h * dst_aspect);
            src_rect.x = (it->cover_w - src_rect.w) / 2;
            src_rect.y = 0;
        } else {
            /* Obraz wyższy niż kafelek (np. pionowy plakat PS5) – wytnij środek-górę */
            src_rect.w = it->cover_w;
            src_rect.h = (int)(it->cover_w / dst_aspect);
            src_rect.x = 0;
            src_rect.y = (it->cover_h - src_rect.h) / 4;
        }

        SDL_Rect dst_rect = { x, y, w, h };
        SDL_RenderCopy(r, it->cover_tex, &src_rect, &dst_rect);
    } else {
        /* Elegancki, stylowy Placeholder z logo WOLODRIVE i gradientem bazowym */
        fill_vertical_gradient(r, x, y, w, h,
                               it->cover_r, it->cover_g, it->cover_b, 255,
                               (uint8_t)(it->cover_r / 3), (uint8_t)(it->cover_g / 3), (uint8_t)(it->cover_b / 3), 255,
                               24);

        /* Geometryczny akcent w tle placeholdera */
        fill_rect(r, x + w - 90, y, 90, h, 255, 255, 255, 12);
        fill_rect(r, x, y + h / 2 - 1, w, 2, 255, 255, 255, 18);

        SDL_Color watermark_col = { 255, 255, 255, 75 };
        SDL_Color sub_col       = { 220, 220, 225, 160 };
        draw_text(r, is_hero ? app->font_row_title : app->font_small,
                  "WOLODRIVE", x + 18, y + h / 2 - (is_hero ? 28 : 22), watermark_col, 0);

        if (it->cover_url[0] != '\0' && !it->cover_ready_on_disk) {
            draw_text(r, app->font_micro, TR(app, TXT_COVER_LOADING), x + 18, y + h / 2 + 4, sub_col, 0);
        } else {
            draw_text(r, app->font_micro, it->title, x + 18, y + h / 2 + 4, sub_col, w - 36);
        }
    }
}

bool ui_create_renderer(WolodriveApp *app) {
    Uint32 flags = SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC;
#ifndef __PS5__
    /* WSLg can create an accelerated renderer successfully but present only black.
       An explicit SDL_RENDER_DRIVER remains available for driver testing. */
    if (!SDL_GetHint(SDL_HINT_RENDER_DRIVER) &&
        (getenv("WSL_DISTRO_NAME") || getenv("WSL_INTEROP"))) {
        flags = SDL_RENDERER_SOFTWARE;
    }
#endif
    app->renderer = SDL_CreateRenderer(app->window, -1, flags);
    if (!app->renderer && flags != SDL_RENDERER_SOFTWARE) {
        fprintf(stderr, "[WOLODRIVE WARN] Accelerated renderer: %s; trying software\n", SDL_GetError());
        app->renderer = SDL_CreateRenderer(app->window, -1, SDL_RENDERER_SOFTWARE);
    }
    if (!app->renderer) {
        fprintf(stderr, "[WOLODRIVE ERROR] SDL_CreateRenderer: %s\n", SDL_GetError());
        return false;
    }
    if (SDL_RenderSetLogicalSize(app->renderer, SCREEN_WIDTH, SCREEN_HEIGHT) != 0) {
        fprintf(stderr, "[WOLODRIVE ERROR] SDL_RenderSetLogicalSize: %s\n", SDL_GetError());
        return false;
    }
    SDL_RendererInfo info;
    if (SDL_GetRendererInfo(app->renderer, &info) == 0) {
        printf("[WOLODRIVE DISPLAY] Video: %s; renderer: %s\n",
               SDL_GetCurrentVideoDriver(), info.name);
    }
    return true;
}

bool ui_init(WolodriveApp *app) {
    app->font_logo       = open_first_available_font(FONT_CANDIDATES_BOLD, 32);
    app->font_hero_title = open_first_available_font(FONT_CANDIDATES_BOLD, 52);
    app->font_row_title  = open_first_available_font(FONT_CANDIDATES_BOLD, 28);
    app->font_body       = open_first_available_font(FONT_CANDIDATES_REGULAR, 26);
    app->font_small      = open_first_available_font(FONT_CANDIDATES_BOLD, 20);
    app->font_micro      = open_first_available_font(FONT_CANDIDATES_BOLD, 16);

    if (!app->font_logo || !app->font_hero_title || !app->font_row_title ||
        !app->font_body || !app->font_small || !app->font_micro) {
        fprintf(stderr, "[WOLODRIVE ERROR] UI fonts could not be loaded: %s\n", TTF_GetError());
        return false;
    }
    if (!TTF_GlyphIsProvided(app->font_body, 0x4e2d)) {
        fprintf(stderr, "[WOLODRIVE WARN] Install Noto Sans CJK or provide assets/font_cjk.ttc for Chinese text.\n");
    }

    app->active_row = 0;
    app->vertical_scroll_y = 0.0f;
    app->target_vertical_scroll_y = 0.0f;
    app->focus_pulse = 0.0f;

    ui_rebuild_carousels(app);
    return true;
}

void ui_cleanup(WolodriveApp *app) {
    catalog_free_textures(app);
    for (int i = 0; i < app->library_artwork_count; i++) {
        if (app->library_artwork[i].texture) SDL_DestroyTexture(app->library_artwork[i].texture);
    }
    app->library_artwork_count = 0;
    if (app->font_logo)       TTF_CloseFont(app->font_logo);
    if (app->font_hero_title) TTF_CloseFont(app->font_hero_title);
    if (app->font_row_title)  TTF_CloseFont(app->font_row_title);
    if (app->font_body)       TTF_CloseFont(app->font_body);
    if (app->font_small)      TTF_CloseFont(app->font_small);
    if (app->font_micro)      TTF_CloseFont(app->font_micro);
}

void ui_rebuild_carousels(WolodriveApp *app) {
    const char *titles[TOTAL_ROWS] = {
        TR(app, TXT_NAV_HOME),
        TR(app, TXT_NAV_GAMES),
        TR(app, TXT_NAV_EMULATORS),
        TR(app, TXT_NAV_FILES),
        TR(app, TXT_NAV_PAYLOADS),
        TR(app, TXT_NAV_DOWNLOADS)
    };

    SDL_LockMutex(app->catalog_mutex);

    CarouselRow *home = &app->rows[VIEW_HOME];
    GameItem *selected_home = home->selected_col > 0 && home->selected_col < home->item_count
                             ? home->items[home->selected_col] : NULL;
    for (int r = 0; r < TOTAL_ROWS; r++) {
        snprintf(app->rows[r].title, sizeof(app->rows[r].title), "%s", titles[r]);
        app->rows[r].item_count = 0;
    }
    home->item_count = catalog_latest_games_locked(app, home->items, MAX_ITEMS_PER_ROW);
    home->selected_col = 0;
    if (selected_home) {
        for (int i = 0; i < home->item_count; i++) {
            if (home->items[i] == selected_home) { home->selected_col = i; break; }
        }
    }

    for (int i = 0; i < app->catalog_count; i++) {
        GameItem *it = &app->catalog[i];

        /* The existing catalog has no reliable emulator/file/payload/ROM type.
           Keep those views explicit rather than guessing from package titles. */
        if ((app->active_row != VIEW_GAMES || app->library_depth != LIBRARY_GAMES || library_matches_console(app, it)) &&
            app->rows[VIEW_GAMES].item_count < MAX_ITEMS_PER_ROW) {
            app->rows[VIEW_GAMES].items[app->rows[VIEW_GAMES].item_count++] = it;
        }
        if ((it->state == STATE_DOWNLOADING || it->state == STATE_INSTALLED) &&
            app->rows[VIEW_DOWNLOADS].item_count < MAX_ITEMS_PER_ROW) {
            app->rows[VIEW_DOWNLOADS].items[app->rows[VIEW_DOWNLOADS].item_count++] = it;
        }
    }

    for (int r = 0; r < TOTAL_ROWS; r++) {
        if (app->rows[r].selected_col >= app->rows[r].item_count) {
            app->rows[r].selected_col = (app->rows[r].item_count > 0) ? app->rows[r].item_count - 1 : 0;
        }
    }

    app->catalog_dirty = false;
    SDL_UnlockMutex(app->catalog_mutex);
}

static GameItem *get_focused_item(WolodriveApp *app) {
    if (is_library_view(app->active_row) && app->library_depth != LIBRARY_GAMES) return NULL;
    CarouselRow *row = &app->rows[app->active_row];
    if (row->item_count <= 0) return NULL;
    return row->items[row->selected_col];
}

static void track_description_focus(WolodriveApp *app, GameItem *item) {
    if (app->description_item != item) {
        app->description_item = item;
        app->show_original_description = false;
        app->description_scroll = 0;
    }
}

static void description_text(WolodriveApp *app, GameItem *item, char *out, size_t capacity) {
    track_description_focus(app, item);
    /* Copy catalog text before handing it to the separate translation worker. */
    char original[MAX_DESCRIPTION_BYTES];
    SDL_LockMutex(app->catalog_mutex);
    TextId id = item->description_id;
    SDL_strlcpy(original, item->description, sizeof(original));
    SDL_UnlockMutex(app->catalog_mutex);
    if (id == TXT_DESCRIPTION_FALLBACK) {
        snprintf(out, capacity, TR(app, TXT_DESCRIPTION_FALLBACK), item->source_name, item->version);
    } else if (id != TEXT_COUNT) {
        SDL_strlcpy(out, TR(app, id), capacity);
    } else if (app->show_original_description ||
               (!app->translations && SDL_AtomicGet(&app->language) == LANG_EN)) {
        SDL_strlcpy(out, original, capacity);
    } else {
        TranslationStatus status = translation_get(app->translations, original,
            (Language)SDL_AtomicGet(&app->language), out, capacity);
        if (status != TRANSLATION_READY) {
            TextId message = status == TRANSLATION_UNCONFIGURED ? TXT_TRANSLATION_UNCONFIGURED :
                             status == TRANSLATION_FAILED ? TXT_TRANSLATION_FAILED : TXT_TRANSLATING;
            SDL_strlcpy(out, TR(app, message), capacity);
        }
    }
}

static void library_back(WolodriveApp *app) {
    if (app->library_depth == LIBRARY_GAMES) app->library_depth = LIBRARY_CONSOLES;
    else if (app->library_depth == LIBRARY_CONSOLES) app->library_depth = LIBRARY_COMPANIES;
    else app->library_sidebar_focus = true;
}

static void library_open_selected(WolodriveApp *app) {
    app->library_sidebar_focus = false;
    if (app->library_depth == LIBRARY_COMPANIES) {
        app->library_company = app->library_company_selection;
        app->library_console_selection = 0;
        app->library_depth = LIBRARY_CONSOLES;
    } else if (app->library_depth == LIBRARY_CONSOLES) {
        app->library_depth = LIBRARY_GAMES;
        CarouselRow *row = &app->rows[app->active_row];
        row->selected_col = 0;
        row->scroll_x = row->target_scroll_x = 0;
        ui_rebuild_carousels(app);
    }
}

static bool handle_library_input(WolodriveApp *app, const SDL_Event *ev) {
    if (!is_library_view(app->active_row)) return false;
    const LibraryCompany *company = library_company_at(app->library_company);
    int count = app->library_depth == LIBRARY_COMPANIES ? library_company_count() : company->console_count;
    int *selection = app->library_depth == LIBRARY_COMPANIES ?
                     &app->library_company_selection : &app->library_console_selection;
    if (ev->type == SDL_MOUSEBUTTONDOWN && ev->button.button == SDL_BUTTON_LEFT) {
        if (app->library_depth != LIBRARY_COMPANIES &&
            contains_point(LIBRARY_BACK_BUTTON, ev->button.x, ev->button.y)) {
            app->library_sidebar_focus = false;
            library_back(app);
            return true;
        }
        if (app->library_depth == LIBRARY_GAMES) {
            if (ev->button.x >= CONTENT_X) app->library_sidebar_focus = false;
            return false;
        }
        int page_start = *selection / LIBRARY_PAGE_SIZE * LIBRARY_PAGE_SIZE;
        for (int i = page_start; i < count && i < page_start + LIBRARY_PAGE_SIZE; i++) {
            if (contains_point(library_tile_target(i), ev->button.x, ev->button.y)) {
                *selection = i;
                library_open_selected(app);
                return true;
            }
        }
        return true;
    }
    int action = 0; /* left, right, up, down, open, back */
    if (ev->type == SDL_KEYDOWN) {
        switch (ev->key.keysym.sym) {
            case SDLK_LEFT: case SDLK_a: action = 1; break;
            case SDLK_RIGHT: case SDLK_d: action = 2; break;
            case SDLK_UP: case SDLK_w: action = 3; break;
            case SDLK_DOWN: case SDLK_s: action = 4; break;
            case SDLK_RETURN: case SDLK_SPACE: action = 5; break;
            case SDLK_ESCAPE: case SDLK_BACKSPACE: case SDLK_b: action = 6; break;
            default: break;
        }
        if (ev->key.repeat && action >= 5) return true;
    } else if (ev->type == SDL_CONTROLLERBUTTONDOWN) {
        switch (ev->cbutton.button) {
            case SDL_CONTROLLER_BUTTON_DPAD_LEFT: action = 1; break;
            case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: action = 2; break;
            case SDL_CONTROLLER_BUTTON_DPAD_UP: action = 3; break;
            case SDL_CONTROLLER_BUTTON_DPAD_DOWN: action = 4; break;
            case SDL_CONTROLLER_BUTTON_A: action = 5; break;
            case SDL_CONTROLLER_BUTTON_B: action = 6; break;
            default: break;
        }
    }
    if (action == 6) { library_back(app); return true; }
    if (app->library_sidebar_focus) {
        if (action == 2 || action == 5) { app->library_sidebar_focus = false; return true; }
        if (action == 1) return true;
        return false; /* Up/down still navigate the sidebar. */
    }
    if (app->library_depth == LIBRARY_GAMES) {
        if (action == 1 && app->rows[app->active_row].selected_col == 0) {
            app->library_sidebar_focus = true;
            return true;
        }
        return action == 3 || action == 4; /* Console games use a horizontal row. */
    }
    if (action == 1) {
        if (*selection % LIBRARY_COLUMNS == 0) app->library_sidebar_focus = true;
        else (*selection)--;
    } else if (action == 2) {
        if (*selection % LIBRARY_COLUMNS < LIBRARY_COLUMNS - 1 && *selection + 1 < count) (*selection)++;
    } else if (action == 3) {
        if (*selection >= LIBRARY_COLUMNS) *selection -= LIBRARY_COLUMNS;
        else app->library_sidebar_focus = true;
    } else if (action == 4) {
        if ((*selection / LIBRARY_COLUMNS + 1) * LIBRARY_COLUMNS < count)
            *selection = SDL_min(*selection + LIBRARY_COLUMNS, count - 1);
    } else if (action == 5) library_open_selected(app);
    return action != 0 || ev->type == SDL_MOUSEWHEEL;
}

void ui_handle_input(WolodriveApp *app, const SDL_Event *ev) {
    if (handle_language_input(app, ev)) return;
    if (ev->type == SDL_MOUSEBUTTONDOWN && ev->button.button == SDL_BUTTON_LEFT) {
        for (int r = 0; r < TOTAL_ROWS; r++) {
            if (contains_point(sidebar_target(r), ev->button.x, ev->button.y)) {
                select_sidebar_view(app, r);
                track_description_focus(app, get_focused_item(app));
                return;
            }
        }
    }
    if (handle_library_input(app, ev)) return;
    if (ev->type == SDL_MOUSEBUTTONDOWN && ev->button.button == SDL_BUTTON_LEFT) {
        CarouselRow *clicked_row = &app->rows[app->active_row];
        if (ev->button.x >= RAIL_WIDTH + 20) {
            for (int col = 0; col < clicked_row->item_count; col++) {
                SDL_Rect card = card_target(app, col);
                if (contains_point(card, ev->button.x, ev->button.y)) {
                    bool activate = clicked_row->selected_col == col &&
                                    ev->button.y >= CARDS_Y + CARD_HEIGHT;
                    clicked_row->selected_col = col;
                    track_description_focus(app, get_focused_item(app));
                    if (activate) sys_trigger_action(app, get_focused_item(app));
                    return;
                }
            }
        }
    }
    GameItem *focused = get_focused_item(app);
    track_description_focus(app, focused);
    if (ev->type == SDL_MOUSEMOTION) {
        app->description_hovered = contains_point(DESCRIPTION_AREA, ev->motion.x, ev->motion.y);
    }
    if ((ev->type == SDL_KEYDOWN && ev->key.keysym.sym == SDLK_i && !ev->key.repeat) ||
        (ev->type == SDL_CONTROLLERBUTTONDOWN && ev->cbutton.button == SDL_CONTROLLER_BUTTON_RIGHTSTICK) ||
        (ev->type == SDL_MOUSEBUTTONDOWN && ev->button.button == SDL_BUTTON_LEFT &&
         contains_point(DESCRIPTION_BUTTON, ev->button.x, ev->button.y))) {
        if (focused && focused->description_id == TEXT_COUNT) {
            app->show_original_description = !app->show_original_description;
            app->description_scroll = 0;
        }
        return;
    }
    if (ev->type == SDL_MOUSEWHEEL && app->description_hovered) {
        int direction = ev->wheel.direction == SDL_MOUSEWHEEL_FLIPPED ? -1 : 1;
        app->description_scroll = SDL_max(0, app->description_scroll - ev->wheel.y * direction * 24);
        return;
    }
    if (ev->type == SDL_KEYDOWN && (ev->key.keysym.sym == SDLK_PAGEUP || ev->key.keysym.sym == SDLK_PAGEDOWN)) {
        app->description_scroll = SDL_max(0, app->description_scroll + (ev->key.keysym.sym == SDLK_PAGEUP ? -48 : 48));
        return;
    }
    CarouselRow *row = &app->rows[app->active_row];

    if (ev->type == SDL_KEYDOWN) {
        switch (ev->key.keysym.sym) {
            case SDLK_LEFT:
            case SDLK_a:
                if (row->selected_col > 0) row->selected_col--;
                break;
            case SDLK_RIGHT:
            case SDLK_d:
                if (row->selected_col + 1 < row->item_count) row->selected_col++;
                break;
            case SDLK_UP:
            case SDLK_w:
                if (app->active_row > 0) select_sidebar_view(app, app->active_row - 1);
                break;
            case SDLK_DOWN:
            case SDLK_s:
                if (app->active_row + 1 < TOTAL_ROWS) {
                    select_sidebar_view(app, app->active_row + 1);
                }
                break;
            case SDLK_RETURN:
            case SDLK_SPACE:
                sys_trigger_action(app, get_focused_item(app));
                break;
            case SDLK_BACKSPACE:
            case SDLK_b:
                sys_cancel_action(app, get_focused_item(app));
                break;
            case SDLK_r:
            case SDLK_y:
            case SDLK_F5:
                catalog_start_async_sync(app);
                break;
            case SDLK_ESCAPE:
                app->running = false;
                break;
            default:
                break;
        }
    } else if (ev->type == SDL_CONTROLLERBUTTONDOWN) {
        switch (ev->cbutton.button) {
            case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
                if (row->selected_col > 0) row->selected_col--;
                break;
            case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
                if (row->selected_col + 1 < row->item_count) row->selected_col++;
                break;
            case SDL_CONTROLLER_BUTTON_DPAD_UP:
                if (app->active_row > 0) select_sidebar_view(app, app->active_row - 1);
                break;
            case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
                if (app->active_row + 1 < TOTAL_ROWS) {
                    select_sidebar_view(app, app->active_row + 1);
                }
                break;
            case SDL_CONTROLLER_BUTTON_A: /* [X] Cross na DualSense */
                sys_trigger_action(app, get_focused_item(app));
                break;
            case SDL_CONTROLLER_BUTTON_B: /* [O] Circle na DualSense */
                sys_cancel_action(app, get_focused_item(app));
                break;
            case SDL_CONTROLLER_BUTTON_Y: /* [Trójkąt] Triangle na DualSense */
                catalog_start_async_sync(app);
                break;
            default:
                break;
        }
    }
}

void ui_update_animations(WolodriveApp *app, float dt) {
    if (app->catalog_dirty) {
        ui_rebuild_carousels(app);
    }

    app->focus_pulse += dt;

    const float card_stride = CARD_WIDTH + CARD_GAP;
    for (int r = 0; r < TOTAL_ROWS; r++) {
        CarouselRow *row = &app->rows[r];
        /* Keep the focused card at the left edge, as in the TV interface. */
        row->target_scroll_x = row->selected_col * card_stride;
        row->scroll_x += (row->target_scroll_x - row->scroll_x) * fminf(1.0f, dt * 12.0f);
    }
}

/* A single texture avoids seams between translucent strips at scaled sizes. */
static void shade_hero(SDL_Renderer *r) {
    SDL_Surface *surface = SDL_CreateRGBSurfaceWithFormat(0, 256, 1, 32, SDL_PIXELFORMAT_RGBA32);
    if (!surface) return;
    for (int x = 0; x < 256; x++) {
        float t = (float)x / 255.0f;
        float fade = fmaxf(0.0f, (t - 0.28f) / 0.72f);
        Uint8 alpha = (Uint8)(245.0f * (1.0f - fade) * (1.0f - fade));
        ((Uint32 *)surface->pixels)[x] = SDL_MapRGBA(surface->format, 0, 0, 0, alpha);
    }
    SDL_Texture *texture = SDL_CreateTextureFromSurface(r, surface);
    SDL_FreeSurface(surface);
    if (!texture) return;
    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
    SDL_Rect destination = {RAIL_WIDTH, 0, SCREEN_WIDTH - RAIL_WIDTH, 720};
    SDL_RenderCopy(r, texture, NULL, &destination);
    SDL_DestroyTexture(texture);
}

static const char *action_label(WolodriveApp *app, GameItem *item) {
    if (item->state == STATE_DOWNLOADING) return TR(app, TXT_DOWNLOADING);
    if (item->state == STATE_INSTALLED)
        return TR(app, item->format == FMT_PKG ? TXT_INSTALLED : TXT_MOUNT);
    return TR(app, TXT_DOWNLOAD);
}

static const char *empty_view_message(WolodriveApp *app) {
    if (app->active_row == VIEW_HOME) return TR(app, TXT_NO_LATEST_GAMES);
    if (app->active_row == VIEW_EMULATORS && app->library_depth == LIBRARY_GAMES)
        return TR(app, TXT_EMULATOR_NOT_CONNECTED);
    if (app->active_row == VIEW_GAMES && app->library_depth == LIBRARY_GAMES) {
        const LibraryConsole *console = library_console_at(app->library_company, app->library_console_selection);
        if (console && !console->title_id_prefix) return TR(app, TXT_CONSOLE_NOT_CONNECTED);
    }
    if (app->active_row == VIEW_EMULATORS || app->active_row == VIEW_FILES ||
        app->active_row == VIEW_PAYLOADS) return TR(app, TXT_SECTION_UNAVAILABLE);
    return TR(app, app->active_row == VIEW_DOWNLOADS ? TXT_NO_DOWNLOADS : TXT_NO_PACKAGES);
}

static const char *view_content_title(WolodriveApp *app) {
    if (app->active_row == VIEW_HOME) return TR(app, TXT_LATEST_GAMES);
    if (is_library_view(app->active_row) && app->library_depth == LIBRARY_GAMES) {
        const LibraryConsole *console = library_console_at(app->library_company, app->library_console_selection);
        if (console) return console->name;
    }
    return app->rows[app->active_row].title;
}

static void render_hero_banner(WolodriveApp *app) {
    SDL_Renderer *r = app->renderer;
    GameItem *hero = get_focused_item(app);
    SDL_Color white = {245, 245, 245, 255};
    SDL_Color muted = {185, 185, 185, 255};
    if (!hero) {
        draw_text(r, app->font_hero_title, view_content_title(app),
                  CONTENT_X, 150, white, 900);
        draw_text(r, app->font_body, empty_view_message(app), CONTENT_X, 320, muted, 690);
        return;
    }

    render_cover_or_placeholder(app, hero, RAIL_WIDTH, 0, SCREEN_WIDTH - RAIL_WIDTH, 720, true);
    fill_rect(r, RAIL_WIDTH, 0, SCREEN_WIDTH - RAIL_WIDTH, 720, 0, 0, 0, 55);
    shade_hero(r);
    fill_vertical_gradient(r, RAIL_WIDTH, 450, SCREEN_WIDTH - RAIL_WIDTH, 300,
                           0, 0, 0, 0, 0, 0, 0, 255, 32);

    draw_text(r, app->font_logo, "W", CONTENT_X, 100, (SDL_Color){229, 9, 20, 255}, 0);
    draw_text(r, app->font_micro, "W O L O D R I V E", CONTENT_X + 45, 116, white, 0);
    /* Two title lines fit without displacing metadata or description. */
    SDL_Rect title_clip = {CONTENT_X, 142, 940, 136};
    SDL_RenderSetClipRect(r, &title_clip);
    draw_text(r, app->font_hero_title, hero->title, CONTENT_X, 142, white, title_clip.w);
    SDL_RenderSetClipRect(r, NULL);
    char metadata[256];
    snprintf(metadata, sizeof(metadata), "%s   v%s   %s   %.1f GB",
             hero->id_code, hero->version, sys_format_short_badge(hero->format), hero->size_gb);
    if (app->active_row == VIEW_HOME) {
        uint64_t date = hero->catalog_added_at / 1000000;
        snprintf(metadata, sizeof(metadata), "%s   %s   %.1f GB   %04u-%02u-%02u",
                 hero->id_code, sys_format_short_badge(hero->format), hero->size_gb,
                 (unsigned)(date / 10000), (unsigned)(date / 100 % 100), (unsigned)(date % 100));
    }
    draw_single_line(r, app->font_small, metadata, CONTENT_X, 276, muted,
                     (SDL_Rect){CONTENT_X, 276, 800, 34});
    char description[MAX_TRANSLATED_DESCRIPTION_BYTES];
    description_text(app, hero, description, sizeof(description));
    SDL_Surface *description_surface = TTF_RenderUTF8_Blended_Wrapped(app->font_body, description, muted, DESCRIPTION_AREA.w);
    if (description_surface) {
        int maximum_scroll = SDL_max(0, description_surface->h - DESCRIPTION_AREA.h);
        app->description_scroll = SDL_min(app->description_scroll, maximum_scroll);
        SDL_Texture *texture = SDL_CreateTextureFromSurface(r, description_surface);
        if (texture) {
            SDL_Rect destination = {DESCRIPTION_AREA.x, DESCRIPTION_AREA.y - app->description_scroll,
                                    description_surface->w, description_surface->h};
            SDL_RenderSetClipRect(r, &DESCRIPTION_AREA);
            SDL_RenderCopy(r, texture, NULL, &destination);
            SDL_RenderSetClipRect(r, NULL);
            SDL_DestroyTexture(texture);
        }
        if (maximum_scroll > 0) {
            fill_rect(r, DESCRIPTION_AREA.x + DESCRIPTION_AREA.w + 4, DESCRIPTION_AREA.y, 3, DESCRIPTION_AREA.h, 65, 65, 75, 255);
            int thumb_y = DESCRIPTION_AREA.y + (DESCRIPTION_AREA.h - 12) * app->description_scroll / maximum_scroll;
            fill_rect(r, DESCRIPTION_AREA.x + DESCRIPTION_AREA.w + 4, thumb_y, 3, 12, 180, 180, 190, 255);
        }
        SDL_FreeSurface(description_surface);
    }
    if (hero->description_id == TEXT_COUNT) {
        fill_rect(r, DESCRIPTION_BUTTON.x, DESCRIPTION_BUTTON.y, DESCRIPTION_BUTTON.w, DESCRIPTION_BUTTON.h, 35, 35, 43, 255);
        TextId label = app->show_original_description ? TXT_TRANSLATED_DESCRIPTION : TXT_ORIGINAL_DESCRIPTION;
        draw_single_line(r, app->font_small, TR(app, label), DESCRIPTION_BUTTON.x + 12,
                         DESCRIPTION_BUTTON.y + 10, white, DESCRIPTION_BUTTON);
    }

}

static void render_carousels(WolodriveApp *app) {
    SDL_Renderer *r = app->renderer;
    SDL_Color white = {245, 245, 245, 255}, muted = {170, 170, 170, 255};
    CarouselRow *row = &app->rows[app->active_row];
    draw_text(r, app->font_row_title, view_content_title(app), CONTENT_X, 552, white, 0);
    if (!row->item_count) {
        return;
    }
    SDL_Rect clip = {RAIL_WIDTH + 20, CARDS_Y - 5, SCREEN_WIDTH - RAIL_WIDTH - 20,
                     CARD_HEIGHT + CARD_FOOTER + 10};
    SDL_RenderSetClipRect(r, &clip);
    for (int col = 0; col < row->item_count; col++) {
        SDL_Rect card = card_target(app, col);
        if (card.x + card.w < clip.x || card.x >= SCREEN_WIDTH) continue;
        GameItem *item = row->items[col];
        bool selected = col == row->selected_col;
        render_cover_or_placeholder(app, item, card.x, card.y, card.w, CARD_HEIGHT, false);
        fill_vertical_gradient(r, card.x, card.y + CARD_HEIGHT - 70, card.w, 70,
                               0, 0, 0, 0, 0, 0, 0, 220, 16);
        draw_single_line(r, app->font_small, item->title, card.x + 12,
                         card.y + CARD_HEIGHT - 36, white,
                         (SDL_Rect){SDL_max(card.x + 12, clip.x), card.y + CARD_HEIGHT - 36,
                                    SDL_min(card.x + card.w - 12, SCREEN_WIDTH) - SDL_max(card.x + 12, clip.x), 32});
        /* draw_single_line restores the carousel clip. */
        draw_text(r, app->font_small, "W", card.x + 10, card.y + 8,
                  (SDL_Color){229, 9, 20, 255}, 0);
        int footer_y = card.y + CARD_HEIGHT;
        fill_rect(r, card.x, footer_y, card.w, CARD_FOOTER,
                  selected ? 240 : 0, selected ? 240 : 0, selected ? 240 : 0, 255);
        char status[256];
        if (selected) snprintf(status, sizeof(status), "[X] %s", action_label(app, item));
        else snprintf(status, sizeof(status), "%s  /  %.1f GB", sys_format_short_badge(item->format), item->size_gb);
        SDL_Rect footer_clip = {SDL_max(card.x + 8, clip.x), footer_y, 0, CARD_FOOTER};
        footer_clip.w = SDL_max(0, SDL_min(card.x + card.w - 8, SCREEN_WIDTH) - footer_clip.x);
        draw_fitted_text(r, app->font_small, status,
                         (SDL_Rect){card.x + 12, footer_y, card.w - 24, CARD_FOOTER},
                         selected ? (SDL_Color){0, 0, 0, 255} : muted, footer_clip, true);
        if (item->state == STATE_DOWNLOADING) {
            int progress = (int)(card.w * fminf(100.0f, fmaxf(0.0f, item->progress)) / 100.0f);
            fill_rect(r, card.x, footer_y - 4, progress, 4, 229, 9, 20, 255);
        }
        if (selected) draw_border(r, card.x - 4, card.y - 4, card.w + 8, card.h + 8,
                                  4, 240, 240, 240, 255);
    }
    SDL_RenderSetClipRect(r, NULL);
    char position[40];
    snprintf(position, sizeof(position), "%d / %d", row->selected_col + 1, row->item_count);
    draw_text(r, app->font_micro, position, CONTENT_X, 864, muted, 0);
}

/* Geometry icons remain available on PS5 and do not depend on font symbols. */
static void render_sidebar(WolodriveApp *app) {
    SDL_Renderer *r = app->renderer;
    fill_rect(r, 0, 0, RAIL_WIDTH, SCREEN_HEIGHT, 12, 12, 12, 255);
    fill_rect(r, RAIL_WIDTH - 1, 0, 1, SCREEN_HEIGHT, 48, 48, 48, 255);
    for (int row = 0; row < TOTAL_ROWS; row++) {
        SDL_Rect target = sidebar_target(row);
        bool selected = row == app->active_row;
        Uint8 c = selected ? 245 : 160;
        int x = 32, y = target.y + 28;
        if (selected) {
            fill_rect(r, 0, target.y, RAIL_WIDTH - 1, target.h, 25, 25, 25, 255);
            fill_rect(r, 0, target.y, 4, target.h, 229, 9, 20, 255);
        }
        SDL_Color color = {c, c, c, 255};
        SDL_Rect label = {84, target.y + 24, RAIL_WIDTH - 100, 42};
        draw_fitted_text(r, app->font_small, app->rows[row].title, label, color, label, false);
        if (row == VIEW_HOME) {
            SDL_SetRenderDrawColor(r, c, c, c, 255);
            SDL_RenderDrawLine(r, x - 2, y + 12, x + 14, y - 1);
            SDL_RenderDrawLine(r, x + 14, y - 1, x + 30, y + 12);
            draw_border(r, x + 2, y + 12, 24, 20, 2, c, c, c, 255);
            fill_rect(r, x + 11, y + 21, 7, 11, c, c, c, 255);
        } else if (row == VIEW_GAMES) {
            SDL_Point outline[] = {{x + 4, y + 5}, {x + 26, y + 5},
                {x + 30, y + 10}, {x + 34, y + 29}, {x + 27, y + 30},
                {x + 21, y + 23}, {x + 9, y + 23}, {x + 3, y + 30},
                {x - 4, y + 29}, {x, y + 10}, {x + 4, y + 5}};
            SDL_SetRenderDrawColor(r, c, c, c, 255);
            SDL_RenderDrawLines(r, outline, sizeof(outline) / sizeof(outline[0]));
            fill_rect(r, x + 4, y + 13, 10, 2, c, c, c, 255);
            fill_rect(r, x + 8, y + 9, 2, 10, c, c, c, 255);
            fill_rect(r, x + 22, y + 11, 3, 3, c, c, c, 255);
            fill_rect(r, x + 26, y + 16, 3, 3, c, c, c, 255);
        } else if (row == VIEW_EMULATORS) {
            draw_border(r, x, y + 25, 32, 8, 2, c, c, c, 255);
            fill_rect(r, x + 14, y + 11, 3, 14, c, c, c, 255);
            draw_border(r, x + 10, y + 2, 11, 11, 2, c, c, c, 255);
        } else if (row == VIEW_FILES) {
            SDL_Point folder[] = {{x, y + 7}, {x + 12, y + 7},
                {x + 17, y + 12}, {x + 32, y + 12}, {x + 32, y + 31},
                {x, y + 31}, {x, y + 7}};
            SDL_SetRenderDrawColor(r, c, c, c, 255);
            SDL_RenderDrawLines(r, folder, sizeof(folder) / sizeof(folder[0]));
            fill_rect(r, x + 2, y + 15, 28, 2, c, c, c, 255);
        } else if (row == VIEW_PAYLOADS) {
            draw_border(r, x + 6, y + 6, 22, 22, 2, c, c, c, 255);
            draw_border(r, x + 12, y + 12, 10, 10, 2, c, c, c, 255);
            for (int pin = 0; pin < 3; pin++) {
                int offset = 10 + pin * 6;
                fill_rect(r, x + offset, y + 1, 2, 5, c, c, c, 255);
                fill_rect(r, x + offset, y + 28, 2, 5, c, c, c, 255);
                fill_rect(r, x + 1, y + offset, 5, 2, c, c, c, 255);
                fill_rect(r, x + 28, y + offset, 5, 2, c, c, c, 255);
            }
        } else if (row == VIEW_DOWNLOADS) {
            fill_rect(r, x + 13, y, 3, 22, c, c, c, 255);
            SDL_SetRenderDrawColor(r, c, c, c, 255);
            SDL_RenderDrawLine(r, x + 6, y + 15, x + 14, y + 23);
            SDL_RenderDrawLine(r, x + 14, y + 23, x + 23, y + 15);
            fill_rect(r, x, y + 29, 30, 2, c, c, c, 255);
        }
    }
}



static void render_language_control(WolodriveApp *app) {
    SDL_Renderer *r = app->renderer;
    Uint8 c = app->language_menu_open ? 245 : 160;
    if (app->language_menu_open) {
        fill_rect(r, LANGUAGE_BUTTON.x, LANGUAGE_BUTTON.y, LANGUAGE_BUTTON.w - 1,
                  LANGUAGE_BUTTON.h, 25, 25, 25, 255);
        fill_rect(r, 0, LANGUAGE_BUTTON.y, 4, LANGUAGE_BUTTON.h, 229, 9, 20, 255);
    }
    /* Outline globe matches the sidebar icons without relying on a font glyph. */
    int cx = 48, cy = LANGUAGE_BUTTON.y + 44;
    SDL_SetRenderDrawColor(r, c, c, c, 255);
    for (int ring = 0; ring < 2; ring++) {
        SDL_Point points[33];
        for (int i = 0; i <= 32; i++) {
            float angle = (float)i * 6.2831853f / 32.0f;
            points[i] = (SDL_Point){cx + (int)lroundf(cosf(angle) * (ring ? 7 : 16)),
                                    cy + (int)lroundf(sinf(angle) * 16)};
        }
        SDL_RenderDrawLines(r, points, 33);
    }
    SDL_RenderDrawLine(r, cx - 16, cy, cx + 16, cy);
    SDL_RenderDrawLine(r, cx - 13, cy - 8, cx + 13, cy - 8);
    SDL_RenderDrawLine(r, cx - 13, cy + 8, cx + 13, cy + 8);
    SDL_Rect label = {84, LANGUAGE_BUTTON.y + 24, RAIL_WIDTH - 128, 42};
    draw_fitted_text(r, app->font_small,
                     i18n_language_name((Language)SDL_AtomicGet(&app->language)),
                     label, (SDL_Color){c, c, c, 255}, label, false);
    int arrow_x = RAIL_WIDTH - 30;
    int arrow_y = cy + (app->language_menu_open ? 4 : -4);
    int tip_y = cy + (app->language_menu_open ? -4 : 4);
    SDL_SetRenderDrawColor(r, c, c, c, 255);
    SDL_RenderDrawLine(r, arrow_x - 5, arrow_y, arrow_x, tip_y);
    SDL_RenderDrawLine(r, arrow_x, tip_y, arrow_x + 5, arrow_y);
}

static void render_top_bar(WolodriveApp *app) {
    SDL_Renderer *r = app->renderer;

    SDL_Color red = {229, 9, 20, 255};
    SDL_Color muted = {155, 155, 155, 255};
    draw_text(r, app->font_logo, "WOLODRIVE", 1620, 956, red, 0);
    draw_text(r, app->font_micro, "← →  ↑ ↓ / D-pad    X / Enter", CONTENT_X, 984, muted, 0);
    draw_single_line(r, app->font_micro, TR(app, TXT_BACK_HINT), CONTENT_X + 378, 984, muted,
                     (SDL_Rect){CONTENT_X + 378, 984, 260, 30});
    draw_single_line(r, app->font_micro, TR(app, TXT_REFRESH_HINT), CONTENT_X + 658, 984, muted,
                     (SDL_Rect){CONTENT_X + 658, 984, 600, 30});

    char sync_info[256];
    SDL_LockMutex(app->catalog_mutex);
    switch (app->sync_status_id) {
        case TXT_SYNC_SUBCATALOG:
            snprintf(sync_info, sizeof(sync_info), TR(app, TXT_SYNC_SUBCATALOG),
                     app->sync_current, app->sync_total);
            break;
        case TXT_SYNC_COVER:
            snprintf(sync_info, sizeof(sync_info), TR(app, TXT_SYNC_COVER),
                     app->sync_title, app->sync_current, app->sync_total);
            break;
        case TXT_SYNC_DONE:
            snprintf(sync_info, sizeof(sync_info), TR(app, TXT_SYNC_DONE),
                     app->catalog_count, app->sync_sources, app->sync_current, app->sync_total);
            break;
        default:
            snprintf(sync_info, sizeof(sync_info), "%s", TR(app, app->sync_status_id));
            break;
    }
    SDL_UnlockMutex(app->catalog_mutex);

    draw_single_line(r, app->font_micro, sync_info, CONTENT_X, 1032, muted,
                     (SDL_Rect){CONTENT_X, 1032, 880, 32});
    render_language_control(app);

    /* Prawa strona: /data, /mnt/ext0, status sieci i temperatura CPU/GPU */
    char telemetry[256];
    snprintf(telemetry, sizeof(telemetry),
             "/data: %.0f GB | ext0: %.0f GB | CPU: %d°C | GPU: %d°C",
             app->stats.data_free_gb,
             app->stats.ext0_free_gb,
             app->stats.cpu_temp_c,
             app->stats.gpu_temp_c);

    draw_single_line(r, app->font_micro, telemetry, 1200, 1032, muted,
                     (SDL_Rect){1200, 1032, 670, 32});
}

static void render_language_menu(WolodriveApp *app) {
    if (!app->language_menu_open) return;
    SDL_Renderer *r = app->renderer;
    SDL_Color white = {245, 245, 250, 255};
    SDL_Color muted = {170, 170, 180, 255};
    draw_drop_shadow(r, LANGUAGE_MENU.x, LANGUAGE_MENU.y, LANGUAGE_MENU.w, LANGUAGE_MENU.h, 8, 8, 220);
    fill_rect(r, LANGUAGE_MENU.x, LANGUAGE_MENU.y, LANGUAGE_MENU.w, LANGUAGE_MENU.h, 24, 24, 30, 255);
    draw_border(r, LANGUAGE_MENU.x, LANGUAGE_MENU.y, LANGUAGE_MENU.w, LANGUAGE_MENU.h, 1, 90, 90, 100, 255);
    for (int i = 0; i < LANG_COUNT; i++) {
        int y = LANGUAGE_MENU.y + 8 + i * 44;
        bool selected = SDL_AtomicGet(&app->language) == (int)LANGUAGE_ORDER[i];
        if (i == app->language_menu_index) {
            fill_rect(r, LANGUAGE_MENU.x + 4, y, LANGUAGE_MENU.w - 8, 44, 58, 58, 70, 255);
            draw_border(r, LANGUAGE_MENU.x + 4, y, LANGUAGE_MENU.w - 8, 44, 1, 229, 9, 20, 255);
        }
        if (selected) fill_rect(r, LANGUAGE_MENU.x + 14, y + 16, 10, 10, 229, 9, 20, 255);
        draw_text(r, app->font_body, i18n_language_name(LANGUAGE_ORDER[i]),
                  LANGUAGE_MENU.x + 38, y + 10, selected ? white : muted, 0);
    }
}

static void render_toast_notification(WolodriveApp *app) {
    if (SDL_GetTicks() >= app->toast_expire_tick || app->toast_message[0] == '\0') return;

    SDL_Renderer *r = app->renderer;
    SDL_Color white = { 255, 255, 255, 255 };

    int box_w = 660, box_h = 56;
    int box_x = SCREEN_WIDTH - box_w - 40;
    int box_y = TOPBAR_HEIGHT + 18;

    draw_drop_shadow(r, box_x, box_y, box_w, box_h, 8, 6, 200);
    fill_rect(r, box_x, box_y, box_w, box_h, 20, 20, 26, 245);
    fill_rect(r, box_x, box_y, 6, box_h, COLOR_ACCENT_R, COLOR_ACCENT_G, COLOR_ACCENT_B, 255);
    draw_border(r, box_x, box_y, box_w, box_h, 1, 255, 255, 255, 60);
    draw_text(r, app->font_small, app->toast_message, box_x + 22, box_y + 18, white, box_w - 36);
}

static int library_console_game_count(WolodriveApp *app, const LibraryConsole *console) {
    if (!console || !console->title_id_prefix) return 0;
    int count = 0;
    SDL_LockMutex(app->catalog_mutex);
    for (int i = 0; i < app->catalog_count; i++) {
        GameItem *item = &app->catalog[i];
        if (item->is_game && library_console_matches_id(console, item->id_code)) count++;
    }
    SDL_UnlockMutex(app->catalog_mutex);
    return count;
}

static void render_library_breadcrumb(WolodriveApp *app) {
    SDL_Renderer *r = app->renderer;
    SDL_Color muted = {180, 180, 180, 255};
    const LibraryCompany *company = library_company_at(app->library_company);
    if (app->library_depth == LIBRARY_COMPANIES) return;
    fill_rect(r, LIBRARY_BACK_BUTTON.x, LIBRARY_BACK_BUTTON.y,
              LIBRARY_BACK_BUTTON.w, LIBRARY_BACK_BUTTON.h, 28, 28, 28, 255);
    draw_fitted_text(r, app->font_small, TR(app, TXT_LIBRARY_BACK),
                     (SDL_Rect){CONTENT_X + 18, 28, 144, 42}, muted, LIBRARY_BACK_BUTTON, false);
    char path[200];
    const LibraryConsole *console = library_console_at(app->library_company, app->library_console_selection);
    snprintf(path, sizeof(path), "%s / %s%s%s", library_section_title(app), company->name,
             app->library_depth == LIBRARY_GAMES ? " / " : "",
             app->library_depth == LIBRARY_GAMES ? console->name : "");
    draw_single_line(r, app->font_small, path, CONTENT_X + 204, 36, muted,
                     (SDL_Rect){CONTENT_X + 204, 24, 1150, 50});
}

/* Native alpha defines the product bounds; hardware pixels are never keyed out. */
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

/* Console marks and Sony's black wordmark need white for contrast on charcoal.
   Other company artwork retains its original brand colors and internal detail. */
static void prepare_library_logo(SDL_Surface *surface, const char *path) {
    if (!strstr(path, "/logos/") && !strstr(path, "/companies/sony.png")) return;
    if (SDL_LockSurface(surface) != 0) return;
    for (int y = 0; y < surface->h; y++) {
        for (int x = 0; x < surface->w; x++) {
            Uint8 *pixel = (Uint8 *)surface->pixels + y * surface->pitch + x * 4;
            pixel[0] = pixel[1] = pixel[2] = 245;
        }
    }
    SDL_UnlockSurface(surface);
}

static LibraryArtwork *load_library_artwork(WolodriveApp *app, const char *path) {
    if (!path || !path[0]) return NULL;
    for (int i = 0; i < app->library_artwork_count; i++) {
        if (strcmp(app->library_artwork[i].path, path) == 0) return &app->library_artwork[i];
    }
    if (app->library_artwork_count >= MAX_LIBRARY_ARTWORK) return NULL;
    LibraryArtwork *art = &app->library_artwork[app->library_artwork_count++];
    art->path = path;
    SDL_Surface *surface = IMG_Load(path);
    char resolved[1024];
#ifdef __PS5__
    if (!surface) {
        snprintf(resolved, sizeof(resolved), "%s/%s", WOLODRIVE_DATA_PATH, path);
        surface = IMG_Load(resolved);
    }
#else
    /* Support running from the repo, beside a bundled assets directory, or
       from a different working directory with the executable in build/. */
    char *base = SDL_GetBasePath();
    if (!surface && base) {
        snprintf(resolved, sizeof(resolved), "%s%s", base, path);
        surface = IMG_Load(resolved);
        if (!surface) {
            snprintf(resolved, sizeof(resolved), "%s../%s", base, path);
            surface = IMG_Load(resolved);
        }
    }
    SDL_free(base);
#endif
    if (!surface) {
        fprintf(stderr, "[WOLODRIVE ARTWORK] Could not load %s: %s\n", path, IMG_GetError());
        return art; /* Cache failure too, preserving a usable text fallback. */
    }
    art->width = surface->w;
    art->height = surface->h;
    SDL_Surface *display = SDL_ConvertSurfaceFormat(surface, SDL_PIXELFORMAT_RGBA32, 0);
    if (display) {
        SDL_FreeSurface(surface);
        surface = display;
        prepare_library_logo(surface, path);
        art->source_bounds = library_alpha_bounds(surface);
    }
    if (!display) art->source_bounds = (SDL_Rect){0, 0, surface->w, surface->h};
    art->texture = SDL_CreateTextureFromSurface(app->renderer, surface);
    SDL_FreeSurface(surface);
    if (art->texture) SDL_SetTextureBlendMode(art->texture, SDL_BLENDMODE_BLEND);
    return art;
}

static bool render_library_artwork(WolodriveApp *app, const char *path, SDL_Rect bounds) {
    LibraryArtwork *art = load_library_artwork(app, path);
    if (!art || !art->texture || art->width <= 0 || art->height <= 0) return false;
    if (art->source_bounds.w <= 0 || art->source_bounds.h <= 0) return false;
    float scale = fminf((float)bounds.w / art->source_bounds.w, (float)bounds.h / art->source_bounds.h);
    int width = (int)(art->source_bounds.w * scale), height = (int)(art->source_bounds.h * scale);
    SDL_Rect destination = {bounds.x + (bounds.w - width) / 2,
                            bounds.y + (bounds.h - height) / 2, width, height};
    SDL_RenderCopy(app->renderer, art->texture, &art->source_bounds, &destination);
    return true;
}

static void render_library_grid(WolodriveApp *app) {
    SDL_Renderer *r = app->renderer;
    SDL_Color white = {245, 245, 245, 255}, muted = {165, 165, 165, 255};
    bool companies = app->library_depth == LIBRARY_COMPANIES;
    const LibraryCompany *company = library_company_at(app->library_company);
    int selected = companies ? app->library_company_selection : app->library_console_selection;
    int count = companies ? library_company_count() : company->console_count;
    int first = selected / LIBRARY_PAGE_SIZE * LIBRARY_PAGE_SIZE;
    draw_text(r, app->font_hero_title, companies ? library_section_title(app) : company->name,
              CONTENT_X, 100, white, 1300);
    draw_text(r, app->font_small, TR(app, companies ? TXT_CHOOSE_COMPANY : TXT_CHOOSE_CONSOLE),
              CONTENT_X, 178, muted, 1200);
    for (int i = first; i < count && i < first + LIBRARY_PAGE_SIZE; i++) {
        SDL_Rect tile = library_tile_target(i);
        const LibraryCompany *brand = companies ? library_company_at(i) : company;
        const LibraryConsole *console = companies ? NULL : library_console_at(app->library_company, i);
        bool focused = !app->library_sidebar_focus && i == selected;
        fill_rect(r, tile.x, tile.y, tile.w, tile.h, 24, 24, 24, 255);
        SDL_Rect image = companies ? (SDL_Rect){tile.x + 32, tile.y + 66, tile.w - 64, 144} :
                                    (SDL_Rect){tile.x + 16, tile.y + 14, tile.w - 32, 194};
        const char *artwork = companies ? brand->artwork_path :
            (console->artwork_path ? console->artwork_path : console->logo_path);
        if (!render_library_artwork(app, artwork, image)) {
            draw_fitted_text(r, app->font_row_title, companies ? brand->name : console->name,
                             image, white, image, true);
        }
        if (!companies) {
            SDL_Rect logo = {tile.x + 36, tile.y + 220, tile.w - 72, 36};
            if (console->artwork_path) render_library_artwork(app, console->logo_path, logo);
            SDL_Rect title = {tile.x + 16, tile.y + 264, tile.w - 32, 24};
            draw_fitted_text(r, app->font_micro, console->name, title, white, title, true);
        }
        char subtitle[100];
        TextId count_label = companies ? TXT_CONSOLE_COUNT :
            (app->active_row == VIEW_EMULATORS ? TXT_LIBRARY_EMULATOR_COUNT : TXT_LIBRARY_GAME_COUNT);
        snprintf(subtitle, sizeof(subtitle), TR(app, count_label), companies ? brand->console_count :
                 (app->active_row == VIEW_EMULATORS ? app->rows[VIEW_EMULATORS].item_count : library_console_game_count(app, console)));
        SDL_Rect footer = {tile.x + 24, tile.y + (companies ? 256 : 292), tile.w - 48, companies ? 32 : 20};
        draw_fitted_text(r, companies ? app->font_small : app->font_micro,
                         subtitle, footer, muted, footer, true);
        if (focused) draw_border(r, tile.x, tile.y, tile.w, tile.h, 3, 245, 245, 245, 255);
    }
    char position[50];
    snprintf(position, sizeof(position), "%d / %d", selected + 1, count);
    draw_text(r, app->font_micro, position, CONTENT_X, 910, muted, 0);
    draw_text(r, app->font_micro, TR(app, app->library_sidebar_focus ?
              TXT_LIBRARY_ENTER_HINT : TXT_LIBRARY_NAV_HINT), CONTENT_X, 944, muted, 1200);
    render_library_breadcrumb(app);
}

void ui_render(WolodriveApp *app) {
    SDL_Renderer *r = app->renderer;

    /* The presented backbuffer is undefined; clear before drawing scaled layers. */
    SDL_SetRenderDrawColor(r, 0, 0, 0, 255);
    SDL_RenderClear(r);

    if (is_library_view(app->active_row) && app->library_depth != LIBRARY_GAMES) {
        render_library_grid(app);
    } else {
        render_hero_banner(app);
        render_carousels(app);
        if (is_library_view(app->active_row)) render_library_breadcrumb(app);
    }
    render_sidebar(app);
    render_top_bar(app);
    render_toast_notification(app);
    render_language_menu(app);

    SDL_RenderPresent(r);
}
