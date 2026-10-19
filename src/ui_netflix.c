#include "wolodrive.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

static const char *FONT_CANDIDATES_BOLD[] = {
    "/data/homebrew/wolodrive/assets/font_bold.ttf",
    "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
    "/usr/share/fonts/truetype/ubuntu/Ubuntu-B.ttf",
    "/usr/share/fonts/truetype/liberation/LiberationSans-Bold.ttf",
    "/system_ex/app/NPXS40028/psm/Application/resource/font.ttf",
    NULL
};

static const char *FONT_CANDIDATES_REGULAR[] = {
    "/data/homebrew/wolodrive/assets/font.ttf",
    "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
    "/usr/share/fonts/truetype/ubuntu/Ubuntu-R.ttf",
    "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
    "/system_ex/app/NPXS40028/psm/Application/resource/font.ttf",
    NULL
};

static TTF_Font *open_first_available_font(const char **candidates, int ptsize) {
    for (int i = 0; candidates[i] != NULL; i++) {
        TTF_Font *f = TTF_OpenFont(candidates[i], ptsize);
        if (f) return f;
    }
    return NULL;
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
    if (steps > h) steps = h;
    for (int i = 0; i < steps; i++) {
        float t = (float)i / (float)(steps - 1);
        int slice_y = y + (i * h) / steps;
        int next_y  = y + ((i + 1) * h) / steps;
        int slice_h = next_y - slice_y;
        uint8_t cr = (uint8_t)(r1 + (r2 - r1) * t);
        uint8_t cg = (uint8_t)(g1 + (g2 - g1) * t);
        uint8_t cb = (uint8_t)(b1 + (b2 - b1) * t);
        uint8_t ca = (uint8_t)(a1 + (a2 - a1) * t);
        fill_rect(r, x, slice_y, w, slice_h, cr, cg, cb, ca);
    }
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

/* Świecąca ramka (Glow Effect) wokół zaznaczonego kafelka */
static void draw_glow_frame(SDL_Renderer *r, int x, int y, int w, int h,
                            uint8_t cr, uint8_t cg, uint8_t cb, float pulse) {
    int glow_layers = 6;
    float intensity = 0.80f + 0.20f * sinf(pulse * 4.5f);

    for (int i = glow_layers; i >= 1; i--) {
        uint8_t alpha = (uint8_t)((120.0f / (float)(i + 1)) * intensity);
        int expand = i * 2;
        draw_border(r, x - expand, y - expand, w + expand * 2, h + expand * 2, 2, cr, cg, cb, alpha);
    }
    /* Główna, ostra ramka Netflix Red (#E50914) */
    draw_border(r, x - 3, y - 3, w + 6, h + 6, 3, cr, cg, cb, 255);
    /* Subtelny biały obrys wewnętrzny nadający ostrość krawędzi okładki */
    draw_border(r, x, y, w, h, 1, 255, 255, 255, 165);
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

/* Rysuje elegancką plakietkę (Glassmorphism Badge) w rogu kafelka lub w Hero Bannerze */
static int draw_glass_badge(SDL_Renderer *r, TTF_Font *font, const char *label,
                            int x, int y, int pad_x, int pad_y,
                            uint8_t bg_r, uint8_t bg_g, uint8_t bg_b, uint8_t bg_a,
                            uint8_t accent_r, uint8_t accent_g, uint8_t accent_b, bool left_bar) {
    if (!font || !label || !label[0]) return 0;
    int tw = 0, th = 0;
    TTF_SizeUTF8(font, label, &tw, &th);
    int bw = tw + pad_x * 2 + (left_bar ? 4 : 0);
    int bh = th + pad_y * 2;

    fill_rect(r, x, y, bw, bh, bg_r, bg_g, bg_b, bg_a);
    draw_border(r, x, y, bw, bh, 1, 255, 255, 255, 42);
    if (left_bar) {
        fill_rect(r, x, y, 3, bh, accent_r, accent_g, accent_b, 255);
    }

    SDL_Color white = { 255, 255, 255, 255 };
    draw_text(r, font, label, x + pad_x + (left_bar ? 4 : 0), y + pad_y, white, 0);
    return bw;
}

/* Rysuje ikonę przycisku kontrolera DualSense ([X], [O], [Trójkąt]) + podpis */
static int draw_dualsense_prompt(SDL_Renderer *r, TTF_Font *font_sym, TTF_Font *font_lbl,
                                 const char *symbol, const char *label,
                                 int x, int y, uint8_t sym_r, uint8_t sym_g, uint8_t sym_b) {
    int btn_size = 32;
    /* Cień pod przyciskiem */
    fill_rect(r, x + 2, y + 3, btn_size, btn_size, 0, 0, 0, 140);
    /* Tło przycisku DualSense */
    fill_rect(r, x, y, btn_size, btn_size, 28, 28, 34, 240);
    draw_border(r, x, y, btn_size, btn_size, 2, sym_r, sym_g, sym_b, 230);

    SDL_Color sym_col = { sym_r, sym_g, sym_b, 255 };
    SDL_Color lbl_col = { 235, 235, 240, 255 };

    int sw = 0, sh = 0;
    TTF_SizeUTF8(font_sym, symbol, &sw, &sh);
    draw_text(r, font_sym, symbol, x + (btn_size - sw) / 2, y + (btn_size - sh) / 2, sym_col, 0);

    int lw = draw_text(r, font_lbl, label, x + btn_size + 10, y + 5, lbl_col, 0);
    return btn_size + 10 + lw + 28;
}

/* Leniwe ładowanie tekstury okładki z lokalnego cache'u dyskowego w wątku renderującym */
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
            draw_text(r, app->font_micro, "POBIERANIE OKŁADKI...", x + 18, y + h / 2 + 4, sub_col, 0);
        } else {
            draw_text(r, app->font_micro, it->title, x + 18, y + h / 2 + 4, sub_col, w - 36);
        }
    }
}

bool ui_init(WolodriveApp *app) {
    app->font_logo       = open_first_available_font(FONT_CANDIDATES_BOLD, 32);
    app->font_hero_title = open_first_available_font(FONT_CANDIDATES_BOLD, 46);
    app->font_row_title  = open_first_available_font(FONT_CANDIDATES_BOLD, 22);
    app->font_body       = open_first_available_font(FONT_CANDIDATES_REGULAR, 19);
    app->font_small      = open_first_available_font(FONT_CANDIDATES_BOLD, 15);
    app->font_micro      = open_first_available_font(FONT_CANDIDATES_BOLD, 13);

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
        "Ostatnio dodane (Katalog Pegasus-DL)",
        "Gry PS5 (.exfat / .ffpkg)",
        "Gry PS4 (.pkg)",
        "W trakcie pobierania i zainstalowane"
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

void ui_handle_input(WolodriveApp *app, const SDL_Event *ev) {
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
                if (app->active_row + 1 < TOTAL_ROWS && app->rows[app->active_row + 1].item_count > 0) {
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
                if (app->active_row + 1 < TOTAL_ROWS && app->rows[app->active_row + 1].item_count > 0) {
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

    const float card_stride = 328.0f; /* 298px szerokość kafelka + 30px odstęp */
    for (int r = 0; r < TOTAL_ROWS; r++) {
        CarouselRow *row = &app->rows[r];
        if (row->selected_col >= 4) {
            row->target_scroll_x = (float)(row->selected_col - 3) * card_stride;
        } else {
            row->target_scroll_x = 0.0f;
        }
        row->scroll_x += (row->target_scroll_x - row->scroll_x) * fminf(1.0f, dt * 12.0f);
    }

    float row_height = 248.0f;
    if (app->active_row >= 1) {
        app->target_vertical_scroll_y = (float)(app->active_row) * (row_height * 0.74f);
    } else {
        app->target_vertical_scroll_y = 0.0f;
    }
    app->vertical_scroll_y += (app->target_vertical_scroll_y - app->vertical_scroll_y) * fminf(1.0f, dt * 10.0f);
}

/* Subtelny pionowy i radialny gradient tła (#141414 -> #080808) oraz efekt winiety */
static void render_background_and_ambient(WolodriveApp *app) {
    SDL_Renderer *r = app->renderer;
    GameItem *hero = get_focused_item(app);

    /* 1. Pionowy gradient od głębokiego antracytu (#141414) do czystej czerni (#080808) */
    fill_vertical_gradient(r, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT,
                           COLOR_BG_TOP_R, COLOR_BG_TOP_G, COLOR_BG_TOP_B, 255,
                           COLOR_BG_BOT_R, COLOR_BG_BOT_G, COLOR_BG_BOT_B, 255,
                           64);

    /* 2. Radialna poświata ambientowa (Ambient Glow) z koloru wybranej gry w górnej strefie ekranu */
    if (hero) {
        int cx = 560, cy = 240;
        for (int i = 16; i >= 1; i--) {
            int rw = i * 95;
            int rh = i * 36;
            uint8_t alpha = (uint8_t)(12 + (16 - i) * 2);
            fill_rect(r, cx - rw / 2, cy - rh / 2, rw, rh,
                      hero->cover_r, hero->cover_g, hero->cover_b, alpha);
        }
    }
}

static void render_vignette(SDL_Renderer *r) {
    /* Dolna i boczna winieta dodająca filmowej głębi 10-foot TV UI */
    fill_vertical_gradient(r, 0, SCREEN_HEIGHT - 95, SCREEN_WIDTH, 95,
                           0, 0, 0, 0,
                           0, 0, 0, 185, 20);
    for (int i = 0; i < 16; i++) {
        uint8_t a = (uint8_t)((16 - i) * 6);
        fill_rect(r, i * 4, 0, 4, SCREEN_HEIGHT, 0, 0, 0, a);
        fill_rect(r, SCREEN_WIDTH - (i + 1) * 4, 0, 4, SCREEN_HEIGHT, 0, 0, 0, a);
    }
}

static void render_hero_banner(WolodriveApp *app) {
    SDL_Renderer *r = app->renderer;
    GameItem *hero = get_focused_item(app);
    if (!hero) return;

    /* Cieniowane tło Hero Bannera (górne 45% ekranu = 486px) */
    fill_vertical_gradient(r, 0, TOPBAR_HEIGHT, SCREEN_WIDTH, HERO_HEIGHT - TOPBAR_HEIGHT,
                           (uint8_t)(hero->cover_r * 0.36f), (uint8_t)(hero->cover_g * 0.36f), (uint8_t)(hero->cover_b * 0.36f), 165,
                           COLOR_BG_TOP_R, COLOR_BG_TOP_G, COLOR_BG_TOP_B, 245,
                           36);

    SDL_Color white = { COLOR_TEXT_R, COLOR_TEXT_G, COLOR_TEXT_B, 255 };
    SDL_Color muted = { COLOR_MUTED_R, COLOR_MUTED_G, COLOR_MUTED_B, 255 };

    /* Prawa kolumna Hero Bannera: Duża karta prezentacji okładki z głębokim cieniem */
    int art_x = 1340, art_y = 94, art_w = 488, art_h = 342;
    draw_drop_shadow(r, art_x, art_y, art_w, art_h, 12, 10, 210);
    render_cover_or_placeholder(app, hero, art_x, art_y, art_w, art_h, true);

    /* Dolny gradient Glassmorphism na dużej okładce Hero */
    fill_vertical_gradient(r, art_x, art_y + art_h - 115, art_w, 115,
                           8, 8, 12, 0,
                           8, 8, 12, 235, 20);

    draw_glass_badge(r, app->font_small, hero->id_code, art_x + 18, art_y + 16, 10, 5,
                     14, 14, 18, 215, COLOR_ACCENT_R, COLOR_ACCENT_G, COLOR_ACCENT_B, true);

    char ver_badge[64];
    snprintf(ver_badge, sizeof(ver_badge), "v%s  •  %s", hero->version, sys_format_short_badge(hero->format));
    draw_glass_badge(r, app->font_small, ver_badge, art_x + 18, art_y + art_h - 46, 10, 5,
                     20, 20, 26, 220, 255, 255, 255, false);

    draw_border(r, art_x, art_y, art_w, art_h, 2, 255, 255, 255, 55);

    /* Lewa kolumna informacyjna w stylu Netflix / PS5 OS */
    int left_x = 80;
    fill_rect(r, left_x, 96, 5, 24, COLOR_ACCENT_R, COLOR_ACCENT_G, COLOR_ACCENT_B, 255);

    char series_hdr[160];
    snprintf(series_hdr, sizeof(series_hdr), "WOLODRIVE STORE   •   ŹRÓDŁO: %s", hero->source_name);
    draw_text(r, app->font_small, series_hdr, left_x + 15, 99, muted, 0);

    /* Duży tytuł gry z delikatnym cieniem pod literami */
    SDL_Color shadow_col = { 0, 0, 0, 190 };
    draw_text(r, app->font_hero_title, hero->title, left_x + 2, 132, shadow_col, 1200);
    draw_text(r, app->font_hero_title, hero->title, left_x, 130, white, 1200);

    /* Rząd eleganckich plakietek metadanych (ID, Rozmiar, Format, Ścieżka) */
    int bx = left_x;
    int by = 198;
    bx += draw_glass_badge(r, app->font_small, hero->id_code, bx, by, 12, 5,
                           24, 24, 30, 220, COLOR_ACCENT_R, COLOR_ACCENT_G, COLOR_ACCENT_B, true) + 12;

    char size_lbl[48];
    snprintf(size_lbl, sizeof(size_lbl), "ROZMIAR: %.1f GB", hero->size_gb);
    bx += draw_glass_badge(r, app->font_small, size_lbl, bx, by, 12, 5,
                           24, 24, 30, 220, 180, 180, 190, false) + 12;

    bx += draw_glass_badge(r, app->font_small, sys_format_to_string(hero->format), bx, by, 12, 5,
                           24, 24, 30, 220, 180, 180, 190, false) + 12;

    /* Elegancki separator poziomy z czerwonym akcentem */
    fill_rect(r, left_x, 240, 90, 2, COLOR_ACCENT_R, COLOR_ACCENT_G, COLOR_ACCENT_B, 255);
    fill_rect(r, left_x + 90, 240, 1080, 1, 255, 255, 255, 30);

    /* Opis gry (#94949C) */
    draw_text(r, app->font_body, hero->description, left_x, 254, muted, 1180);

    /* Docelowa ścieżka montażu / instalacji */
    char path_line[320];
    snprintf(path_line, sizeof(path_line), "Cel instalacji: %s", hero->install_path);
    draw_text(r, app->font_small, path_line, left_x, 314, muted, 0);

    /* Główny przycisk akcji (#E50914) z cieniem */
    int btn_y = 354;
    int btn_w = 340;
    int btn_h = 52;
    draw_drop_shadow(r, left_x, btn_y, btn_w, btn_h, 6, 4, 180);
    fill_rect(r, left_x, btn_y, btn_w, btn_h, COLOR_ACCENT_R, COLOR_ACCENT_G, COLOR_ACCENT_B, 255);
    draw_border(r, left_x, btn_y, btn_w, btn_h, 1, 255, 255, 255, 95);

    const char *btn_label = "POBIERZ / ZAINSTALUJ";
    if (hero->state == STATE_DOWNLOADING) {
        btn_label = "POBIERANIE W TOKU...";
    } else if (hero->state == STATE_INSTALLED) {
        btn_label = (hero->format == FMT_PKG) ? "ZAINSTALOWANO (FPKG)" : "ZAMONTUJ SHADOWMOUNT";
    }
    draw_text(r, app->font_small, btn_label, left_x + 26, btn_y + 16, white, 0);

    /* Pasek postępu w Hero Bannerze dla aktywnie pobieranej gry */
    if (hero->state == STATE_DOWNLOADING || hero->state == STATE_INSTALLED) {
        int bar_x = left_x + btn_w + 26;
        int bar_w = 380;
        fill_rect(r, bar_x, btn_y + 15, bar_w, 22, 32, 32, 38, 240);
        int fill_w = (int)((hero->progress / 100.0f) * bar_w);
        fill_rect(r, bar_x, btn_y + 15, fill_w, 22, COLOR_ACCENT_R, COLOR_ACCENT_G, COLOR_ACCENT_B, 255);
        draw_border(r, bar_x, btn_y + 15, bar_w, 22, 1, 255, 255, 255, 45);

        char prog_txt[64];
        snprintf(prog_txt, sizeof(prog_txt), "%.1f%% (%.1f GB)", hero->progress, hero->size_gb);
        draw_text(r, app->font_small, prog_txt, bar_x + bar_w + 16, btn_y + 16, white, 0);
    }

    /* Pasek legendy przycisków kontrolera DualSense na dole Hero Bannera */
    int ds_x = left_x;
    int ds_y = 428;
    ds_x += draw_dualsense_prompt(r, app->font_small, app->font_small,
                                  "X", "Pobierz / Zamontuj", ds_x, ds_y, 85, 165, 255);
    ds_x += draw_dualsense_prompt(r, app->font_small, app->font_small,
                                  "O", "Wstecz / Anuluj", ds_x, ds_y, 255, 95, 95);
    ds_x += draw_dualsense_prompt(r, app->font_small, app->font_small,
                                  "Δ", "Odśwież katalog Pegasus-DL [R]", ds_x, ds_y, 65, 220, 135);

    /* Dolny separator Hero Bannera */
    fill_rect(r, 0, HERO_HEIGHT - 2, SCREEN_WIDTH, 2, 255, 255, 255, 22);
}

static void render_carousels(WolodriveApp *app) {
    SDL_Renderer *r = app->renderer;
    SDL_Color white = { COLOR_TEXT_R, COLOR_TEXT_G, COLOR_TEXT_B, 255 };
    SDL_Color muted = { COLOR_MUTED_R, COLOR_MUTED_G, COLOR_MUTED_B, 255 };

    int start_y = HERO_HEIGHT + 18 - (int)app->vertical_scroll_y;
    const int row_spacing = 248;
    const int card_w = 298;
    const int card_h = 168;
    const int card_gap = 30;

    for (int row_idx = 0; row_idx < TOTAL_ROWS; row_idx++) {
        CarouselRow *row = &app->rows[row_idx];
        int y = start_y + row_idx * row_spacing;

        if (y < HERO_HEIGHT - 36 || y > SCREEN_HEIGHT - 35) continue;

        bool is_active_row = (row_idx == app->active_row);

        /* Akcent przed tytułem aktywnego rzędu */
        if (is_active_row) {
            fill_rect(r, 66, y + 4, 4, 20, COLOR_ACCENT_R, COLOR_ACCENT_G, COLOR_ACCENT_B, 255);
        }

        char row_header[128];
        snprintf(row_header, sizeof(row_header), "%s  (%d)", row->title, row->item_count);
        draw_text(r, app->font_row_title, row_header, 80, y, is_active_row ? white : muted, 0);

        int cards_y = y + 38;

        /* KROK 1: Rysujemy wszystkie nieaktywne kafelki w rzędzie */
        for (int pass = 0; pass < 2; pass++) {
            for (int col = 0; col < row->item_count; col++) {
                bool is_selected = (is_active_row && col == row->selected_col);
                if ((pass == 0 && is_selected) || (pass == 1 && !is_selected)) continue;

                GameItem *it = row->items[col];
                int card_x = 80 + col * (card_w + card_gap) - (int)row->scroll_x;
                if (card_x + card_w < -40 || card_x > SCREEN_WIDTH + 40) continue;

                /* Efekt Scale-Up / Zoom dla zaznaczonego kafelka */
                int zoom_w = is_selected ? 22 : 0;
                int zoom_h = is_selected ? 14 : 0;
                int lift_y = is_selected ? -6 : 0;

                int draw_x = card_x - zoom_w / 2;
                int draw_y = cards_y - zoom_h / 2 + lift_y;
                int draw_w = card_w + zoom_w;
                int draw_h = card_h + zoom_h;

                /* 1. Wielowarstwowy cień (Drop Shadow) pod kafelkiem – mocniejszy dla wybranego */
                if (is_selected) {
                    draw_drop_shadow(r, draw_x, draw_y, draw_w, draw_h, 12, 10, 235);
                } else {
                    draw_drop_shadow(r, draw_x, draw_y, draw_w, draw_h, 6, 6, 155);
                }

                /* 2. Okładka z cache'u (SDL2_image) lub stylowy placeholder Wolodrive */
                render_cover_or_placeholder(app, it, draw_x, draw_y, draw_w, draw_h, false);

                /* 3. Półprzezroczysty gradient (Glassmorphism / ciemny od dołu) dla idealnej czytelności tytułu */
                int grad_h = (int)(draw_h * 0.58f);
                fill_vertical_gradient(r, draw_x, draw_y + draw_h - grad_h, draw_w, grad_h,
                                       10, 10, 14, 0,
                                       8, 8, 12, 242,
                                       22);

                /* Górny delikatny gradient pod plakietki narożne */
                fill_vertical_gradient(r, draw_x, draw_y, draw_w, 42,
                                       0, 0, 0, 145,
                                       0, 0, 0, 0,
                                       10);

                /* 4. Schludne plakietki (Badges) w rogach kafelka */
                draw_glass_badge(r, app->font_micro, it->id_code,
                                 draw_x + 10, draw_y + 10, 7, 3,
                                 14, 14, 18, 215,
                                 COLOR_ACCENT_R, COLOR_ACCENT_G, COLOR_ACCENT_B, true);

                char right_badge[32];
                snprintf(right_badge, sizeof(right_badge), "%s • %.0fG",
                         sys_format_short_badge(it->format), it->size_gb);
                int rbw = 0, rbh = 0;
                TTF_SizeUTF8(app->font_micro, right_badge, &rbw, &rbh);
                draw_glass_badge(r, app->font_micro, right_badge,
                                 draw_x + draw_w - rbw - 24, draw_y + 10, 7, 3,
                                 18, 18, 24, 215, 255, 255, 255, false);

                /* 5. Biały tytuł gry oraz status na dole kafelka */
                draw_text(r, app->font_small, it->title,
                          draw_x + 12, draw_y + draw_h - 48, white, draw_w - 24);

                char sub_info[64];
                if (it->state == STATE_DOWNLOADING) {
                    snprintf(sub_info, sizeof(sub_info), "POBIERANIE: %.0f%%  •  %.1f GB", it->progress, it->size_gb);
                } else if (it->state == STATE_INSTALLED) {
                    snprintf(sub_info, sizeof(sub_info), "GOTOWE DO GRY  •  %.1f GB", it->size_gb);
                } else {
                    snprintf(sub_info, sizeof(sub_info), "%s  •  %.1f GB", it->source_name, it->size_gb);
                }
                draw_text(r, app->font_micro, sub_info,
                          draw_x + 12, draw_y + draw_h - 24, is_selected ? white : muted, draw_w - 24);

                /* Pasek postępu na dole kafelka */
                if (it->state == STATE_DOWNLOADING || it->state == STATE_INSTALLED) {
                    fill_rect(r, draw_x, draw_y + draw_h - 5, draw_w, 5, 35, 35, 42, 255);
                    int p_w = (int)((it->progress / 100.0f) * draw_w);
                    fill_rect(r, draw_x, draw_y + draw_h - 5, p_w, 5,
                              COLOR_ACCENT_R, COLOR_ACCENT_G, COLOR_ACCENT_B, 255);
                }

                /* 6. Obramowanie: Świecąca ramka (Glow Effect) dla wybranego kafelka */
                if (is_selected) {
                    draw_glow_frame(r, draw_x, draw_y, draw_w, draw_h,
                                    COLOR_ACCENT_R, COLOR_ACCENT_G, COLOR_ACCENT_B, app->focus_pulse);
                } else {
                    draw_border(r, draw_x, draw_y, draw_w, draw_h, 1, 255, 255, 255, 32);
                }
            }
        }
    }
}

static void render_top_bar(WolodriveApp *app) {
    SDL_Renderer *r = app->renderer;

    /* Wielowarstwowy cień (Drop Shadow) pod górnym paskiem nawigacyjnym */
    fill_vertical_gradient(r, 0, TOPBAR_HEIGHT, SCREEN_WIDTH, 26,
                           0, 0, 0, 195,
                           0, 0, 0, 0, 12);

    /* Tło Top Bara (Glassmorphism) */
    fill_rect(r, 0, 0, SCREEN_WIDTH, TOPBAR_HEIGHT, 10, 10, 14, 242);
    fill_rect(r, 0, TOPBAR_HEIGHT - 1, SCREEN_WIDTH, 1, 255, 255, 255, 32);

    SDL_Color red   = { COLOR_ACCENT_R, COLOR_ACCENT_G, COLOR_ACCENT_B, 255 };
    SDL_Color white = { COLOR_TEXT_R, COLOR_TEXT_G, COLOR_TEXT_B, 255 };
    SDL_Color muted = { COLOR_MUTED_R, COLOR_MUTED_G, COLOR_MUTED_B, 255 };

    /* Logo WOLODRIVE po lewej stronie */
    draw_text(r, app->font_logo, "WOLODRIVE", 80, 17, red, 0);

    char sync_info[160];
    SDL_LockMutex(app->catalog_mutex);
    snprintf(sync_info, sizeof(sync_info), "%s",
             app->sync_status_text[0] ? app->sync_status_text : "Pegasus-DL Ready");
    SDL_UnlockMutex(app->catalog_mutex);

    draw_text(r, app->font_small, sync_info, 305, 27, muted, 0);

    /* Prawa strona: /data, /mnt/ext0, status sieci i temperatura CPU/GPU */
    char telemetry[256];
    snprintf(telemetry, sizeof(telemetry),
             "/data: %.1f GB   |   /mnt/ext0: %.1f GB   |   %s   |   CPU: %d°C  GPU: %d°C",
             app->stats.data_free_gb,
             app->stats.ext0_free_gb,
             app->stats.ip_address,
             app->stats.cpu_temp_c,
             app->stats.gpu_temp_c);

    draw_text(r, app->font_small, telemetry, 980, 27, white, 0);
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

    render_background_and_ambient(app);
    render_hero_banner(app);
    render_carousels(app);
    render_vignette(r);
    render_top_bar(app);
    render_toast_notification(app);

    SDL_RenderPresent(r);
}
