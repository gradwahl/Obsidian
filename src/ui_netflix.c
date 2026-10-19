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
static const SDL_Rect LANGUAGE_BUTTON = {1510, 14, 360, 44};
static const SDL_Rect LANGUAGE_MENU = {1510, 66, 360, LANG_COUNT * 44 + 16};
static const SDL_Rect DESCRIPTION_AREA = {142, 320, 690, 142};
static const SDL_Rect DESCRIPTION_BUTTON = {142, 478, 330, 42};
enum { RAIL_WIDTH = 104, CONTENT_X = 142, CARDS_Y = 600,
       CARD_WIDTH = 350, CARD_HEIGHT = 198, CARD_GAP = 12, CARD_FOOTER = 44 };

static SDL_Rect sidebar_target(int row) {
    return (SDL_Rect){0, 366 + row * 90, RAIL_WIDTH, 90};
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

/* Fit long translated actions inside the card without dropping their ending. */
static void draw_card_action(SDL_Renderer *r, TTF_Font *font, const char *text,
                             SDL_Rect card, SDL_Color color, SDL_Rect clip) {
    SDL_Surface *surface = TTF_RenderUTF8_Blended(font, text, color);
    if (!surface) return;
    SDL_Texture *texture = SDL_CreateTextureFromSurface(r, surface);
    if (texture) {
        float scale = fminf(1.0f, (float)(card.w - 24) / surface->w);
        int w = (int)(surface->w * scale), h = (int)(surface->h * scale);
        SDL_Rect destination = {card.x + (card.w - w) / 2,
                                card.y + CARD_HEIGHT + (CARD_FOOTER - h) / 2, w, h};
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
    if (app->font_logo)       TTF_CloseFont(app->font_logo);
    if (app->font_hero_title) TTF_CloseFont(app->font_hero_title);
    if (app->font_row_title)  TTF_CloseFont(app->font_row_title);
    if (app->font_body)       TTF_CloseFont(app->font_body);
    if (app->font_small)      TTF_CloseFont(app->font_small);
    if (app->font_micro)      TTF_CloseFont(app->font_micro);
}

void ui_rebuild_carousels(WolodriveApp *app) {
    const char *titles[TOTAL_ROWS] = {
        TR(app, TXT_ROW_RECENT),
        TR(app, TXT_ROW_PS5),
        TR(app, TXT_ROW_PS4),
        TR(app, TXT_ROW_DOWNLOADS)
    };

    SDL_LockMutex(app->catalog_mutex);

    for (int r = 0; r < TOTAL_ROWS; r++) {
        snprintf(app->rows[r].title, sizeof(app->rows[r].title), "%s", titles[r]);
        app->rows[r].item_count = 0;
    }

    for (int i = 0; i < app->catalog_count; i++) {
        GameItem *it = &app->catalog[i];

        if (app->rows[0].item_count < MAX_ITEMS_PER_ROW) {
            app->rows[0].items[app->rows[0].item_count++] = it;
        }
        if ((it->format == FMT_EXFAT || it->format == FMT_FFPKG) && app->rows[1].item_count < MAX_ITEMS_PER_ROW) {
            app->rows[1].items[app->rows[1].item_count++] = it;
        }
        if (it->format == FMT_PKG && app->rows[2].item_count < MAX_ITEMS_PER_ROW) {
            app->rows[2].items[app->rows[2].item_count++] = it;
        }
        if ((it->state == STATE_DOWNLOADING || it->state == STATE_INSTALLED) &&
            app->rows[3].item_count < MAX_ITEMS_PER_ROW) {
            app->rows[3].items[app->rows[3].item_count++] = it;
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

void ui_handle_input(WolodriveApp *app, const SDL_Event *ev) {
    if (handle_language_input(app, ev)) return;
    if (ev->type == SDL_MOUSEBUTTONDOWN && ev->button.button == SDL_BUTTON_LEFT) {
        for (int r = 0; r < TOTAL_ROWS; r++) {
            if (contains_point(sidebar_target(r), ev->button.x, ev->button.y)) {
                app->active_row = r;
                track_description_focus(app, get_focused_item(app));
                return;
            }
        }
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
                if (app->active_row > 0) app->active_row--;
                break;
            case SDLK_DOWN:
            case SDLK_s:
                if (app->active_row + 1 < TOTAL_ROWS) {
                    app->active_row++;
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
                if (app->active_row > 0) app->active_row--;
                break;
            case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
                if (app->active_row + 1 < TOTAL_ROWS) {
                    app->active_row++;
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

static void render_hero_banner(WolodriveApp *app) {
    SDL_Renderer *r = app->renderer;
    GameItem *hero = get_focused_item(app);
    SDL_Color white = {245, 245, 245, 255};
    SDL_Color muted = {185, 185, 185, 255};
    if (!hero) {
        draw_text(r, app->font_hero_title, app->rows[app->active_row].title,
                  CONTENT_X, 150, white, 900);
        draw_text(r, app->font_body, TR(app, TXT_NO_PACKAGES), CONTENT_X, 320, muted, 690);
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
    draw_text(r, app->font_row_title, row->title, CONTENT_X, 552, white, 0);
    if (!row->item_count) {
        draw_text(r, app->font_body, TR(app, TXT_NO_PACKAGES), CONTENT_X, CARDS_Y, muted, 1000);
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
        draw_card_action(r, app->font_small, status, card,
                         selected ? (SDL_Color){0, 0, 0, 255} : muted, footer_clip);
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
        int x = 38, y = target.y + 30;
        if (selected) fill_rect(r, 0, target.y, 4, target.h, 229, 9, 20, 255);
        if (row == 0) {
            SDL_SetRenderDrawColor(r, c, c, c, 255);
            SDL_RenderDrawLine(r, x - 2, y + 12, x + 14, y - 1);
            SDL_RenderDrawLine(r, x + 14, y - 1, x + 30, y + 12);
            draw_border(r, x + 2, y + 12, 24, 20, 2, c, c, c, 255);
            fill_rect(r, x + 11, y + 21, 7, 11, c, c, c, 255);
        } else if (row == 1 || row == 2) {
            draw_border(r, x, y + 2, 30, 23, 2, c, c, c, 255);
            fill_rect(r, x + 12, y + 25, 6, 5, c, c, c, 255);
            fill_rect(r, x + 6, y + 30, 18, 2, c, c, c, 255);
            draw_text(r, app->font_micro, row == 1 ? "5" : "4", x + 10, y + 1,
                      (SDL_Color){c, c, c, 255}, 0);
        } else {
            fill_rect(r, x + 13, y, 3, 22, c, c, c, 255);
            SDL_SetRenderDrawColor(r, c, c, c, 255);
            SDL_RenderDrawLine(r, x + 6, y + 15, x + 14, y + 23);
            SDL_RenderDrawLine(r, x + 14, y + 23, x + 23, y + 15);
            fill_rect(r, x, y + 29, 30, 2, c, c, c, 255);
        }
    }
}



static void render_top_bar(WolodriveApp *app) {
    SDL_Renderer *r = app->renderer;

    SDL_Color red = {229, 9, 20, 255};
    SDL_Color white = {245, 245, 245, 255};
    SDL_Color muted = {155, 155, 155, 255};
    draw_text(r, app->font_logo, "WOLODRIVE", 1620, 956, red, 0);
    draw_text(r, app->font_micro, "← →  ↑ ↓ / D-pad    X / Enter", CONTENT_X, 984, muted, 0);
    draw_single_line(r, app->font_micro, TR(app, TXT_BACK_HINT), 520, 984, muted,
                     (SDL_Rect){520, 984, 260, 30});
    draw_single_line(r, app->font_micro, TR(app, TXT_REFRESH_HINT), 800, 984, muted,
                     (SDL_Rect){800, 984, 600, 30});

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
                     (SDL_Rect){CONTENT_X, 1032, 1040, 32});
    char language_hint[96];
    snprintf(language_hint, sizeof(language_hint), TR(app, TXT_LANGUAGE),
             i18n_language_name((Language)SDL_AtomicGet(&app->language)));
    fill_rect(r, LANGUAGE_BUTTON.x, LANGUAGE_BUTTON.y, LANGUAGE_BUTTON.w, LANGUAGE_BUTTON.h,
              35, 35, 43, 255);
    draw_border(r, LANGUAGE_BUTTON.x, LANGUAGE_BUTTON.y, LANGUAGE_BUTTON.w, LANGUAGE_BUTTON.h,
                1, app->language_menu_open ? 229 : 110, 65, 75, 255);
    draw_single_line(r, app->font_small, language_hint, LANGUAGE_BUTTON.x + 14, 27, white,
                     (SDL_Rect){LANGUAGE_BUTTON.x + 12, 14, LANGUAGE_BUTTON.w - 90, 44});
    draw_text(r, app->font_micro, "L / □", LANGUAGE_BUTTON.x + LANGUAGE_BUTTON.w - 65, 28, muted, 0);

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

void ui_render(WolodriveApp *app) {
    SDL_Renderer *r = app->renderer;

    /* The presented backbuffer is undefined; clear before drawing scaled layers. */
    SDL_SetRenderDrawColor(r, 0, 0, 0, 255);
    SDL_RenderClear(r);

    render_hero_banner(app);
    render_carousels(app);
    render_sidebar(app);
    render_top_bar(app);
    render_toast_notification(app);
    render_language_menu(app);

    SDL_RenderPresent(r);
}
