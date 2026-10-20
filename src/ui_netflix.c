#include "wolodrive.h"
#include "artwork.h"
#include "screenshots.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include "ui_text.h"

static const char *FONT_CANDIDATES_BOLD[] = {
    "assets/font_bold.ttf",
    "assets/font_cjk.ttc",
    "assets/font.ttf",
    "/data/homebrew/wolodrive/assets/font_bold.ttf",
    "/data/homebrew/wolodrive/assets/font_cjk.ttc",
    "/data/homebrew/wolodrive/assets/font.ttf",
    "/system_ex/app/NPXS40028/psm/Application/resource/font.ttf",
    "/system/common/font/SST-Bold.otf",
    "/system/common/font/SST-Japanese-Bold.otf",
    "/system/common/font/SST-Medium.otf",
    "C:/Windows/Fonts/segoeuib.ttf",
    "C:/Windows/Fonts/arialbd.ttf",
    "C:/Windows/Fonts/msyhbd.ttc",
    "C:/Windows/Fonts/msyh.ttc",
    "C:/Windows/Fonts/tahomabd.ttf",
    "C:/Windows/Fonts/calibrib.ttf",
    "/mnt/c/Windows/Fonts/segoeuib.ttf",
    "/mnt/c/Windows/Fonts/arialbd.ttf",
    "/mnt/c/Windows/Fonts/msyhbd.ttc",
    "/mnt/c/Windows/Fonts/msyh.ttc",
    "/usr/share/fonts/opentype/noto/NotoSansCJK-Bold.ttc",
    "/usr/share/fonts/noto-cjk/NotoSansCJK-Bold.ttc",
    "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
    "/usr/share/fonts/TTF/DejaVuSans-Bold.ttf",
    "/usr/share/fonts/truetype/liberation/LiberationSans-Bold.ttf",
    "/usr/share/fonts/truetype/ubuntu/Ubuntu-B.ttf",
    "/usr/share/fonts/truetype/freefont/FreeSansBold.ttf",
    NULL
};

static const char *FONT_CANDIDATES_REGULAR[] = {
    "assets/font.ttf",
    "assets/font_cjk.ttc",
    "assets/font_bold.ttf",
    "/data/homebrew/wolodrive/assets/font.ttf",
    "/data/homebrew/wolodrive/assets/font_cjk.ttc",
    "/data/homebrew/wolodrive/assets/font_bold.ttf",
    "/system_ex/app/NPXS40028/psm/Application/resource/font.ttf",
    "/system/common/font/SST-Medium.otf",
    "/system/common/font/SST-Japanese-Medium.otf",
    "/system/common/font/SST-Light.otf",
    "C:/Windows/Fonts/segoeui.ttf",
    "C:/Windows/Fonts/arial.ttf",
    "C:/Windows/Fonts/msyh.ttc",
    "C:/Windows/Fonts/tahoma.ttf",
    "C:/Windows/Fonts/calibri.ttf",
    "/mnt/c/Windows/Fonts/segoeui.ttf",
    "/mnt/c/Windows/Fonts/arial.ttf",
    "/mnt/c/Windows/Fonts/msyh.ttc",
    "/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc",
    "/usr/share/fonts/noto-cjk/NotoSansCJK-Regular.ttc",
    "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
    "/usr/share/fonts/TTF/DejaVuSans.ttf",
    "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
    "/usr/share/fonts/truetype/ubuntu/Ubuntu-R.ttf",
    "/usr/share/fonts/truetype/freefont/FreeSans.ttf",
    NULL
};

static TTF_Font *open_font_with_path_resolution(const char *candidate, int ptsize) {
    if (!candidate || !candidate[0]) return NULL;
    TTF_Font *f = TTF_OpenFont(candidate, ptsize);
    if (f) return f;

    if (candidate[0] != '/' && candidate[1] != ':') {
        char *base = SDL_GetBasePath();
        if (base) {
            char resolved[1024];
            snprintf(resolved, sizeof(resolved), "%s%s", base, candidate);
            f = TTF_OpenFont(resolved, ptsize);
            if (!f) {
                snprintf(resolved, sizeof(resolved), "%s../%s", base, candidate);
                f = TTF_OpenFont(resolved, ptsize);
            }
            SDL_free(base);
            if (f) return f;
        }
    }
#ifdef _WIN32
    const char *windir = getenv("WINDIR");
    if (!windir) windir = getenv("SystemRoot");
    if (windir && windir[0]) {
        char resolved[1024];
        const char *fname = strrchr(candidate, '/');
        if (!fname) fname = strrchr(candidate, '\');
        fname = fname ? fname + 1 : candidate;
        snprintf(resolved, sizeof(resolved), "%s\Fonts\%s", windir, fname);
        f = TTF_OpenFont(resolved, ptsize);
        if (f) return f;
    }
#endif
    return NULL;
}

static TTF_Font *open_first_available_font(const char **candidates, int ptsize) {
    for (int i = 0; candidates[i] != NULL; i++) {
        TTF_Font *f = open_font_with_path_resolution(candidates[i], ptsize);
        if (!f) continue;
        if (TTF_GlyphIsProvided(f, 0x4e2d) && TTF_GlyphIsProvided(f, 0x042f)) return f;
        TTF_CloseFont(f);
    }
    for (int i = 0; candidates[i] != NULL; i++) {
        TTF_Font *f = open_font_with_path_resolution(candidates[i], ptsize);
        if (!f) continue;
        if (TTF_GlyphIsProvided(f, 0x042f)) return f;
        TTF_CloseFont(f);
    }
    for (int i = 0; candidates[i] != NULL; i++) {
        TTF_Font *f = open_font_with_path_resolution(candidates[i], ptsize);
        if (f) return f;
    }
    return NULL;
}

/* Menu order is independent of persisted language codes and enum values. */
static const Language LANGUAGE_ORDER[LANG_COUNT] = {
    LANG_PL, LANG_EN, LANG_FR, LANG_NL, LANG_DE, LANG_ES, LANG_IT, LANG_ZH, LANG_RU
};
enum { SIDEBAR_COLLAPSED_W = 92, RAIL_WIDTH = 92, SIDEBAR_EXPANDED_W = 272,
       CONTENT_X = RAIL_WIDTH + 38, CARDS_Y = 576,
       CARD_WIDTH = 216, CARD_HEIGHT = 300, CARD_GAP = 16, CARD_TITLE = 28, CARD_FOOTER = 40,
       HOME_ROW_STRIDE = 442, HOME_GRID_COLS = 6 };
static const SDL_Rect LANGUAGE_BUTTON = {0, 24, SIDEBAR_EXPANDED_W, 90};
static const SDL_Rect LANGUAGE_MENU = {14, 122, SIDEBAR_EXPANDED_W - 28, LANG_COUNT * 44 + 16};
static const SDL_Rect DESCRIPTION_AREA = {CONTENT_X, 320, 690, 142};
static const SDL_Rect DESCRIPTION_BUTTON = {CONTENT_X, 478, 330, 42};
static const SDL_Rect HERO_PLAY_BUTTON = {CONTENT_X + 344, 478, 206, 42};
static const SDL_Rect HERO_INFO_BUTTON = {CONTENT_X + 562, 478, 236, 42};
static const SDL_Rect HOME_SHOW_ALL_BUTTON = {CONTENT_X, 524, 560, 44};
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
    app->sidebar_focus = true;
    if (view == VIEW_HOME) {
        app->home_cat_index = 0;
        app->home_grid_expanded = false;
    }
    if (view == VIEW_SEARCH) SDL_StartTextInput();
    else SDL_StopTextInput();
    if (is_library_view(view)) {
        app->library_depth = LIBRARY_COMPANIES;
        ui_rebuild_carousels(app);
    }
}

static SDL_Rect sidebar_target(int row) {
    int position = row == VIEW_SETTINGS ? VIEW_DOWNLOADS+1 : row;
    return (SDL_Rect){0, row == VIEW_SEARCH ? 150 : 240 + position * 90, SIDEBAR_EXPANDED_W, 90};
}

static CarouselRow *active_home_row(WolodriveApp *app) {
    if (app->active_row == VIEW_HOME && app->home_cat_index > 0 &&
        app->home_cat_index < HOME_CAT_COUNT &&
        app->home_categories[app->home_cat_index].item_count > 0) {
        return &app->home_categories[app->home_cat_index];
    }
    return &app->rows[app->active_row];
}

static SDL_Rect card_target(WolodriveApp *app, int col) {
    CarouselRow *row = active_home_row(app);
    return (SDL_Rect){CONTENT_X + col * (CARD_WIDTH + CARD_GAP) -
                     (int)row->scroll_x,
                     CARDS_Y, CARD_WIDTH, CARD_HEIGHT + CARD_TITLE + CARD_FOOTER};
}

static SDL_Rect home_row_card_target(WolodriveApp *app, int cat, int col) {
    CarouselRow *row = &app->home_categories[cat];
    int row_y = CARDS_Y + cat * HOME_ROW_STRIDE - (int)app->home_vertical_scroll;
    return (SDL_Rect){CONTENT_X + col * (CARD_WIDTH + CARD_GAP) - (int)row->scroll_x,
                      row_y, CARD_WIDTH, CARD_HEIGHT + CARD_TITLE + CARD_FOOTER};
}

static SDL_Rect home_grid_card_target(WolodriveApp *app, int index) {
    int col = index % HOME_GRID_COLS;
    int row = index / HOME_GRID_COLS - app->home_grid_scroll_row;
    int cell_h = CARD_HEIGHT + CARD_TITLE + CARD_FOOTER + 22;
    return (SDL_Rect){CONTENT_X + col * (CARD_WIDTH + CARD_GAP),
                      128 + row * cell_h - (int)app->home_vertical_scroll,
                      CARD_WIDTH, CARD_HEIGHT + CARD_TITLE + CARD_FOOTER};
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
    if (ev->type == SDL_KEYDOWN && ev->key.keysym.sym == SDLK_l &&
        (app->active_row != VIEW_SEARCH || app->sidebar_focus || app->language_menu_open)) {
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

static int draw_text(WolodriveApp *app, TTF_Font *font, const char *text,
                     int x, int y, SDL_Color color, int max_width) {
    UiTextEntry *entry = cached_text(app, font, text, color, max_width);
    if (!entry) return 0;
    SDL_Rect dst = {x, y, entry->w, entry->h};
    SDL_RenderCopy(app->renderer, entry->texture, NULL, &dst);
    return entry->w;
}

static void draw_single_line(WolodriveApp *app, TTF_Font *font, const char *text,
                             int x, int y, SDL_Color color, SDL_Rect clip) {
    SDL_Renderer *r = app->renderer;
    SDL_Rect previous;
    bool clipped = SDL_RenderIsClipEnabled(r);
    SDL_RenderGetClipRect(r, &previous);
    SDL_RenderSetClipRect(r, &clip);
    draw_text(app, font, text, x, y, color, 0);
    SDL_RenderSetClipRect(r, clipped ? &previous : NULL);
}

/* Keep translated navigation and action labels complete within their bounds. */
static void draw_fitted_text(WolodriveApp *app, TTF_Font *font, const char *text,
                             SDL_Rect bounds, SDL_Color color, SDL_Rect clip, bool centered) {
    SDL_Renderer *r = app->renderer;
    UiTextEntry *entry = cached_text(app, font, text, color, 0);
    if (!entry) return;
    {
        float scale = fminf(1.0f, fminf((float)bounds.w / entry->w,
                                      (float)bounds.h / entry->h));
        int w = (int)(entry->w * scale), h = (int)(entry->h * scale);
        SDL_Rect destination = {bounds.x + (centered ? (bounds.w - w) / 2 : 0),
                                bounds.y + (bounds.h - h) / 2, w, h};
        SDL_Rect previous;
        bool clipped = SDL_RenderIsClipEnabled(r);
        SDL_RenderGetClipRect(r, &previous);
        SDL_RenderSetClipRect(r, &clip);
        SDL_RenderCopy(r, entry->texture, NULL, &destination);
        SDL_RenderSetClipRect(r, clipped ? &previous : NULL);
    }
}

static int render_box_brand(WolodriveApp *app, const GameItem *item, SDL_Rect box) {
    bool ps5 = SDL_strncasecmp(item->id_code, "PPSA", 4) == 0;
    bool ps4 = SDL_strncasecmp(item->id_code, "CUSA", 4) == 0;
    if (!ps5 && !ps4) return 0;
    int band = SDL_max(18, SDL_min(32, box.h/9));
    fill_rect(app->renderer, box.x, box.y, box.w, band,
              ps5 ? 245 : 0, ps5 ? 245 : 62, ps5 ? 245 : 155, 255);
    SDL_Rect brand = {box.x+8, box.y, box.w-16, band};
    draw_fitted_text(app, app->font_small, ps5 ? "PS5" : "PS4", brand,
                     ps5 ? (SDL_Color){12,12,16,255} : (SDL_Color){255,255,255,255}, brand, false);
    return band;
}

/* Zaokrąglenie rogów pionowego kafelka okładki (proporcje pudełkowe) */
static void draw_rounded_card_corners(SDL_Renderer *r, int x, int y, int w, int h,
                                      uint8_t br, uint8_t bg, uint8_t bb) {
    static const int trim[6] = { 6, 4, 3, 2, 1, 1 };
    for (int row = 0; row < 6; row++) {
        int cw = trim[row];
        fill_rect(r, x, y + row, cw, 1, br, bg, bb, 255);
        fill_rect(r, x + w - cw, y + row, cw, 1, br, bg, bb, 255);
        fill_rect(r, x, y + h - 1 - row, cw, 1, br, bg, bb, 255);
        fill_rect(r, x + w - cw, y + h - 1 - row, cw, 1, br, bg, bb, 255);
    }
}

static void render_cover_or_placeholder_alpha(WolodriveApp *app, GameItem *it,
                                              int x, int y, int w, int h, bool is_hero, Uint8 alpha) {
    SDL_Renderer *r = app->renderer;
    int width = 0, height = 0;
    SDL_Texture *texture = artwork_get(app, it, is_hero, is_hero ? 100 : 80, &width, &height);
    if (!texture && is_hero) texture = artwork_get(app, it, false, 100, &width, &height);
    if (texture && width > 0 && height > 0) {
        SDL_Rect box = {x, y, w, h};
        SDL_Color color = artwork_background(app, texture);
        fill_vertical_gradient(r, x, y, w, h, color.r/3, color.g/3, color.b/3, alpha,
                               color.r/8, color.g/8, color.b/8, alpha, 24);
        bool complete_box = !is_hero && artwork_complete_box(app, texture);
        if (!is_hero && !complete_box) {
            int band = render_box_brand(app, it, box);
            box.y += band; box.h -= band;
        }
        /* Fill the case below its platform strip without letterboxing. */
        SDL_Rect destination = artwork_fit(width, height, box, true);
        SDL_Rect previous;
        bool clipped = SDL_RenderIsClipEnabled(r);
        SDL_RenderGetClipRect(r, &previous);
        SDL_Rect intersection = box;
        if (clipped) SDL_IntersectRect(&box, &previous, &intersection);
        SDL_RenderSetClipRect(r, &intersection);
        SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
        SDL_SetTextureAlphaMod(texture, alpha);
        SDL_RenderCopy(r, texture, NULL, &destination);
        SDL_SetTextureAlphaMod(texture, 255);
        if (is_hero) fill_rect(r, x, y, w, h, 0, 0, 0, (Uint8)((70 * alpha) / 255));
        SDL_RenderSetClipRect(r, clipped ? &previous : NULL);
    } else {
        /* Elegancki, stylowy Placeholder z logo WOLODRIVE i gradientem bazowym */
        fill_vertical_gradient(r, x, y, w, h,
                               it->cover_r, it->cover_g, it->cover_b, alpha,
                               (uint8_t)(it->cover_r / 3), (uint8_t)(it->cover_g / 3), (uint8_t)(it->cover_b / 3), alpha,
                               24);

        /* Geometryczny akcent w tle placeholdera */
        fill_rect(r, x + w - 90, y, 90, h, 255, 255, 255, (Uint8)((12 * alpha) / 255));
        fill_rect(r, x, y + h / 2 - 1, w, 2, 255, 255, 255, (Uint8)((18 * alpha) / 255));

        SDL_Color watermark_col = { 255, 255, 255, (Uint8)((75 * alpha) / 255) };
        SDL_Color sub_col       = { 220, 220, 225, (Uint8)((160 * alpha) / 255) };
        draw_text(app, is_hero ? app->font_row_title : app->font_small,
                  "WOLODRIVE", x + 18, y + h / 2 - (is_hero ? 28 : 22), watermark_col, 0);

        draw_text(app, app->font_micro, it->title, x + 18, y + h / 2 + 4, sub_col, w - 36);
        if (!is_hero) render_box_brand(app, it, (SDL_Rect){x,y,w,h});
    }
    if (!is_hero) draw_border(r, x, y, w, h, 2, 22, 62, 110, alpha);
}

static void render_cover_or_placeholder(WolodriveApp *app, GameItem *it,
                                        int x, int y, int w, int h, bool is_hero) {
    render_cover_or_placeholder_alpha(app, it, x, y, w, h, is_hero, 255);
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
    app->vsync_enabled = false;
    if (SDL_GetRendererInfo(app->renderer, &info) == 0) {
        app->vsync_enabled = (info.flags & SDL_RENDERER_PRESENTVSYNC) != 0;
        printf("[WOLODRIVE DISPLAY] Video: %s; renderer: %s\n",
               SDL_GetCurrentVideoDriver(), info.name);
    }
    /* Also request VSync on the final renderer, including the WSL/software
       path. Unsupported backends retain the independent 60 FPS frame cap. */
#if SDL_VERSION_ATLEAST(2, 0, 18)
    if (SDL_RenderSetVSync(app->renderer, 1) == 0) app->vsync_enabled = true;
#endif
    printf("[WOLODRIVE DISPLAY] VSync: %s; frame limit: 60 FPS\n",
           app->vsync_enabled ? "enabled" : "unavailable (using frame pacing)");
    return true;
}

bool ui_init(WolodriveApp *app) {
    app->font_logo       = open_first_available_font(FONT_CANDIDATES_BOLD, 32);
    app->font_hero_title = open_first_available_font(FONT_CANDIDATES_BOLD, 52);
    app->font_row_title  = open_first_available_font(FONT_CANDIDATES_BOLD, 28);
    app->font_body       = open_first_available_font(FONT_CANDIDATES_REGULAR, 26);
    app->font_small      = open_first_available_font(FONT_CANDIDATES_BOLD, 20);
    app->font_micro      = open_first_available_font(FONT_CANDIDATES_BOLD, 16);

    TTF_Font *fallback_font = app->font_body ? app->font_body :
                              app->font_logo ? app->font_logo :
                              app->font_hero_title ? app->font_hero_title :
                              app->font_row_title ? app->font_row_title :
                              app->font_small ? app->font_small : app->font_micro;
    if (fallback_font) {
        if (!app->font_logo) app->font_logo = fallback_font;
        if (!app->font_hero_title) app->font_hero_title = fallback_font;
        if (!app->font_row_title) app->font_row_title = fallback_font;
        if (!app->font_body) app->font_body = fallback_font;
        if (!app->font_small) app->font_small = fallback_font;
        if (!app->font_micro) app->font_micro = fallback_font;
    } else {
        fprintf(stderr, "[WOLODRIVE ERROR] UI fonts could not be loaded: %s
", TTF_GetError());
        return false;
    }
    if (!TTF_GlyphIsProvided(app->font_body, 0x4e2d)) {
        fprintf(stderr, "[WOLODRIVE WARN] Install Noto Sans CJK or provide assets/font_cjk.ttc for Chinese text.\n");
    }

    app->active_row = 0;
    app->home_cat_index = 0;
    app->home_grid_expanded = false;
    app->home_grid_anim = 0.0f;
    app->sidebar_width = (float)SIDEBAR_COLLAPSED_W;
    app->sidebar_dim_alpha = 0.0f;
    app->hero_crossfade = 1.0f;
    app->card_focus_scale = 1.0f;
    app->vertical_scroll_y = 0.0f;
    app->target_vertical_scroll_y = 0.0f;
    app->focus_pulse = 0.0f;

    ui_rebuild_carousels(app);
    return true;
}

void ui_cleanup(WolodriveApp *app) {
    text_cache_destroy(app);
    screenshots_destroy(app->screenshots); app->screenshots = NULL;
    artwork_destroy(app);
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

static void search_rebuild_locked(WolodriveApp *app);

static int cmp_popular_desc(const void *a, const void *b) {
    const GameItem *ga = *(const GameItem *const *)a;
    const GameItem *gb = *(const GameItem *const *)b;
    if (gb->download_count != ga->download_count)
        return (gb->download_count > ga->download_count) - (gb->download_count < ga->download_count);
    return SDL_strcasecmp(ga->title, gb->title);
}

static int cmp_newest_desc(const void *a, const void *b) {
    const GameItem *ga = *(const GameItem *const *)a;
    const GameItem *gb = *(const GameItem *const *)b;
    int d = strcmp(gb->release_date, ga->release_date);
    if (d != 0) return d;
    if (gb->catalog_added_at != ga->catalog_added_at)
        return (gb->catalog_added_at > ga->catalog_added_at) - (gb->catalog_added_at < ga->catalog_added_at);
    return SDL_strcasecmp(ga->title, gb->title);
}

static int cmp_rating_desc(const void *a, const void *b) {
    const GameItem *ga = *(const GameItem *const *)a;
    const GameItem *gb = *(const GameItem *const *)b;
    if (fabsf(gb->rating_score - ga->rating_score) > 0.001f)
        return (gb->rating_score > ga->rating_score) ? 1 : -1;
    return (gb->download_count > ga->download_count) - (gb->download_count < ga->download_count);
}

void ui_rebuild_carousels(WolodriveApp *app) {
    const char *titles[TOTAL_ROWS] = {
        TR(app, TXT_NAV_HOME),
        TR(app, TXT_NAV_GAMES),
        TR(app, TXT_NAV_EMULATORS),
        TR(app, TXT_NAV_FILES),
        TR(app, TXT_NAV_PAYLOADS),
        TR(app, TXT_NAV_DOWNLOADS),
        TR(app, TXT_NAV_SEARCH),
        TR(app, TXT_NAV_SETTINGS)
    };
    const char *cat_titles[HOME_CAT_COUNT] = {
        TR(app, TXT_CAT_PS5),
        TR(app, TXT_CAT_PS4),
        TR(app, TXT_CAT_POPULAR),
        TR(app, TXT_CAT_NEWEST),
        TR(app, TXT_CAT_TOP_RATED)
    };

    SDL_LockMutex(app->catalog_mutex);

    CarouselRow *home = &app->rows[VIEW_HOME];
    GameItem *selected_home = home->selected_col > 0 && home->selected_col < home->item_count
                             ? home->items[home->selected_col] : NULL;
    for (int r = 0; r < TOTAL_ROWS; r++) {
        snprintf(app->rows[r].title, sizeof(app->rows[r].title), "%s", titles[r]);
        app->rows[r].item_count = 0;
    }
    for (int c = 0; c < HOME_CAT_COUNT; c++) {
        snprintf(app->home_categories[c].title, sizeof(app->home_categories[c].title), "%s", cat_titles[c]);
        app->home_categories[c].item_count = 0;
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
        if (!settings_accepts_item(app, it)) continue;

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

        if (it->is_game) {
            bool is_ps4 = (SDL_strncasecmp(it->id_code, "CUSA", 4) == 0 || it->format == FMT_PKG);
            CarouselRow *plat = &app->home_categories[is_ps4 ? HOME_CAT_PS4 : HOME_CAT_PS5];
            if (plat->item_count < MAX_ITEMS_PER_ROW) plat->items[plat->item_count++] = it;

            CarouselRow *pop = &app->home_categories[HOME_CAT_POPULAR];
            if (pop->item_count < MAX_ITEMS_PER_ROW) pop->items[pop->item_count++] = it;

            CarouselRow *newest = &app->home_categories[HOME_CAT_NEWEST];
            if (newest->item_count < MAX_ITEMS_PER_ROW) newest->items[newest->item_count++] = it;

            CarouselRow *rated = &app->home_categories[HOME_CAT_TOP_RATED];
            if (rated->item_count < MAX_ITEMS_PER_ROW) rated->items[rated->item_count++] = it;
        }
    }

    if (app->home_categories[HOME_CAT_POPULAR].item_count > 1) {
        qsort(app->home_categories[HOME_CAT_POPULAR].items,
              (size_t)app->home_categories[HOME_CAT_POPULAR].item_count,
              sizeof(GameItem *), cmp_popular_desc);
    }
    if (app->home_categories[HOME_CAT_NEWEST].item_count > 1) {
        qsort(app->home_categories[HOME_CAT_NEWEST].items,
              (size_t)app->home_categories[HOME_CAT_NEWEST].item_count,
              sizeof(GameItem *), cmp_newest_desc);
    }
    if (app->home_categories[HOME_CAT_TOP_RATED].item_count > 1) {
        qsort(app->home_categories[HOME_CAT_TOP_RATED].items,
              (size_t)app->home_categories[HOME_CAT_TOP_RATED].item_count,
              sizeof(GameItem *), cmp_rating_desc);
    }

    for (int r = 0; r < TOTAL_ROWS; r++) {
        if (app->rows[r].selected_col >= app->rows[r].item_count) {
            app->rows[r].selected_col = (app->rows[r].item_count > 0) ? app->rows[r].item_count - 1 : 0;
        }
    }
    for (int c = 0; c < HOME_CAT_COUNT; c++) {
        if (app->home_categories[c].selected_col >= app->home_categories[c].item_count) {
            app->home_categories[c].selected_col =
                (app->home_categories[c].item_count > 0) ? app->home_categories[c].item_count - 1 : 0;
        }
    }

    app->catalog_dirty = false;
    search_rebuild_locked(app);
    SDL_UnlockMutex(app->catalog_mutex);
}

static GameItem *get_focused_item(WolodriveApp *app) {
    if (app->detail_open) return app->detail_item;
    if (app->active_row == VIEW_SEARCH)
        return app->search_count ? app->search_results[app->search_selected] : NULL;
    if (is_library_view(app->active_row) && app->library_depth != LIBRARY_GAMES) return NULL;
    CarouselRow *row = active_home_row(app);
    if (row->item_count <= 0) return NULL;
    int col = SDL_max(0, SDL_min(row->selected_col, row->item_count - 1));
    return row->items[col];
}

static void track_description_focus(WolodriveApp *app, GameItem *item) {
    if (app->description_item != item) {
        if (app->hero_current_item && app->hero_current_item != item) {
            app->hero_prev_item = app->hero_current_item;
            app->hero_crossfade = 0.0f;
            app->card_focus_scale = 1.0f;
        }
        app->hero_current_item = item;
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
    else app->sidebar_focus = true;
}

static void library_open_selected(WolodriveApp *app) {
    app->sidebar_focus = false;
    if (app->library_depth == LIBRARY_COMPANIES) {
        app->library_company = app->library_company_selection;
        app->library_console_selection = 0;
        app->library_depth = LIBRARY_CONSOLES;
    } else if (app->library_depth == LIBRARY_CONSOLES) {
        app->library_depth = LIBRARY_GAMES;
        CarouselRow *row = &app->rows[app->active_row];
        row->selected_col = 0;
        row->scroll_x = row->target_scroll_x = 0;
        row->scroll_velocity = 0;
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
            app->sidebar_focus = false;
            library_back(app);
            return true;
        }
        if (app->library_depth == LIBRARY_GAMES) {
            if (ev->button.x >= CONTENT_X) app->sidebar_focus = false;
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
    if (app->sidebar_focus) {
        if (action == 2 || action == 5) { app->sidebar_focus = false; return true; }
        if (action == 1) return true;
        return false; /* Up/down still navigate the sidebar. */
    }
    if (app->library_depth == LIBRARY_GAMES) {
        if (action == 1 && app->rows[app->active_row].selected_col == 0) {
            app->sidebar_focus = true;
            return true;
        }
        return action == 3 || action == 4; /* Console games use a horizontal row. */
    }
    if (action == 1) {
        if (*selection % LIBRARY_COLUMNS == 0) app->sidebar_focus = true;
        else (*selection)--;
    } else if (action == 2) {
        if (*selection % LIBRARY_COLUMNS < LIBRARY_COLUMNS - 1 && *selection + 1 < count) (*selection)++;
    } else if (action == 3) {
        if (*selection >= LIBRARY_COLUMNS) *selection -= LIBRARY_COLUMNS;
    } else if (action == 4) {
        if ((*selection / LIBRARY_COLUMNS + 1) * LIBRARY_COLUMNS < count)
            *selection = SDL_min(*selection + LIBRARY_COLUMNS, count - 1);
    } else if (action == 5) library_open_selected(app);
    return action != 0 || ev->type == SDL_MOUSEWHEEL;
}

static void ui_open_game(WolodriveApp *app, GameItem *item);
static const char *action_label(WolodriveApp *app, GameItem *item);
#include "ui_search.h"
#include "ui_game_details.h"
#include "ui_settings.h"

static void sync_home_active_row(WolodriveApp *app) {
    if (app->home_cat_index < 0) app->home_cat_index = 0;
    if (app->home_cat_index >= HOME_CAT_COUNT) app->home_cat_index = HOME_CAT_COUNT - 1;
    CarouselRow *cat = &app->home_categories[app->home_cat_index];
    CarouselRow *home = &app->rows[VIEW_HOME];
    home->item_count = cat->item_count;
    for (int i = 0; i < cat->item_count && i < MAX_CATALOG_ITEMS; i++) {
        home->items[i] = cat->items[i];
    }
    if (cat->selected_col >= cat->item_count) {
        cat->selected_col = cat->item_count > 0 ? cat->item_count - 1 : 0;
    }
    home->selected_col = cat->selected_col;
    home->scroll_x = cat->scroll_x;
    home->target_scroll_x = cat->target_scroll_x;
    home->scroll_velocity = cat->scroll_velocity;
}

static const char *home_category_title(WolodriveApp *app, int cat) {
    switch (cat) {
        case HOME_CAT_PS5:       return TR(app, TXT_CAT_PS5);
        case HOME_CAT_PS4:       return TR(app, TXT_CAT_PS4);
        case HOME_CAT_POPULAR:   return TR(app, TXT_CAT_POPULAR);
        case HOME_CAT_NEWEST:    return TR(app, TXT_CAT_NEWEST);
        case HOME_CAT_TOP_RATED: return TR(app, TXT_CAT_TOP_RATED);
        default:                 return TR(app, TXT_LATEST_GAMES);
    }
}

void ui_handle_input(WolodriveApp *app, const SDL_Event *ev) {
    if (ev->type == SDL_WINDOWEVENT) {
        if (ev->window.event == SDL_WINDOWEVENT_LEAVE ||
            ev->window.event == SDL_WINDOWEVENT_FOCUS_LOST) {
            app->sidebar_hovered = false;
            app->description_hovered = false;
        }
    }
    if (ev->type == SDL_KEYDOWN) {
        if (ev->key.keysym.sym == SDLK_F11 ||
            ((ev->key.keysym.mod & KMOD_ALT) && ev->key.keysym.sym == SDLK_RETURN)) {
            Uint32 flags = SDL_GetWindowFlags(app->window);
            bool is_fullscreen = (flags & (SDL_WINDOW_FULLSCREEN | SDL_WINDOW_FULLSCREEN_DESKTOP)) != 0;
            SDL_SetWindowFullscreen(app->window, is_fullscreen ? 0 : SDL_WINDOW_FULLSCREEN_DESKTOP);
        }
    }
    if (ev->type == SDL_MOUSEMOTION) {
        int mx = ev->motion.x;
        float sb_limit = app->sidebar_width > (float)SIDEBAR_COLLAPSED_W
                             ? app->sidebar_width
                             : (float)SIDEBAR_COLLAPSED_W + 24.0f;
        app->sidebar_hovered = (mx >= 0 && (float)mx <= sb_limit);
    }

    if (handle_language_input(app, ev)) return;
    if (app->detail_open) { handle_game_details_input(app, ev); return; }
    if (ev->type == SDL_MOUSEBUTTONDOWN && ev->button.button == SDL_BUTTON_LEFT) {
        int sidebar_hit_w = (app->sidebar_hovered || app->sidebar_focus || app->sidebar_expand > 0.35f)
                                ? SIDEBAR_EXPANDED_W
                                : SIDEBAR_COLLAPSED_W;
        for (int r = 0; r < TOTAL_ROWS; r++) {
            SDL_Rect sb = sidebar_target(r);
            sb.w = sidebar_hit_w;
            if (contains_point(sb, ev->button.x, ev->button.y)) {
                app->home_grid_expanded = false;
                select_sidebar_view(app, r);
                track_description_focus(app, get_focused_item(app));
                return;
            }
        }
    }
    /* Category navigation belongs exclusively to the left rail. Consume entry
       keys so returning to content preserves its selection without activating it. */
    if (app->sidebar_focus) {
        int direction = 0;
        bool enter = false, left = false;
        if (ev->type == SDL_KEYDOWN) {
            SDL_Keycode key = ev->key.keysym.sym;
            direction = (key == SDLK_DOWN || key == SDLK_s) - (key == SDLK_UP || key == SDLK_w);
            enter = key == SDLK_RIGHT || key == SDLK_d || key == SDLK_RETURN || key == SDLK_SPACE;
            left = key == SDLK_LEFT || key == SDLK_a;
        } else if (ev->type == SDL_CONTROLLERBUTTONDOWN) {
            Uint8 button = ev->cbutton.button;
            direction = (button == SDL_CONTROLLER_BUTTON_DPAD_DOWN) -
                        (button == SDL_CONTROLLER_BUTTON_DPAD_UP);
            enter = button == SDL_CONTROLLER_BUTTON_DPAD_RIGHT || button == SDL_CONTROLLER_BUTTON_A;
            left = button == SDL_CONTROLLER_BUTTON_DPAD_LEFT;
        }
        if (direction) {
            int position = app->active_row == VIEW_SEARCH ? -1 :
                           app->active_row == VIEW_SETTINGS ? VIEW_DOWNLOADS+1 : app->active_row;
            position += direction;
            if (position >= -1 && position <= VIEW_DOWNLOADS+1)
                select_sidebar_view(app, position == -1 ? VIEW_SEARCH :
                                    position == VIEW_DOWNLOADS+1 ? VIEW_SETTINGS : position);
            return;
        }
        if (enter) { app->sidebar_focus = false; return; }
        if (left) return;
    }
    if (handle_settings_input(app, ev)) return;
    if (handle_search_input(app, ev)) return;
    if (handle_library_input(app, ev)) return;

    /* Przyciski akcji w sekcji Hero ([Play/Pobierz] oraz [Więcej informacji]) */
    if (ev->type == SDL_MOUSEBUTTONDOWN && ev->button.button == SDL_BUTTON_LEFT &&
        app->active_row != VIEW_SEARCH && app->active_row != VIEW_SETTINGS &&
        !app->home_grid_expanded) {
        GameItem *hero_item = get_focused_item(app);
        if (hero_item && contains_point(HERO_PLAY_BUTTON, ev->button.x, ev->button.y)) {
            app->sidebar_focus = false;
            sys_trigger_action(app, hero_item);
            return;
        }
        if (hero_item && contains_point(HERO_INFO_BUTTON, ev->button.x, ev->button.y)) {
            app->sidebar_focus = false;
            ui_open_game(app, hero_item);
            return;
        }
    }

    /* Kliknięcie w tytuł kategorii lub "Pokaż wszystko" oraz kafelki na ekranie Home */
    if (ev->type == SDL_MOUSEBUTTONDOWN && ev->button.button == SDL_BUTTON_LEFT &&
        app->active_row == VIEW_HOME) {
        int mx = ev->button.x, my = ev->button.y;
        if (app->home_grid_expanded) {
            SDL_Rect grid_header = {CONTENT_X, TOPBAR_HEIGHT + 8, SCREEN_WIDTH - CONTENT_X - 48, 44};
            if (contains_point(grid_header, mx, my)) {
                app->home_grid_expanded = false;
                return;
            }
            CarouselRow *cat_row = &app->home_categories[app->home_cat_index];
            for (int col = 0; col < cat_row->item_count; col++) {
                SDL_Rect card = home_grid_card_target(app, col);
                if (contains_point(card, mx, my)) {
                    app->sidebar_focus = false;
                    cat_row->selected_col = col;
                    app->rows[VIEW_HOME].selected_col = col;
                    track_description_focus(app, get_focused_item(app));
                    ui_open_game(app, get_focused_item(app));
                    return;
                }
            }
            return;
        }

        for (int cat = 0; cat < HOME_CAT_COUNT; cat++) {
            int row_y = CARDS_Y + cat * HOME_ROW_STRIDE - (int)app->home_vertical_scroll;
            int header_y = row_y - 44;
            if (header_y < HERO_HEIGHT - 40 || header_y > SCREEN_HEIGHT) continue;
            SDL_Rect title_hit = {CONTENT_X, header_y, 560, 38};
            SDL_Rect show_all_hit = {HOME_SHOW_ALL_BUTTON.x, header_y + 2,
                                     HOME_SHOW_ALL_BUTTON.w, HOME_SHOW_ALL_BUTTON.h};
            if (contains_point(title_hit, mx, my) || contains_point(show_all_hit, mx, my)) {
                app->sidebar_focus = false;
                app->home_cat_index = cat;
                sync_home_active_row(app);
                app->home_grid_expanded = true;
                track_description_focus(app, get_focused_item(app));
                return;
            }
        }

        for (int cat = 0; cat < HOME_CAT_COUNT; cat++) {
            CarouselRow *cat_row = &app->home_categories[cat];
            for (int col = 0; col < cat_row->item_count; col++) {
                SDL_Rect card = home_row_card_target(app, cat, col);
                if (card.y + card.h < HERO_HEIGHT - 16 || card.y > SCREEN_HEIGHT) continue;
                if (contains_point(card, mx, my)) {
                    app->sidebar_focus = false;
                    app->home_cat_index = cat;
                    cat_row->selected_col = col;
                    sync_home_active_row(app);
                    track_description_focus(app, get_focused_item(app));
                    ui_open_game(app, get_focused_item(app));
                    return;
                }
            }
        }
    }

    if (ev->type == SDL_MOUSEBUTTONDOWN && ev->button.button == SDL_BUTTON_LEFT) {
        CarouselRow *clicked_row = &app->rows[app->active_row];
        if (ev->button.x >= RAIL_WIDTH + 20) {
            for (int col = 0; col < clicked_row->item_count; col++) {
                SDL_Rect card = card_target(app, col);
                if (contains_point(card, ev->button.x, ev->button.y)) {
                    app->sidebar_focus = false;
                    clicked_row->selected_col = col;
                    track_description_focus(app, get_focused_item(app));
                    ui_open_game(app, get_focused_item(app));
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
    if (ev->type == SDL_KEYDOWN && ev->key.keysym.sym == SDLK_g && !ev->key.repeat &&
        app->active_row == VIEW_HOME) {
        app->home_grid_expanded = !app->home_grid_expanded;
        return;
    }
    if (ev->type == SDL_CONTROLLERBUTTONDOWN && ev->cbutton.button == SDL_CONTROLLER_BUTTON_X &&
        app->active_row == VIEW_HOME) {
        app->home_grid_expanded = !app->home_grid_expanded;
        return;
    }
    if (ev->type == SDL_MOUSEWHEEL && app->description_hovered) {
        int direction = ev->wheel.direction == SDL_MOUSEWHEEL_FLIPPED ? -1 : 1;
        app->description_scroll = SDL_max(0, app->description_scroll - ev->wheel.y * direction * 24);
        return;
    }
    if (ev->type == SDL_MOUSEWHEEL && app->active_row == VIEW_HOME && !app->description_hovered) {
        int direction = ev->wheel.direction == SDL_MOUSEWHEEL_FLIPPED ? -1 : 1;
        int dy = ev->wheel.y * direction;
        if (!app->home_grid_expanded) {
            if (dy < 0 && app->home_cat_index < HOME_CAT_COUNT - 1) {
                app->home_cat_index++;
                sync_home_active_row(app);
            } else if (dy > 0 && app->home_cat_index > 0) {
                app->home_cat_index--;
                sync_home_active_row(app);
            }
            track_description_focus(app, get_focused_item(app));
        }
        return;
    }
    if (ev->type == SDL_KEYDOWN && (ev->key.keysym.sym == SDLK_PAGEUP || ev->key.keysym.sym == SDLK_PAGEDOWN)) {
        app->description_scroll = SDL_max(0, app->description_scroll + (ev->key.keysym.sym == SDLK_PAGEUP ? -48 : 48));
        return;
    }

    if (app->active_row == VIEW_HOME) {
        CarouselRow *cat_row = &app->home_categories[app->home_cat_index];
        CarouselRow *home_row = &app->rows[VIEW_HOME];
        /* Keep cat_row->selected_col synchronized if external code modified home_row->selected_col */
        if (cat_row->selected_col != home_row->selected_col &&
            home_row->selected_col >= 0 && home_row->selected_col < cat_row->item_count) {
            cat_row->selected_col = home_row->selected_col;
        }
        if (ev->type == SDL_KEYDOWN) {
            switch (ev->key.keysym.sym) {
                case SDLK_LEFT:
                case SDLK_a:
                    if (cat_row->selected_col > 0) {
                        cat_row->selected_col--;
                        home_row->selected_col = cat_row->selected_col;
                    } else {
                        app->sidebar_focus = true;
                    }
                    break;
                case SDLK_RIGHT:
                case SDLK_d:
                    if (cat_row->selected_col + 1 < cat_row->item_count) {
                        cat_row->selected_col++;
                        home_row->selected_col = cat_row->selected_col;
                    }
                    break;
                case SDLK_UP:
                case SDLK_w:
                    if (app->home_grid_expanded) {
                        if (cat_row->selected_col >= HOME_GRID_COLS) {
                            cat_row->selected_col -= HOME_GRID_COLS;
                            home_row->selected_col = cat_row->selected_col;
                        }
                    } else if (app->home_cat_index > 0) {
                        app->home_cat_index--;
                        sync_home_active_row(app);
                    }
                    break;
                case SDLK_DOWN:
                case SDLK_s:
                    if (app->home_grid_expanded) {
                        if (cat_row->selected_col + HOME_GRID_COLS < cat_row->item_count) {
                            cat_row->selected_col += HOME_GRID_COLS;
                            home_row->selected_col = cat_row->selected_col;
                        }
                    } else if (app->home_cat_index + 1 < HOME_CAT_COUNT) {
                        app->home_cat_index++;
                        sync_home_active_row(app);
                    }
                    break;
                case SDLK_RETURN:
                case SDLK_SPACE:
                    ui_open_game(app, get_focused_item(app));
                    break;
                case SDLK_BACKSPACE:
                case SDLK_b:
                    if (app->home_grid_expanded) app->home_grid_expanded = false;
                    else sys_cancel_action(app, get_focused_item(app));
                    break;
                case SDLK_r:
                case SDLK_y:
                case SDLK_F5:
                    catalog_start_async_sync(app);
                    break;
                case SDLK_ESCAPE:
                    if (app->home_grid_expanded) app->home_grid_expanded = false;
                    else app->running = false;
                    break;
                default:
                    break;
            }
        } else if (ev->type == SDL_CONTROLLERBUTTONDOWN) {
            switch (ev->cbutton.button) {
                case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
                    if (cat_row->selected_col > 0) {
                        cat_row->selected_col--;
                        home_row->selected_col = cat_row->selected_col;
                    } else {
                        app->sidebar_focus = true;
                    }
                    break;
                case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
                    if (cat_row->selected_col + 1 < cat_row->item_count) {
                        cat_row->selected_col++;
                        home_row->selected_col = cat_row->selected_col;
                    }
                    break;
                case SDL_CONTROLLER_BUTTON_DPAD_UP:
                    if (app->home_grid_expanded) {
                        if (cat_row->selected_col >= HOME_GRID_COLS) {
                            cat_row->selected_col -= HOME_GRID_COLS;
                            home_row->selected_col = cat_row->selected_col;
                        }
                    } else if (app->home_cat_index > 0) {
                        app->home_cat_index--;
                        sync_home_active_row(app);
                    }
                    break;
                case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
                    if (app->home_grid_expanded) {
                        if (cat_row->selected_col + HOME_GRID_COLS < cat_row->item_count) {
                            cat_row->selected_col += HOME_GRID_COLS;
                            home_row->selected_col = cat_row->selected_col;
                        }
                    } else if (app->home_cat_index + 1 < HOME_CAT_COUNT) {
                        app->home_cat_index++;
                        sync_home_active_row(app);
                    }
                    break;
                case SDL_CONTROLLER_BUTTON_A:
                    ui_open_game(app, get_focused_item(app));
                    break;
                case SDL_CONTROLLER_BUTTON_B:
                    if (app->home_grid_expanded) app->home_grid_expanded = false;
                    else sys_cancel_action(app, get_focused_item(app));
                    break;
                case SDL_CONTROLLER_BUTTON_Y:
                    catalog_start_async_sync(app);
                    break;
                default:
                    break;
            }
        }
        track_description_focus(app, get_focused_item(app));
        return;
    }

    CarouselRow *row = &app->rows[app->active_row];

    if (ev->type == SDL_KEYDOWN) {
        switch (ev->key.keysym.sym) {
            case SDLK_LEFT:
            case SDLK_a:
                if (row->selected_col > 0) row->selected_col--;
                else app->sidebar_focus = true;
                break;
            case SDLK_RIGHT:
            case SDLK_d:
                if (row->selected_col + 1 < row->item_count) row->selected_col++;
                break;
            case SDLK_UP:
            case SDLK_w:
                break;
            case SDLK_DOWN:
            case SDLK_s:
                break;
            case SDLK_RETURN:
            case SDLK_SPACE:
                ui_open_game(app, get_focused_item(app));
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
                else app->sidebar_focus = true;
                break;
            case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
                if (row->selected_col + 1 < row->item_count) row->selected_col++;
                break;
            case SDL_CONTROLLER_BUTTON_DPAD_UP:
                break;
            case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
                break;
            case SDL_CONTROLLER_BUTTON_A: /* [X] Cross na DualSense */
                ui_open_game(app, get_focused_item(app));
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
    track_description_focus(app, get_focused_item(app));
}

void ui_update_animations(WolodriveApp *app, float dt) {
    if (app->catalog_dirty) {
        ui_rebuild_carousels(app);
    }

    app->focus_pulse += dt;

    /* Sync home_categories[home_cat_index] with rows[VIEW_HOME] if tests changed rows[VIEW_HOME].selected_col */
    if (app->active_row == VIEW_HOME) {
        CarouselRow *cat = &app->home_categories[app->home_cat_index];
        CarouselRow *home = &app->rows[VIEW_HOME];
        if (cat->selected_col != home->selected_col &&
            home->selected_col >= 0 && home->selected_col < cat->item_count) {
            cat->selected_col = home->selected_col;
        }
    }

    const float card_stride = CARD_WIDTH + CARD_GAP;
    for (int r = 0; r < TOTAL_ROWS; r++) {
        CarouselRow *row = &app->rows[r];
        /* Keep the focused card at the left edge, as in the TV interface. */
        row->target_scroll_x = row->selected_col * card_stride;
        if (dt <= 0.0f) continue;
        /* Exact critically damped motion: gentle acceleration, continuous
           velocity on repeated input, and identical timing at different FPS. */
        const float speed = 24.0f;
        float offset = row->scroll_x - row->target_scroll_x;
        float motion = (row->scroll_velocity + speed * offset) * dt;
        float decay = expf(-speed * dt);
        row->scroll_x = row->target_scroll_x + (offset + motion) * decay;
        row->scroll_velocity = (row->scroll_velocity - speed * motion) * decay;
        float maximum = SDL_max(0, row->item_count - 1) * card_stride;
        if (row->scroll_x < 0 || row->scroll_x > maximum) {
            row->scroll_x = fmaxf(0, fminf(row->scroll_x, maximum));
            row->scroll_velocity = 0;
        }
        if (fabsf(row->scroll_x - row->target_scroll_x) < 0.05f && fabsf(row->scroll_velocity) < 0.5f) {
            row->scroll_x = row->target_scroll_x;
            row->scroll_velocity = 0;
        }
    }

    for (int c = 0; c < HOME_CAT_COUNT; c++) {
        CarouselRow *row = &app->home_categories[c];
        row->target_scroll_x = row->selected_col * card_stride;
        if (dt <= 0.0f) continue;
        const float speed = 24.0f;
        float offset = row->scroll_x - row->target_scroll_x;
        float motion = (row->scroll_velocity + speed * offset) * dt;
        float decay = expf(-speed * dt);
        row->scroll_x = row->target_scroll_x + (offset + motion) * decay;
        row->scroll_velocity = (row->scroll_velocity - speed * motion) * decay;
        float maximum = SDL_max(0, row->item_count - 1) * card_stride;
        if (row->scroll_x < 0 || row->scroll_x > maximum) {
            row->scroll_x = fmaxf(0, fminf(row->scroll_x, maximum));
            row->scroll_velocity = 0;
        }
        if (fabsf(row->scroll_x - row->target_scroll_x) < 0.05f && fabsf(row->scroll_velocity) < 0.5f) {
            row->scroll_x = row->target_scroll_x;
            row->scroll_velocity = 0;
        }
    }

    if (dt > 0.0f) {
        /* Płynne przewijanie w pionie między kategoriami (Wzór Netflix) */
        float target_v = 0.0f;
        if (app->active_row == VIEW_HOME) {
            if (app->home_grid_expanded) {
                int sel_row = app->home_categories[app->home_cat_index].selected_col / HOME_GRID_COLS;
                if (sel_row > 1) {
                    target_v = (float)((sel_row - 1) * (CARD_HEIGHT + 58));
                }
            } else {
                target_v = (float)(app->home_cat_index * HOME_ROW_STRIDE);
            }
        }
        app->home_vertical_scroll += (target_v - app->home_vertical_scroll) * (1.0f - expf(-12.0f * dt));
        if (fabsf(target_v - app->home_vertical_scroll) < 0.5f) {
            app->home_vertical_scroll = target_v;
        }

        /* Animacja rozszerzenia widoku "Pokaż wszystko" (siatka Grid) */
        float target_grid = app->home_grid_expanded ? 1.0f : 0.0f;
        app->home_grid_anim += (target_grid - app->home_grid_anim) * (1.0f - expf(-12.0f * dt));
        if (app->home_grid_anim < 0.001f) app->home_grid_anim = 0.0f;
        if (app->home_grid_anim > 0.999f) app->home_grid_anim = 1.0f;

        /* Płynne rozsuwanie paska bocznego w prawo po najechaniu / focusie */
        float target_sb = (app->sidebar_hovered || app->sidebar_focus) ? 1.0f : 0.0f;
        app->sidebar_expand += (target_sb - app->sidebar_expand) * (1.0f - expf(-14.0f * dt));
        if (app->sidebar_expand < 0.001f) app->sidebar_expand = 0.0f;
        if (app->sidebar_expand > 0.999f) app->sidebar_expand = 1.0f;
        app->sidebar_width = (float)SIDEBAR_COLLAPSED_W +
                             ((float)(SIDEBAR_EXPANDED_W - SIDEBAR_COLLAPSED_W) * app->sidebar_expand);

        /* Płynne powiększanie wybranego kafelka (scale up / down) */
        float target_scale = (app->sidebar_focus || app->home_grid_expanded) ? 1.0f : 1.06f;
        app->card_focus_scale += (target_scale - app->card_focus_scale) * (1.0f - expf(-14.0f * dt));
        if (fabsf(target_scale - app->card_focus_scale) < 0.002f) app->card_focus_scale = target_scale;

        /* Dynamiczna zmiana tła Hero (cross-fade animation) */
        if (app->hero_crossfade < 1.0f) {
            app->hero_crossfade += 5.5f * dt;
            if (app->hero_crossfade >= 1.0f) {
                app->hero_crossfade = 1.0f;
                app->hero_prev_item = NULL;
            }
        }
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
    if ((settings_filters_active(app)) &&
        (app->active_row == VIEW_HOME || app->active_row == VIEW_GAMES || app->active_row == VIEW_DOWNLOADS))
        return TR(app, TXT_SETTINGS_NO_MATCHES);
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
    if (app->active_row == VIEW_HOME) return home_category_title(app, app->home_cat_index);
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
        draw_text(app, app->font_hero_title, view_content_title(app),
                  CONTENT_X, 150, white, 900);
        draw_text(app, app->font_body, empty_view_message(app), CONTENT_X, 320, muted, 690);
        return;
    }

    /* Cross-fade tła (Background Hero) pomiędzy poprzednią a aktualnie wybraną grą */
    if (app->hero_prev_item && app->hero_prev_item != hero && app->hero_crossfade < 1.0f) {
        Uint8 prev_alpha = (Uint8)(255.0f * (1.0f - app->hero_crossfade));
        Uint8 curr_alpha = (Uint8)(255.0f * app->hero_crossfade);
        render_cover_or_placeholder_alpha(app, app->hero_prev_item,
                                          RAIL_WIDTH, 0, SCREEN_WIDTH - RAIL_WIDTH, 720, true, prev_alpha);
        render_cover_or_placeholder_alpha(app, hero,
                                          RAIL_WIDTH, 0, SCREEN_WIDTH - RAIL_WIDTH, 720, true, curr_alpha);
    } else {
        render_cover_or_placeholder(app, hero, RAIL_WIDTH, 0, SCREEN_WIDTH - RAIL_WIDTH, 720, true);
    }

    /* Czarny gradient zanikający w lewo i w dół (vignette / fade to black) */
    fill_rect(r, RAIL_WIDTH, 0, SCREEN_WIDTH - RAIL_WIDTH, 720, 0, 0, 0, 55);
    shade_hero(r);
    fill_vertical_gradient(r, RAIL_WIDTH, 420, SCREEN_WIDTH - RAIL_WIDTH, 320,
                           0, 0, 0, 0, 0, 0, 0, 255, 32);

    draw_text(app, app->font_logo, "W", CONTENT_X, 100, (SDL_Color){229, 9, 20, 255}, 0);
    draw_text(app, app->font_micro, "W O L O D R I V E", CONTENT_X + 45, 116, white, 0);
    /* Two title lines fit without displacing metadata or description. */
    SDL_Rect title_clip = {CONTENT_X, 142, 940, 136};
    SDL_RenderSetClipRect(r, &title_clip);
    draw_text(app, app->font_hero_title, hero->title, CONTENT_X, 142, white, title_clip.w);
    SDL_RenderSetClipRect(r, NULL);
    char metadata[256];
    const char *rel = hero->release_date[0] ? hero->release_date : "2024-01-01";
    snprintf(metadata, sizeof(metadata),
             "%s   v%s   %s   %.1f GB   ★ %.1f/10   ↓ %u   %s",
             hero->id_code, hero->version, sys_format_short_badge(hero->format),
             hero->size_gb, hero->rating_score, hero->download_count, rel);
    draw_single_line(app, app->font_small, metadata, CONTENT_X, 276, muted,
                     (SDL_Rect){CONTENT_X, 276, 860, 34});
    char description[MAX_TRANSLATED_DESCRIPTION_BYTES];
    description_text(app, hero, description, sizeof(description));
    UiTextEntry *entry = cached_text(app, app->font_body, description, muted, DESCRIPTION_AREA.w);
    if (entry) {
        int maximum_scroll = SDL_max(0, entry->h - DESCRIPTION_AREA.h);
        app->description_scroll = SDL_min(app->description_scroll, maximum_scroll);
        {
            SDL_Rect destination = {DESCRIPTION_AREA.x, DESCRIPTION_AREA.y - app->description_scroll,
                                    entry->w, entry->h};
            SDL_RenderSetClipRect(r, &DESCRIPTION_AREA);
            SDL_RenderCopy(r, entry->texture, NULL, &destination);
            SDL_RenderSetClipRect(r, NULL);
        }
        if (maximum_scroll > 0) {
            fill_rect(r, DESCRIPTION_AREA.x + DESCRIPTION_AREA.w + 4, DESCRIPTION_AREA.y, 3, DESCRIPTION_AREA.h, 65, 65, 75, 255);
            int thumb_y = DESCRIPTION_AREA.y + (DESCRIPTION_AREA.h - 12) * app->description_scroll / maximum_scroll;
            fill_rect(r, DESCRIPTION_AREA.x + DESCRIPTION_AREA.w + 4, thumb_y, 3, 12, 180, 180, 190, 255);
        }
    }

    /* Nowoczesne przyciski akcji w stylu Netflix Smart TV: [Play/Pobierz] oraz [Więcej informacji] */
    fill_rect(r, HERO_PLAY_BUTTON.x, HERO_PLAY_BUTTON.y, HERO_PLAY_BUTTON.w, HERO_PLAY_BUTTON.h,
              229, 9, 20, 255);
    draw_border(r, HERO_PLAY_BUTTON.x, HERO_PLAY_BUTTON.y, HERO_PLAY_BUTTON.w, HERO_PLAY_BUTTON.h,
                2, 255, 255, 255, 220);
    char play_label[128];
    snprintf(play_label, sizeof(play_label), "▶  %s", action_label(app, hero));
    draw_fitted_text(app, app->font_small, play_label,
                     (SDL_Rect){HERO_PLAY_BUTTON.x + 14, HERO_PLAY_BUTTON.y + 8,
                                HERO_PLAY_BUTTON.w - 28, HERO_PLAY_BUTTON.h - 16},
                     white, HERO_PLAY_BUTTON, true);

    fill_rect(r, HERO_INFO_BUTTON.x, HERO_INFO_BUTTON.y, HERO_INFO_BUTTON.w, HERO_INFO_BUTTON.h,
              42, 42, 52, 235);
    draw_border(r, HERO_INFO_BUTTON.x, HERO_INFO_BUTTON.y, HERO_INFO_BUTTON.w, HERO_INFO_BUTTON.h,
                1, 165, 165, 178, 200);
    char info_label[128];
    snprintf(info_label, sizeof(info_label), "ℹ  %s", TR(app, TXT_MORE_INFO));
    draw_fitted_text(app, app->font_small, info_label,
                     (SDL_Rect){HERO_INFO_BUTTON.x + 14, HERO_INFO_BUTTON.y + 8,
                                HERO_INFO_BUTTON.w - 28, HERO_INFO_BUTTON.h - 16},
                     white, HERO_INFO_BUTTON, true);

    if (hero->description_id == TEXT_COUNT) {
        fill_rect(r, DESCRIPTION_BUTTON.x, DESCRIPTION_BUTTON.y, DESCRIPTION_BUTTON.w, DESCRIPTION_BUTTON.h, 35, 35, 43, 255);
        TextId label = app->show_original_description ? TXT_TRANSLATED_DESCRIPTION : TXT_ORIGINAL_DESCRIPTION;
        draw_single_line(app, app->font_small, TR(app, label), DESCRIPTION_BUTTON.x + 12,
                         DESCRIPTION_BUTTON.y + 10, white, DESCRIPTION_BUTTON);
    }
}

static void render_single_card(WolodriveApp *app, GameItem *item, SDL_Rect base_card,
                               bool selected, SDL_Rect clip) {
    SDL_Renderer *r = app->renderer;
    SDL_Color white = {245, 245, 245, 255}, muted = {170, 170, 170, 255};
    SDL_Rect card = base_card;
    if (selected && !app->sidebar_focus) {
        float scale = app->card_focus_scale > 1.0f ? app->card_focus_scale : 1.05f;
        int new_w = (int)lroundf((float)base_card.w * scale);
        int new_h = (int)lroundf((float)CARD_HEIGHT * scale);
        card.x = base_card.x - (new_w - base_card.w) / 2;
        card.y = base_card.y - (new_h - CARD_HEIGHT) / 2;
        card.w = new_w;
        card.h = new_h;
    } else {
        card.h = CARD_HEIGHT;
    }

    render_cover_or_placeholder(app, item, card.x, card.y, card.w, card.h, false);
    draw_rounded_card_corners(r, card.x, card.y, card.w, card.h, 0, 0, 0);

    /* Plakietka oceny (IMDb/Metacritic rating) w prawym górnym rogu okładki */
    if (item->rating_score > 0.0f) {
        char rating_badge[24];
        snprintf(rating_badge, sizeof(rating_badge), "★ %.1f", item->rating_score);
        SDL_Rect badge_rect = {card.x + card.w - 68, card.y + 8, 60, 24};
        fill_rect(r, badge_rect.x, badge_rect.y, badge_rect.w, badge_rect.h, 14, 14, 18, 220);
        draw_fitted_text(app, app->font_micro, rating_badge, badge_rect,
                         (SDL_Color){255, 205, 45, 255}, badge_rect, true);
    }

    SDL_Rect title = {card.x, card.y + card.h, card.w, CARD_TITLE};
    fill_rect(r, title.x, title.y, title.w, title.h, 18, 18, 22, 255);
    draw_single_line(app, app->font_micro, item->title, title.x + 6, title.y + 2, white,
                     (SDL_Rect){SDL_max(title.x + 6, clip.x), title.y,
                                SDL_max(0, SDL_min(title.x + title.w - 6, SCREEN_WIDTH) -
                                           SDL_max(title.x + 6, clip.x)),
                                title.h});
    int footer_y = card.y + card.h + CARD_TITLE;
    fill_rect(r, card.x, footer_y, card.w, CARD_FOOTER,
              selected ? 240 : 0, selected ? 240 : 0, selected ? 240 : 0, 255);
    char status[256];
    if (selected) snprintf(status, sizeof(status), "[X] %s", TR(app, TXT_GAME_DETAILS));
    else snprintf(status, sizeof(status), "%s  /  %.1f GB", sys_format_short_badge(item->format), item->size_gb);
    SDL_Rect footer_clip = {SDL_max(card.x + 8, clip.x), footer_y, 0, CARD_FOOTER};
    footer_clip.w = SDL_max(0, SDL_min(card.x + card.w - 8, SCREEN_WIDTH) - footer_clip.x);
    draw_fitted_text(app, app->font_small, status,
                     (SDL_Rect){card.x + 12, footer_y, card.w - 24, CARD_FOOTER},
                     selected ? (SDL_Color){0, 0, 0, 255} : muted, footer_clip, true);
    if (item->state == STATE_DOWNLOADING) {
        int progress = (int)(card.w * fminf(100.0f, fmaxf(0.0f, item->progress)) / 100.0f);
        fill_rect(r, card.x, footer_y - 4, progress, 4, 229, 9, 20, 255);
    }
    if (selected && !app->sidebar_focus) {
        draw_border(r, card.x - 4, card.y - 4, card.w + 8, card.h + CARD_TITLE + CARD_FOOTER + 8,
                    4, 240, 240, 240, 255);
    }
}

static void render_carousels(WolodriveApp *app) {
    SDL_Renderer *r = app->renderer;
    SDL_Color white = {245, 245, 245, 255}, muted = {170, 170, 170, 255};

    /* Wzór Netflix na ekranie głównym: 5 poziomych kategorii przewijanych w pionie
       oraz mechanika "Pokaż wszystko" (rozszerzenie kategorii na pełnoekranową siatkę Grid) */
    if (app->active_row == VIEW_HOME) {
        CarouselRow *active_cat = &app->home_categories[app->home_cat_index];
        if (app->home_grid_expanded) {
            fill_rect(r, RAIL_WIDTH, 0, SCREEN_WIDTH - RAIL_WIDTH, SCREEN_HEIGHT, 10, 10, 14, 248);
            draw_text(app, app->font_row_title, home_category_title(app, app->home_cat_index),
                      CONTENT_X, TOPBAR_HEIGHT + 12, white, 900);
            SDL_Rect back_btn = {HOME_SHOW_ALL_BUTTON.x - 40, TOPBAR_HEIGHT + 12, 260, 36};
            fill_rect(r, back_btn.x, back_btn.y, back_btn.w, back_btn.h, 36, 36, 46, 255);
            draw_border(r, back_btn.x, back_btn.y, back_btn.w, back_btn.h, 1, 229, 9, 20, 255);
            draw_fitted_text(app, app->font_small, TR(app, TXT_SHOW_LESS),
                             (SDL_Rect){back_btn.x + 12, back_btn.y + 6, back_btn.w - 24, back_btn.h - 12},
                             white, back_btn, true);

            SDL_Rect grid_clip = {RAIL_WIDTH + 12, TOPBAR_HEIGHT + 58,
                                  SCREEN_WIDTH - RAIL_WIDTH - 24, SCREEN_HEIGHT - TOPBAR_HEIGHT - 120};
            SDL_RenderSetClipRect(r, &grid_clip);
            for (int i = 0; i < active_cat->item_count; i++) {
                SDL_Rect card = home_grid_card_target(app, i);
                if (card.y + card.h + CARD_TITLE + CARD_FOOTER < grid_clip.y ||
                    card.y > grid_clip.y + grid_clip.h) continue;
                render_single_card(app, active_cat->items[i], card,
                                   i == active_cat->selected_col, grid_clip);
            }
            SDL_RenderSetClipRect(r, NULL);
            if (active_cat->item_count > 0) {
                char position[40];
                snprintf(position, sizeof(position), "%d / %d",
                         active_cat->selected_col + 1, active_cat->item_count);
                draw_text(app, app->font_micro, position, CONTENT_X, 956, muted, 0);
            }
            return;
        }

        SDL_Rect home_clip = {RAIL_WIDTH + 12, CARDS_Y - 56,
                              SCREEN_WIDTH - RAIL_WIDTH - 12, SCREEN_HEIGHT - (CARDS_Y - 56) - 62};
        SDL_RenderSetClipRect(r, &home_clip);
        for (int cat = 0; cat < HOME_CAT_COUNT; cat++) {
            CarouselRow *cat_row = &app->home_categories[cat];
            int row_y = CARDS_Y + cat * HOME_ROW_STRIDE - (int)app->home_vertical_scroll;
            int header_y = row_y - 44;
            if (row_y + CARD_HEIGHT + CARD_TITLE + CARD_FOOTER < home_clip.y ||
                header_y > home_clip.y + home_clip.h) {
                continue;
            }
            bool is_active_cat = (cat == app->home_cat_index);
            draw_text(app, app->font_row_title, home_category_title(app, cat),
                      CONTENT_X, header_y, is_active_cat ? white : muted, 0);

            /* Przycisk "Pokaż wszystko" przy nagłówku kategorii */
            SDL_Rect show_all = {HOME_SHOW_ALL_BUTTON.x, header_y + 2,
                                 HOME_SHOW_ALL_BUTTON.w, HOME_SHOW_ALL_BUTTON.h};
            fill_rect(r, show_all.x, show_all.y, show_all.w, show_all.h,
                      is_active_cat ? 34 : 22, is_active_cat ? 34 : 22, is_active_cat ? 44 : 28, 235);
            draw_border(r, show_all.x, show_all.y, show_all.w, show_all.h,
                        1, is_active_cat ? 229 : 85, is_active_cat ? 9 : 85, is_active_cat ? 20 : 95, 220);
            char show_all_label[96];
            snprintf(show_all_label, sizeof(show_all_label), "%s ›", TR(app, TXT_SHOW_ALL));
            draw_fitted_text(app, app->font_micro, show_all_label,
                             (SDL_Rect){show_all.x + 10, show_all.y + 4, show_all.w - 20, show_all.h - 8},
                             is_active_cat ? white : muted, show_all, true);

            if (!cat_row->item_count) continue;
            SDL_Rect row_clip = {RAIL_WIDTH + 20, SDL_max(home_clip.y, row_y - 14),
                                 SCREEN_WIDTH - RAIL_WIDTH - 20, CARD_HEIGHT + CARD_TITLE + CARD_FOOTER + 28};
            SDL_RenderSetClipRect(r, &row_clip);
            for (int col = 0; col < cat_row->item_count; col++) {
                SDL_Rect card = home_row_card_target(app, cat, col);
                if (card.x + card.w < row_clip.x || card.x >= SCREEN_WIDTH) continue;
                bool selected = is_active_cat && (col == cat_row->selected_col);
                render_single_card(app, cat_row->items[col], card, selected, row_clip);
            }
            SDL_RenderSetClipRect(r, &home_clip);
        }
        SDL_RenderSetClipRect(r, NULL);
        if (active_cat->item_count > 0) {
            char position[64];
            snprintf(position, sizeof(position), "%d / %d",
                     active_cat->selected_col + 1, active_cat->item_count);
            draw_text(app, app->font_micro, position, CONTENT_X, 956, muted, 0);
        }
        return;
    }

    CarouselRow *row = &app->rows[app->active_row];
    draw_text(app, app->font_row_title, view_content_title(app), CONTENT_X, 532, white, 0);
    if (!row->item_count) {
        return;
    }
    SDL_Rect clip = {RAIL_WIDTH + 20, CARDS_Y - 14, SCREEN_WIDTH - RAIL_WIDTH - 20,
                     CARD_HEIGHT + CARD_TITLE + CARD_FOOTER + 28};
    SDL_RenderSetClipRect(r, &clip);
    for (int col = 0; col < row->item_count; col++) {
        SDL_Rect card = card_target(app, col);
        if (card.x + card.w < clip.x || card.x >= SCREEN_WIDTH) continue;
        GameItem *item = row->items[col];
        bool selected = col == row->selected_col;
        render_single_card(app, item, card, selected, clip);
    }
    SDL_RenderSetClipRect(r, NULL);
    char position[40];
    snprintf(position, sizeof(position), "%d / %d", row->selected_col + 1, row->item_count);
    draw_text(app, app->font_micro, position, CONTENT_X, 956, muted, 0);
}

/* Geometry icons remain available on PS5 and do not depend on font symbols.
   Pasek boczny: domyślnie zwinięty (wyświetla minimalistyczne ikony), a po najechaniu / focusie
   płynnie rozsuwa się w prawo, odsłaniając pełne nazwy zakładek i lekko przyciemniając resztę UI. */
static const char *sidebar_item_title(WolodriveApp *app, int row) {
    if (row >= 0 && row < TOTAL_ROWS && app->rows[row].title[0] != '\0') {
        return app->rows[row].title;
    }
    switch (row) {
        case VIEW_HOME:      return TR(app, TXT_NAV_HOME);
        case VIEW_GAMES:     return TR(app, TXT_NAV_GAMES);
        case VIEW_EMULATORS: return TR(app, TXT_NAV_EMULATORS);
        case VIEW_FILES:     return TR(app, TXT_NAV_FILES);
        case VIEW_PAYLOADS:  return TR(app, TXT_NAV_PAYLOADS);
        case VIEW_DOWNLOADS: return TR(app, TXT_NAV_DOWNLOADS);
        case VIEW_SEARCH:    return TR(app, TXT_NAV_SEARCH);
        case VIEW_SETTINGS:  return TR(app, TXT_NAV_SETTINGS);
        default:             return "";
    }
}

static void render_sidebar(WolodriveApp *app) {
    SDL_Renderer *r = app->renderer;
    int sb_w = (int)lroundf(app->sidebar_width);
    if (sb_w < SIDEBAR_COLLAPSED_W) sb_w = SIDEBAR_COLLAPSED_W;
    if (sb_w > SIDEBAR_EXPANDED_W) sb_w = SIDEBAR_EXPANDED_W;

    /* Lekkie przyciemnienie reszty interfejsu, gdy pasek boczny jest rozwinięty */
    if (app->sidebar_expand > 0.02f) {
        Uint8 dim_alpha = (Uint8)(115.0f * app->sidebar_expand);
        fill_rect(r, sb_w, 0, SCREEN_WIDTH - sb_w, SCREEN_HEIGHT, 0, 0, 0, dim_alpha);
    }

    fill_rect(r, 0, 0, sb_w, SCREEN_HEIGHT, 12, 12, 12, 250);
    fill_rect(r, sb_w - 1, 0, 1, SCREEN_HEIGHT, 48, 48, 48, 255);

    /* Ograniczenie rysowania do bieżącej szerokości paska (płynne odsłanianie) */
    SDL_Rect sb_clip = {0, 0, sb_w, SCREEN_HEIGHT};
    SDL_Rect prev_clip;
    bool was_clipped = SDL_RenderIsClipEnabled(r);
    SDL_RenderGetClipRect(r, &prev_clip);
    SDL_RenderSetClipRect(r, &sb_clip);

    for (int row = 0; row < TOTAL_ROWS; row++) {
        SDL_Rect target = sidebar_target(row);
        bool selected = row == app->active_row;
        Uint8 c = selected ? 245 : 160;
        int x = 30, y = target.y + 28;
        if (selected) {
            fill_rect(r, 0, target.y, sb_w - 1, target.h, 25, 25, 25, 255);
            fill_rect(r, 0, target.y, 4, target.h, 229, 9, 20, 255);
            if (app->sidebar_focus)
                draw_border(r, 5, target.y + 2, sb_w - 9, target.h - 4, 2, 245, 245, 245, 255);
        }

        /* Ikony geometryczne */
        if (row == VIEW_SEARCH) {
            SDL_SetRenderDrawColor(r, c, c, c, 255);
            for (int angle = 0; angle < 360; angle++) {
                float a = angle * 3.14159265f / 180.0f;
                SDL_RenderDrawPoint(r, x + 12 + (int)(12 * cosf(a)), y + 12 + (int)(12 * sinf(a)));
            }
            SDL_RenderDrawLine(r, x + 21, y + 21, x + 33, y + 33);
        } else if (row == VIEW_HOME) {
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
        } else if (row == VIEW_SETTINGS) {
            for (int line = 0; line < 3; line++) {
                fill_rect(r, x, y+line*12, 32, 2, c,c,c,255);
                fill_rect(r, x+(line == 1 ? 20 : 7), y+line*12-3, 5, 8, c,c,c,255);
            }
        }

        /* Renderowanie podpisu zakładki (tekst obok ikony) */
        if (sb_w > SIDEBAR_COLLAPSED_W + 5) {
            Uint8 text_alpha = (Uint8)SDL_min(255, (int)(app->sidebar_expand * 255.0f));
            if (text_alpha > 0) {
                const char *label_text = sidebar_item_title(app, row);
                if (label_text && label_text[0] != '\0') {
                    TTF_Font *font = app->font_small ? app->font_small : app->font_body;
                    SDL_Color text_color = {c, c, c, 255};
                    UiTextEntry *entry = cached_text(app, font, label_text, text_color, 0);
                    if (entry && entry->texture) {
                        int text_x = 84;
                        int text_y = target.y + (target.h - entry->h) / 2;
                        SDL_Rect dst = {text_x, text_y, entry->w, entry->h};

                        SDL_SetTextureBlendMode(entry->texture, SDL_BLENDMODE_BLEND);
                        SDL_SetTextureAlphaMod(entry->texture, text_alpha);
                        SDL_RenderCopy(r, entry->texture, NULL, &dst);
                        SDL_SetTextureAlphaMod(entry->texture, 255);
                    }
                }
            }
        }
    }

    SDL_RenderSetClipRect(r, was_clipped ? &prev_clip : NULL);
}

static void render_language_control(WolodriveApp *app) {
    SDL_Renderer *r = app->renderer;
    int sb_w = (int)lroundf(app->sidebar_width);
    if (sb_w < SIDEBAR_COLLAPSED_W) sb_w = SIDEBAR_COLLAPSED_W;
    Uint8 c = app->language_menu_open ? 245 : 160;
    if (app->language_menu_open) {
        fill_rect(r, LANGUAGE_BUTTON.x, LANGUAGE_BUTTON.y, sb_w - 1,
                  LANGUAGE_BUTTON.h, 25, 25, 25, 255);
        fill_rect(r, 0, LANGUAGE_BUTTON.y, 4, LANGUAGE_BUTTON.h, 229, 9, 20, 255);
    }
    /* Outline globe matches the sidebar icons without relying on a font glyph. */
    int cx = 46, cy = LANGUAGE_BUTTON.y + 44;
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
    if (sb_w > SIDEBAR_COLLAPSED_W + 28) {
        SDL_Rect label = {84, LANGUAGE_BUTTON.y + 24, sb_w - 128, 42};
        if (label.w > 24) {
            draw_fitted_text(app, app->font_small,
                             i18n_language_name((Language)SDL_AtomicGet(&app->language)),
                             label, (SDL_Color){c, c, c, 255}, label, false);
        }
        int arrow_x = sb_w - 30;
        int arrow_y = cy + (app->language_menu_open ? 4 : -4);
        int tip_y = cy + (app->language_menu_open ? -4 : 4);
        SDL_SetRenderDrawColor(r, c, c, c, 255);
        SDL_RenderDrawLine(r, arrow_x - 5, arrow_y, arrow_x, tip_y);
        SDL_RenderDrawLine(r, arrow_x, tip_y, arrow_x + 5, arrow_y);
    }
}

static void render_top_bar(WolodriveApp *app) {
    SDL_Color red = {229, 9, 20, 255};
    SDL_Color muted = {155, 155, 155, 255};
    draw_text(app, app->font_logo, "WOLODRIVE", 1620, 956, red, 0);
    if (app->active_row != VIEW_SEARCH && app->active_row != VIEW_SETTINGS) {
        draw_text(app, app->font_micro, "← →  ↑ ↓ / D-pad    X / Enter", CONTENT_X, 984, muted, 0);
        draw_single_line(app, app->font_micro, TR(app, TXT_BACK_HINT), CONTENT_X + 378, 984, muted,
                         (SDL_Rect){CONTENT_X + 378, 984, 260, 30});
        draw_single_line(app, app->font_micro, TR(app, TXT_REFRESH_HINT), CONTENT_X + 658, 984, muted,
                         (SDL_Rect){CONTENT_X + 658, 984, 600, 30});
    }

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
                     app->catalog_count, app->sync_sources);
            break;
        default:
            snprintf(sync_info, sizeof(sync_info), "%s", TR(app, app->sync_status_id));
            break;
    }
    SDL_UnlockMutex(app->catalog_mutex);

    draw_single_line(app, app->font_micro, sync_info, CONTENT_X, 1032, muted,
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

    draw_single_line(app, app->font_micro, telemetry, 1200, 1032, muted,
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
        draw_text(app, app->font_body, i18n_language_name(LANGUAGE_ORDER[i]),
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
    draw_text(app, app->font_small, app->toast_message, box_x + 22, box_y + 18, white, box_w - 36);
}

static int library_console_game_count(WolodriveApp *app, const LibraryConsole *console) {
    if (!console || !console->title_id_prefix) return 0;
    int count = 0;
    SDL_LockMutex(app->catalog_mutex);
    for (int i = 0; i < app->catalog_count; i++) {
        GameItem *item = &app->catalog[i];
        if (item->is_game && settings_accepts_item(app, item) && library_console_matches_id(console, item->id_code)) count++;
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
    draw_fitted_text(app, app->font_small, TR(app, TXT_LIBRARY_BACK),
                     (SDL_Rect){CONTENT_X + 18, 28, 144, 42}, muted, LIBRARY_BACK_BUTTON, false);
    char path[200];
    const LibraryConsole *console = library_console_at(app->library_company, app->library_console_selection);
    snprintf(path, sizeof(path), "%s / %s%s%s", library_section_title(app), company->name,
             app->library_depth == LIBRARY_GAMES ? " / " : "",
             app->library_depth == LIBRARY_GAMES ? console->name : "");
    draw_single_line(app, app->font_small, path, CONTENT_X + 204, 36, muted,
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
    draw_text(app, app->font_hero_title, companies ? library_section_title(app) : company->name,
              CONTENT_X, 100, white, 1300);
    draw_text(app, app->font_small, TR(app, companies ? TXT_CHOOSE_COMPANY : TXT_CHOOSE_CONSOLE),
              CONTENT_X, 178, muted, 1200);
    for (int i = first; i < count && i < first + LIBRARY_PAGE_SIZE; i++) {
        SDL_Rect tile = library_tile_target(i);
        const LibraryCompany *brand = companies ? library_company_at(i) : company;
        const LibraryConsole *console = companies ? NULL : library_console_at(app->library_company, i);
        bool focused = !app->sidebar_focus && i == selected;
        fill_rect(r, tile.x, tile.y, tile.w, tile.h, 24, 24, 24, 255);
        SDL_Rect image = companies ? (SDL_Rect){tile.x + 32, tile.y + 66, tile.w - 64, 144} :
                                    (SDL_Rect){tile.x + 16, tile.y + 14, tile.w - 32, 194};
        const char *artwork = companies ? brand->artwork_path :
            (console->artwork_path ? console->artwork_path : console->logo_path);
        if (!render_library_artwork(app, artwork, image)) {
            draw_fitted_text(app, app->font_row_title, companies ? brand->name : console->name,
                             image, white, image, true);
        }
        if (!companies) {
            SDL_Rect logo = {tile.x + 36, tile.y + 220, tile.w - 72, 36};
            if (console->artwork_path) render_library_artwork(app, console->logo_path, logo);
            SDL_Rect title = {tile.x + 16, tile.y + 264, tile.w - 32, 24};
            draw_fitted_text(app, app->font_micro, console->name, title, white, title, true);
        }
        char subtitle[100];
        TextId count_label = companies ? TXT_CONSOLE_COUNT :
            (app->active_row == VIEW_EMULATORS ? TXT_LIBRARY_EMULATOR_COUNT : TXT_LIBRARY_GAME_COUNT);
        snprintf(subtitle, sizeof(subtitle), TR(app, count_label), companies ? brand->console_count :
                 (app->active_row == VIEW_EMULATORS ? app->rows[VIEW_EMULATORS].item_count : library_console_game_count(app, console)));
        SDL_Rect footer = {tile.x + 24, tile.y + (companies ? 256 : 292), tile.w - 48, companies ? 32 : 20};
        draw_fitted_text(app, companies ? app->font_small : app->font_micro,
                         subtitle, footer, muted, footer, true);
        if (focused) draw_border(r, tile.x, tile.y, tile.w, tile.h, 3, 245, 245, 245, 255);
    }
    char position[50];
    snprintf(position, sizeof(position), "%d / %d", selected + 1, count);
    draw_text(app, app->font_micro, position, CONTENT_X, 910, muted, 0);
    draw_text(app, app->font_micro, TR(app, app->sidebar_focus ?
              TXT_LIBRARY_ENTER_HINT : TXT_LIBRARY_NAV_HINT), CONTENT_X, 944, muted, 1200);
    render_library_breadcrumb(app);
}

void ui_render(WolodriveApp *app) {
    SDL_Renderer *r = app->renderer;

    /* Queue selection first, followed by visible cards and a small nearby window. */
    if (app->detail_open && app->detail_item) artwork_prefetch(app, app->detail_item, 110);
    if (app->active_row == VIEW_SEARCH) {
        if (app->search_count) artwork_prefetch(app, app->search_results[app->search_selected], 100);
        int start = app->search_selected / SEARCH_PAGE * SEARCH_PAGE;
        for (int i = SDL_max(0, start-3); i < SDL_min(app->search_count, start+SEARCH_PAGE+3); i++)
            artwork_prefetch(app, app->search_results[i], i >= start && i < start+SEARCH_PAGE ? 80 : 10);
    } else {
        CarouselRow *row = &app->rows[app->active_row];
        if (row->item_count) artwork_prefetch(app, row->items[row->selected_col], 100);
        for (int i = SDL_max(0, row->selected_col-2); i < SDL_min(row->item_count, row->selected_col+7); i++)
            artwork_prefetch(app, row->items[i], 10);
    }

    /* The presented backbuffer is undefined; clear before drawing scaled layers. */
    SDL_SetRenderDrawColor(r, 0, 0, 0, 255);
    SDL_RenderClear(r);

    if (app->active_row == VIEW_SETTINGS) {
        render_settings(app);
    } else if (app->active_row == VIEW_SEARCH) {
        render_search(app);
    } else if (is_library_view(app->active_row) && app->library_depth != LIBRARY_GAMES) {
        render_library_grid(app);
    } else {
        render_hero_banner(app);
        render_carousels(app);
        if (is_library_view(app->active_row)) render_library_breadcrumb(app);
    }
    render_sidebar(app);
    render_top_bar(app);
    if (app->detail_open) render_game_details(app);
    render_toast_notification(app);
    render_language_menu(app);

    SDL_RenderPresent(r);
}
