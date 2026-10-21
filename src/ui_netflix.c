#include "wolodrive.h"
#include "artwork.h"
#include "emulator_download.h"
#include "screenshots.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include "ui_text.h"
#include "ui_glass.h"
static HighlightAnim nav_highlight, category_highlight, language_highlight;
static Uint32 language_close_at, sidebar_started;
static float sidebar_from, sidebar_to;
static int ui_height(SDL_Renderer *r) {
    int w=0,h=0; SDL_RenderGetLogicalSize(r,&w,&h);
    if(!h) SDL_GetRendererOutputSize(r,&w,&h);
    return h>0?h:SCREEN_HEIGHT;
}
static int category_y(SDL_Renderer *r) { return ui_height(r)-48-40; }
#ifdef __PS5__
#include "ps5_renderer.h"
#endif

static const char *FONT_CANDIDATES_BOLD[] = {
    "assets/font_bold.ttf",
    "assets/font_cjk.ttc",
    "assets/font.ttf",
    "/data/homebrew/wolodrive/assets/font_bold.ttf",
    "/data/homebrew/wolodrive/assets/font_cjk.ttc",
    "/data/homebrew/wolodrive/assets/font.ttf",
    "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
    "/usr/share/fonts/truetype/liberation/LiberationSans-Bold.ttf",
    "/usr/share/fonts/truetype/ubuntu/Ubuntu-B.ttf",
    "/usr/share/fonts/dejavu/DejaVuSans-Bold.ttf",
    "/usr/share/fonts/dejavu-sans-fonts/DejaVuSans-Bold.ttf",
    "/usr/share/fonts/opentype/noto/NotoSansCJK-Bold.ttc",
    "/usr/share/fonts/noto-cjk/NotoSansCJK-Bold.ttc",
    "/usr/share/fonts/truetype/freefont/FreeSansBold.ttf",
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
    NULL
};

static const char *FONT_CANDIDATES_REGULAR[] = {
    "assets/font.ttf",
    "assets/font_cjk.ttc",
    "assets/font_bold.ttf",
    "/data/homebrew/wolodrive/assets/font.ttf",
    "/data/homebrew/wolodrive/assets/font_cjk.ttc",
    "/data/homebrew/wolodrive/assets/font_bold.ttf",
    "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
    "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
    "/usr/share/fonts/truetype/ubuntu/Ubuntu-R.ttf",
    "/usr/share/fonts/dejavu/DejaVuSans.ttf",
    "/usr/share/fonts/dejavu-sans-fonts/DejaVuSans.ttf",
    "/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc",
    "/usr/share/fonts/noto-cjk/NotoSansCJK-Regular.ttc",
    "/usr/share/fonts/truetype/freefont/FreeSans.ttf",
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
        if (!fname) fname = strrchr(candidate, '\\');
        fname = fname ? fname + 1 : candidate;
        snprintf(resolved, sizeof(resolved), "%s\\Fonts\\%s", windir, fname);
        f = TTF_OpenFont(resolved, ptsize);
        if (f) return f;
    }
#endif
    return NULL;
}

static bool font_supports_polish(TTF_Font *f) {
    if (!f) return false;
    static const uint16_t pl_chars[] = {0x0105, 0x0107, 0x0119, 0x0142, 0x0144, 0x015b, 0x017a, 0x017c};
    for (size_t i = 0; i < sizeof(pl_chars)/sizeof(pl_chars[0]); i++) {
        if (!TTF_GlyphIsProvided(f, pl_chars[i])) return false;
    }
    return true;
}

static TTF_Font *open_first_available_font(const char **candidates, int ptsize) {
    for (int i = 0; candidates[i] != NULL; i++) {
        TTF_Font *f = open_font_with_path_resolution(candidates[i], ptsize);
        if (!f) continue;
        if (TTF_GlyphIsProvided(f, 0x4e2d) && TTF_GlyphIsProvided(f, 0x042f) && font_supports_polish(f)) return f;
        TTF_CloseFont(f);
    }
    for (int i = 0; candidates[i] != NULL; i++) {
        TTF_Font *f = open_font_with_path_resolution(candidates[i], ptsize);
        if (!f) continue;
        if (TTF_GlyphIsProvided(f, 0x042f) && font_supports_polish(f)) return f;
        TTF_CloseFont(f);
    }
    for (int i = 0; candidates[i] != NULL; i++) {
        TTF_Font *f = open_font_with_path_resolution(candidates[i], ptsize);
        if (!f) continue;
        if (font_supports_polish(f)) return f;
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
enum { SIDEBAR_COLLAPSED_W = 64, RAIL_WIDTH = 92, SIDEBAR_EXPANDED_W = 220,
       CONTENT_X = RAIL_WIDTH + 38, CARDS_Y = 576,
       CARD_WIDTH = 216, CARD_HEIGHT = 300, CARD_GAP = 16, CARD_TITLE = 28, CARD_FOOTER = 40,
       HOME_ROW_STRIDE = 442, HOME_GRID_COLS = 6 };
static const SDL_Rect LANGUAGE_BUTTON = {8, 8, SIDEBAR_EXPANDED_W - 16, 44};
static SDL_Rect language_button_rect(const WolodriveApp *app) {
    int w = (int)lroundf(app->sidebar_width);
    if (w < SIDEBAR_COLLAPSED_W) w = SIDEBAR_COLLAPSED_W;
    if (w > SIDEBAR_EXPANDED_W) w = SIDEBAR_EXPANDED_W;
    return w <= SIDEBAR_COLLAPSED_W + 8
        ? (SDL_Rect){10, 8, 44, 44}
        : (SDL_Rect){8, 8, w-16, 44};
}
/* Match the 220px expanded sidebar: 8px inset on either side. */
static const SDL_Rect LANGUAGE_MENU = {8, 92, SIDEBAR_EXPANDED_W - 16, LANG_COUNT * 44 + 16};
static const char *action_label(WolodriveApp *app, GameItem *item);
static const SDL_Rect DESCRIPTION_AREA = {CONTENT_X, 320, 690, 142};
static void get_hero_button_rects(WolodriveApp *app, GameItem *hero,
                                  SDL_Rect *play_rect, SDL_Rect *info_rect, SDL_Rect *toggle_rect) {
    int y = app->ui_theme == UI_THEME_UBIQUITY ? -500 : 470, h = 50;
    int current_x = CONTENT_X;
    TTF_Font *font = app->font_small ? app->font_small : app->font_body;

    char play_label[128] = {0};
    if (hero) snprintf(play_label, sizeof(play_label), "▶  %s", action_label(app, hero));
    int play_w = 210;
    if (font && play_label[0]) {
        int tw = 0, th = 0;
        TTF_SizeUTF8(font, play_label, &tw, &th);
        if (tw > 0) play_w = tw + 36;
    }
    if (play_w < 160) play_w = 160;
    if (play_rect) *play_rect = (SDL_Rect){current_x, y, play_w, h};
    current_x += play_w + 16;

    char info_label[128] = {0};
    if (hero) snprintf(info_label, sizeof(info_label), "ℹ  %s", TR(app, TXT_MORE_INFO));
    int info_w = 230;
    if (font && info_label[0]) {
        int tw = 0, th = 0;
        TTF_SizeUTF8(font, info_label, &tw, &th);
        if (tw > 0) info_w = tw + 36;
    }
    if (info_w < 180) info_w = 180;
    if (info_rect) *info_rect = (SDL_Rect){current_x, y, info_w, h};
    current_x += info_w + 16;

    if (toggle_rect) {
        if (hero && hero->description_id == TEXT_COUNT) {
            TextId label_id = app->show_original_description ? TXT_TRANSLATED_DESCRIPTION : TXT_ORIGINAL_DESCRIPTION;
            const char *label_str = TR(app, label_id);
            char toggle_text[160];
            snprintf(toggle_text, sizeof(toggle_text), "%s [I]", label_str);
            int toggle_w = 240;
            if (font && toggle_text[0]) {
                int tw = 0, th = 0;
                TTF_SizeUTF8(font, toggle_text, &tw, &th);
                if (tw > 0) toggle_w = tw + 44;
            }
            *toggle_rect = (SDL_Rect){current_x, y, toggle_w, h};
        } else {
            *toggle_rect = (SDL_Rect){0, 0, 0, 0};
        }
    }
}

static SDL_Rect get_show_all_button_rect(WolodriveApp *app, int header_y) {
    char show_all_label[96];
    snprintf(show_all_label, sizeof(show_all_label), "%s ›", TR(app, TXT_SHOW_ALL));
    int show_all_w = 200;
    if (app->font_micro) {
        int tw = 0, th = 0;
        TTF_SizeUTF8(app->font_micro, show_all_label, &tw, &th);
        if (tw > 0) show_all_w = tw + 32;
    }
    int show_all_x = SCREEN_WIDTH - 48 - show_all_w;
    return (SDL_Rect){show_all_x, header_y, show_all_w, 34};
}

enum { LIBRARY_COLUMNS = 4, LIBRARY_PAGE_SIZE = 8, LIBRARY_TILE = 320,
       LIBRARY_GAP_X = 36, LIBRARY_GAP_Y = 24, LIBRARY_GRID_Y = 220 };
static const SDL_Rect LIBRARY_BACK_BUTTON = {CONTENT_X, 24, 180, 50};

static SDL_Rect library_tile_target(int index) {
    int slot = index % LIBRARY_PAGE_SIZE;
    return (SDL_Rect){CONTENT_X + slot % LIBRARY_COLUMNS * (LIBRARY_TILE + LIBRARY_GAP_X),
                      LIBRARY_GRID_Y + slot / LIBRARY_COLUMNS * (LIBRARY_TILE + LIBRARY_GAP_Y),
                      LIBRARY_TILE, LIBRARY_TILE};
}

static int library_console_game_count(WolodriveApp *app, const LibraryConsole *console);

static bool library_matches_console(WolodriveApp *app, const GameItem *item) {
    const LibraryConsole *console = library_console_visible_at(app->library_company, app->library_console_selection, app->active_row == VIEW_EMULATORS);
    return item->is_game && (library_console_matches_id(console, item->id_code) ||
        (item->format == FMT_CATALOG && library_console_matches_platform(console, item->platform)));
}

static bool is_library_view(int view) {
    return view == VIEW_GAMES || view == VIEW_EMULATORS;
}

static const char *library_section_title(WolodriveApp *app) {
    return TR(app, app->active_row == VIEW_EMULATORS ? TXT_NAV_EMULATORS : TXT_NAV_GAMES);
}

static void select_sidebar_view(WolodriveApp *app, int view) {
    app->active_row = view;
    app->sidebar_focus = false;
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
    int y_offset = 148;
    int row_h = 48;
    int pos;
    if (row == VIEW_SEARCH) pos = 0;
    else if (row == VIEW_SETTINGS) pos = VIEW_DOWNLOADS + 2;
    else pos = row + 1;
    return (SDL_Rect){8, row == VIEW_SETTINGS ? SCREEN_HEIGHT - 96 : 16 + y_offset + pos * row_h, SIDEBAR_EXPANDED_W - 16, 44};
}

static CarouselRow *active_home_row(WolodriveApp *app) {
    if (app->active_row == VIEW_HOME && app->home_cat_index > 0 &&
        app->home_cat_index < HOME_CAT_COUNT &&
        app->home_categories[app->home_cat_index].item_count > 0) {
        return &app->home_categories[app->home_cat_index];
    }
    return &app->rows[app->active_row];
}

/* The Games/Roms console catalogue must not silently fall back to Netflix.
   Check actual library depth instead of reusing the Home grid-expanded flag. */
static bool ubiquity_carousel_enabled(WolodriveApp *app) {
    if (app->ui_theme != UI_THEME_UBIQUITY) return false;
    if (app->active_row == VIEW_HOME) return !app->home_grid_expanded;
    if (app->active_row == VIEW_GAMES)
        return app->library_depth == LIBRARY_GAMES;
    return app->active_row == VIEW_DOWNLOADS ||
           app->active_row == VIEW_FILES || app->active_row == VIEW_PAYLOADS;
}

static SDL_Rect card_target(WolodriveApp *app, int col) {
    CarouselRow *row = active_home_row(app);
    if (ubiquity_carousel_enabled(app)) {
        enum { UBQ_CARD_W = 232, UBQ_CARD_GAP = 20, UBQ_CARD_Y = 755, UBQ_CARD_H = 232 };
        return (SDL_Rect){CONTENT_X + 24 + col * (UBQ_CARD_W + UBQ_CARD_GAP) -
                          (int)(row->scroll_x * ((float)(UBQ_CARD_W + UBQ_CARD_GAP) /
                                                (CARD_WIDTH + CARD_GAP))),
                          UBQ_CARD_Y, UBQ_CARD_W, UBQ_CARD_H};
    }
    return (SDL_Rect){CONTENT_X + col * (CARD_WIDTH + CARD_GAP) -
                     (int)row->scroll_x,
                     CARDS_Y, CARD_WIDTH, CARD_HEIGHT + CARD_TITLE + CARD_FOOTER};
}

static SDL_Rect home_row_card_target(WolodriveApp *app, int cat, int col) {
    CarouselRow *row = &app->home_categories[cat];
    if (app->ui_theme == UI_THEME_UBIQUITY && !app->home_grid_expanded) {
        enum { UBQ_CARD_W = 232, UBQ_CARD_GAP = 20, UBQ_CARD_Y = 755, UBQ_CARD_H = 232 };
        if (cat != app->home_cat_index) return (SDL_Rect){-500, -500, 1, 1};
        int scroll = (int)(row->scroll_x * ((float)(UBQ_CARD_W + UBQ_CARD_GAP) /
                                            (CARD_WIDTH + CARD_GAP)));
        return (SDL_Rect){CONTENT_X + 24 + col * (UBQ_CARD_W + UBQ_CARD_GAP) - scroll,
                          UBQ_CARD_Y, UBQ_CARD_W, UBQ_CARD_H};
    }
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
    language_close_at = 0;
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
    updateHighlight(&language_highlight,(SDL_FRect){LANGUAGE_MENU.x+8,LANGUAGE_MENU.y+8+app->language_menu_index*44,LANGUAGE_MENU.w-16,44},false);
    language_close_at = SDL_GetTicks() + (Uint32)language_highlight.duration + 120;
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
        if (contains_point(language_button_rect(app), ev->button.x, ev->button.y)) {
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

/* One disjoint SDL_RenderFillRect per scanline: no repeated alpha blending at corners. */
static int rounded_inset(int row, int w, int h, int radius) {
    if (w <= 0 || h <= 0) return 0;
    int rad = SDL_max(0, SDL_min(radius, SDL_min(w, h) / 2));
    if (rad == 0) return 0;
    int edge = SDL_min(row, h - 1 - row);
    if (edge >= rad) return 0;
    float dy = (float)rad - (float)edge - 0.5f;
    return (int)ceilf((float)rad - sqrtf(fmaxf(0.0f, (float)(rad * rad) - dy * dy)));
}

static void fill_rounded_rect(SDL_Renderer *r, int x, int y, int w, int h, int radius,
                              Uint8 cr, Uint8 cg, Uint8 cb, Uint8 ca) {
    if (w <= 0 || h <= 0 || !ca) return;
    SDL_SetRenderDrawBlendMode(r, ca == 255 ? SDL_BLENDMODE_NONE : SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, cr, cg, cb, ca);
    SDL_Rect batch[128];
    int count = 0;
    for (int row = 0; row < h; ++row) {
        int inset = rounded_inset(row, w, h, radius);
        SDL_Rect line = {x + inset, y + row, w - 2 * inset, 1};
        if (line.w > 0) batch[count++] = line;
        if (count >= 126) { SDL_RenderFillRects(r, batch, count); count = 0; }
    }
    if (count) SDL_RenderFillRects(r, batch, count);
}

static void draw_rounded_border(SDL_Renderer *r, int x, int y, int w, int h, int radius,
                                int thickness, Uint8 cr, Uint8 cg, Uint8 cb, Uint8 ca) {
    if (w <= 0 || h <= 0 || thickness <= 0 || !ca) return;
    thickness = SDL_min(thickness, SDL_min(w, h) / 2);
    SDL_SetRenderDrawBlendMode(r, ca == 255 ? SDL_BLENDMODE_NONE : SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, cr, cg, cb, ca);
    SDL_Rect batch[128];
    int count = 0;
    for (int row = 0; row < h; ++row) {
        int outer = rounded_inset(row, w, h, radius);
        int ox = x + outer, ow = w - 2 * outer;
        if (ow <= 0) continue;
        if (row < thickness || row >= h - thickness || w <= 2 * thickness) {
            batch[count++] = (SDL_Rect){ox, y + row, ow, 1};
        } else {
            int inner = rounded_inset(row - thickness, w - 2 * thickness,
                                      h - 2 * thickness, SDL_max(0, radius - thickness));
            int left_limit = x + thickness + inner;
            int right_limit = x + w - thickness - inner;
            SDL_Rect left = {ox, y + row, SDL_max(0, left_limit - ox), 1};
            SDL_Rect right = {right_limit, y + row, SDL_max(0, ox + ow - right_limit), 1};
            if (left.w > 0) batch[count++] = left;
            if (right.w > 0) batch[count++] = right;
        }
        /* Leave room for both spans on the next row. */
        if (count >= 126) { SDL_RenderFillRects(r, batch, count); count = 0; }
    }
    if (count) SDL_RenderFillRects(r, batch, count);
}

static void draw_rounded_glow(SDL_Renderer *r, int x, int y, int w, int h, int radius,
                              int spread, Uint8 cr, Uint8 cg, Uint8 cb, Uint8 max_alpha) {
    if (spread <= 0 || !max_alpha) return;
    /* Separate concentric outlines keep translucent pixels from stacking. */
    for (int band = spread; band >= 1; --band) {
        float near = 1.0f - (float)(band - 1) / (float)spread;
        Uint8 alpha = (Uint8)((float)max_alpha * near * near);
        draw_rounded_border(r, x - band, y - band, w + 2 * band, h + 2 * band,
                            radius + band, 1, cr, cg, cb, alpha);
    }
}

static void fill_primary_pill(SDL_Renderer *r, SDL_Rect rect) {
    drawGlassPill(r,(SDL_FRect){rect.x,rect.y,rect.w,rect.h},GLASS_PRIMARY);
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

static int draw_hero_metadata_pill(WolodriveApp *app, const char *label, int x, int y) {
    if (!label || !label[0]) return x;
    int tw = 0, th = 0;
    TTF_Font *font = app->font_small ? app->font_small : app->font_body;
    if (font) TTF_SizeUTF8(font, label, &tw, &th);
    int width = SDL_max(65, tw + 28);
    if (x + width > SCREEN_WIDTH - 48) return x;
    drawGlassPill(app->renderer,(SDL_FRect){x,y,width,30},GLASS_NEUTRAL);
    SDL_Rect inside = {x + 14, y + 3, width - 28, 26};
    draw_fitted_text(app, font, label, inside, (SDL_Color){218, 218, 226, 255}, inside, true);
    return x + width + 8;
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
static void render_cover_or_placeholder_alpha(WolodriveApp *app, GameItem *it,
                                              int x, int y, int w, int h, bool is_hero, Uint8 alpha) {
    SDL_Renderer *r = app->renderer;
    int width = 0, height = 0;
    SDL_Texture *texture = artwork_get(app, it, is_hero, is_hero ? 100 : 80, &width, &height);
    if (!texture && is_hero) texture = artwork_get(app, it, false, 100, &width, &height);
    if (texture && width > 0 && height > 0) {
        SDL_Rect box = {x, y, w, h};
        SDL_Color color = artwork_background(app, texture);

        SDL_Rect previous;
        bool clipped = SDL_RenderIsClipEnabled(r);
        SDL_RenderGetClipRect(r, &previous);
        SDL_Rect intersection = box;
        if (clipped) SDL_IntersectRect(&box, &previous, &intersection);
        SDL_RenderSetClipRect(r, &intersection);

        if (is_hero) {
            /* 1. Szerokie rozciągnięcie pola grafiki (dstrect = 78% szerokości okna od 22% X) */
            int dst_x = app->ui_theme == UI_THEME_UBIQUITY ? x : (int)(SCREEN_WIDTH * 0.22f);
            int dst_y = 0;
            int dst_w = app->ui_theme == UI_THEME_UBIQUITY ? w : SCREEN_WIDTH - dst_x;
            int dst_h = app->ui_theme == UI_THEME_UBIQUITY ? h : (int)(SCREEN_HEIGHT * 0.62f);
            if (dst_h < h) dst_h = h;
            SDL_Rect dstrect = {dst_x, dst_y, dst_w, dst_h};

            float tex_aspect = (float)width / (float)height;
            float dst_aspect = (float)dst_w / (float)dst_h;

            /* 2. Wypełnienie całej szerokości dstrect z wycentrowaniem i 25% top bias */
            SDL_Rect srcrect;
            if (tex_aspect > dst_aspect) {
                srcrect.h = height;
                srcrect.w = (int)lroundf((float)height * dst_aspect);
                srcrect.x = (width - srcrect.w) / 2;
                srcrect.y = 0;
            } else {
                srcrect.w = width;
                srcrect.h = (int)lroundf((float)width / dst_aspect);
                if (srcrect.h > height) srcrect.h = height;
                srcrect.x = 0;
                srcrect.y = (int)lroundf((float)(height - srcrect.h) * 0.25f);
            }

            SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
            SDL_SetTextureAlphaMod(texture, (Uint8)((245 * alpha) / 255));
            SDL_RenderCopy(r, texture, &srcrect, &dstrect);

            /* Pełna czerń od lewej krawędzi ekranu do dst_x */
            fill_rect(r, 0, 0, dst_x, dst_h, 0, 0, 0, (Uint8)alpha);

            /* 3. Płynne zanikanie lewej krawędzi przez 55% szerokości dstrect (100% czerń -> 0%) */
            int fade_w = (int)(dst_w * 0.55f);
            for (int gx = 0; gx <= fade_w; gx++) {
                float t = (float)gx / (float)fade_w;
                float dark_factor = (1.0f - t) * (1.0f - t);
                Uint8 dark_a = (Uint8)(255.0f * dark_factor * alpha / 255.0f);
                if (dark_a > 0) fill_rect(r, dst_x + gx, dst_y, 1, dst_h, 0, 0, 0, dark_a);
            }

            /* 4. Płynne zanikanie dolnej krawędzi przez 35% wysokości dstrect w pętli 64-krokowej */
            int fade_h = (int)(dst_h * 0.35f);
            int fade_y = dst_y + dst_h - fade_h;
            fill_vertical_gradient(r, 0, fade_y, SCREEN_WIDTH, fade_h,
                                   0, 0, 0, 0,
                                   0, 0, 0, (Uint8)alpha, 64);

            /* Top vignette for clean topbar blending */
            fill_vertical_gradient(r, 0, 0, SCREEN_WIDTH, 80,
                                   0, 0, 0, (Uint8)((160 * alpha) / 255),
                                   0, 0, 0, 0, 24);

            SDL_SetTextureAlphaMod(texture, 255);
            SDL_RenderSetClipRect(r, clipped ? &previous : NULL);
            return;
        }

        fill_vertical_gradient(r, x, y, w, h, color.r/3, color.g/3, color.b/3, alpha,
                               color.r/8, color.g/8, color.b/8, alpha, 24);
        bool complete_box = artwork_complete_box(app, texture);
        if (!complete_box) {
            int band = render_box_brand(app, it, box);
            box.y += band; box.h -= band;
        }

        SDL_Rect destination = artwork_fit(width, height, box, true);
        SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
        SDL_SetTextureAlphaMod(texture, alpha);
        SDL_RenderCopy(r, texture, NULL, &destination);
        SDL_SetTextureAlphaMod(texture, 255);
        SDL_RenderSetClipRect(r, clipped ? &previous : NULL);
    } else {
        /* Elegancki, stylowy Placeholder z logo Obsidian i gradientem bazowym */
        fill_vertical_gradient(r, x, y, w, h,
                               it->cover_r, it->cover_g, it->cover_b, alpha,
                               (uint8_t)(it->cover_r / 3), (uint8_t)(it->cover_g / 3), (uint8_t)(it->cover_b / 3), alpha,
                               24);

        /* Geometryczny akcent w tle placeholdera */
        fill_rect(r, x + w - 90, y, 90, h, 255, 255, 255, (Uint8)((12 * alpha) / 255));
        fill_rect(r, x, y + h / 2 - 1, w, 2, 255, 255, 255, (Uint8)((18 * alpha) / 255));

        SDL_Color watermark_col = { 255, 255, 255, (Uint8)((75 * alpha) / 255) };
        SDL_Color sub_col       = { 220, 220, 225, (Uint8)((160 * alpha) / 255) };
        {
            TTF_Font *ph_f = is_hero ? app->font_row_title : app->font_small;
            int ph_y = y + h / 2 - (is_hero ? 28 : 22);
            SDL_Color ph_red = { 229, 9, 20, (Uint8)((210 * alpha) / 255) };
            int ph_pw = draw_text(app, ph_f, "O", x + 18, ph_y, ph_red, 0);
            draw_text(app, ph_f, "bsidian", x + 18 + (ph_pw > 0 ? ph_pw + 1 : 14), ph_y, watermark_col, 0);
        }

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
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
#ifdef __PS5__
    app->renderer = obsidian_create_ps5_renderer(app->window);
#else
    Uint32 flags = SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC;
    /* WSLg can create an accelerated renderer successfully but present only black.
       An explicit SDL_RENDER_DRIVER remains available for driver testing. */
    if (!SDL_GetHint(SDL_HINT_RENDER_DRIVER) &&
        (getenv("WSL_DISTRO_NAME") || getenv("WSL_INTEROP"))) {
        flags = SDL_RENDERER_SOFTWARE;
    }
    app->renderer = SDL_CreateRenderer(app->window, -1, flags);
    if (!app->renderer && flags != SDL_RENDERER_SOFTWARE) {
        fprintf(stderr, "[OBSIDIAN WARN] Accelerated renderer: %s; trying software\n", SDL_GetError());
        app->renderer = SDL_CreateRenderer(app->window, -1, SDL_RENDERER_SOFTWARE);
    }
#endif
    if (!app->renderer) {
        fprintf(stderr, "[OBSIDIAN ERROR] SDL_CreateRenderer: %s\n", SDL_GetError());
        return false;
    }
    if (SDL_RenderSetLogicalSize(app->renderer, SCREEN_WIDTH, SCREEN_HEIGHT) != 0) {
        fprintf(stderr, "[OBSIDIAN ERROR] SDL_RenderSetLogicalSize: %s\n", SDL_GetError());
        return false;
    }
    SDL_RendererInfo info;
    app->vsync_enabled = false;
    if (SDL_GetRendererInfo(app->renderer, &info) == 0) {
        app->vsync_enabled = (info.flags & SDL_RENDERER_PRESENTVSYNC) != 0;
        printf("[OBSIDIAN DISPLAY] Video: %s; renderer: %s\n",
               SDL_GetCurrentVideoDriver(), info.name);
    }
    /* Also request VSync on the final renderer, including the WSL/software
       path. Unsupported backends retain the independent 60 FPS frame cap. */
#if SDL_VERSION_ATLEAST(2, 0, 18)
    if (SDL_RenderSetVSync(app->renderer, 1) == 0) app->vsync_enabled = true;
#endif
    printf("[OBSIDIAN DISPLAY] VSync: %s; frame limit: 60 FPS\n",
           app->vsync_enabled ? "enabled" : "unavailable (using frame pacing)");
    return true;
}

bool ui_init(WolodriveApp *app) {
    app->font_logo       = open_first_available_font(FONT_CANDIDATES_BOLD, 32);
    app->font_hero_title = open_first_available_font(FONT_CANDIDATES_BOLD, 52);
    app->font_ubiquity_title = open_first_available_font(FONT_CANDIDATES_BOLD, 62);
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
        fprintf(stderr, "[OBSIDIAN ERROR] UI fonts could not be loaded: %s\n", TTF_GetError());
        return false;
    }
    if (!TTF_GlyphIsProvided(app->font_body, 0x4e2d)) {
        fprintf(stderr, "[OBSIDIAN WARN] Install Noto Sans CJK or provide assets/font_cjk.ttc for Chinese text.\n");
    }

    app->active_row = 0;
    app->home_cat_index = 0;
    app->home_grid_expanded = false;
    app->home_grid_anim = 0.0f;
    app->sidebar_width = (float)SIDEBAR_COLLAPSED_W;
    app->sidebar_indicator_y = (float)sidebar_target(app->active_row).y;
    app->sidebar_dim_alpha = 0.0f;
    app->hero_crossfade = 1.0f;
    app->card_focus_scale = 1.0f;
    app->vertical_scroll_y = 0.0f;
    app->target_vertical_scroll_y = 0.0f;
    app->focus_pulse = 0.0f;

    ui_rebuild_carousels(app);
    return true;
}

#include "file_manager_ui.h"

void ui_cleanup(WolodriveApp *app) {
    files_shutdown();
    glass_cleanup(app->renderer);
    sidebar_from=sidebar_to=0;language_close_at=0;
    memset(&nav_highlight,0,sizeof(nav_highlight));
    memset(&category_highlight,0,sizeof(category_highlight));
    memset(&language_highlight,0,sizeof(language_highlight));
    text_cache_destroy(app);
    screenshots_destroy(app->screenshots); app->screenshots = NULL;
    artwork_destroy(app);
    for (int i = 0; i < app->library_artwork_count; i++) {
        if (app->library_artwork[i].texture) SDL_DestroyTexture(app->library_artwork[i].texture);
    }
    app->library_artwork_count = 0;
    if (app->font_logo)       TTF_CloseFont(app->font_logo);
    if (app->font_hero_title) TTF_CloseFont(app->font_hero_title);
    if (app->font_ubiquity_title) TTF_CloseFont(app->font_ubiquity_title);
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

/* Keep Home category membership and ranking consistent with the web preview.
   Compare game identity across sources before retaining the top 128 cards. */
static void search_title_key(const char *title, char key[128]);
static int search_platform(const GameItem *item);

static bool home_same_identity(const GameItem *a, const GameItem *b) {
    if (search_platform(a) != search_platform(b) ||
        a->is_dlc != b->is_dlc || a->is_game != b->is_game) return false;
    char first[128],second[128];
    search_title_key(a->title,first);
    search_title_key(b->title,second);
    return first[0] && strcmp(first,second)==0;
}

static void home_insert_item(CarouselRow *row, GameItem *item,
                             int (*compare)(const void *,const void *)) {
    for(int i=0;i<row->item_count;i++) {
        if(!home_same_identity(row->items[i],item)) continue;
        if(row->items[i]->catalog_added_at>=item->catalog_added_at) return;
        if(!compare) {row->items[i]=item;return;}
        memmove(&row->items[i],&row->items[i+1],
                (size_t)(row->item_count-i-1)*sizeof(GameItem *));
        row->item_count--;
        break;
    }
    if(!compare) {
        if(row->item_count<MAX_ITEMS_PER_ROW) row->items[row->item_count++]=item;
        return;
    }
    int pos=0;
    while(pos<row->item_count && compare(&row->items[pos],&item)<=0) pos++;
    if(pos>=MAX_ITEMS_PER_ROW) return;
    int count=row->item_count;
    if(count<MAX_ITEMS_PER_ROW) row->item_count++;
    for(int i=row->item_count-1;i>pos;i--) row->items[i]=row->items[i-1];
    row->items[pos]=item;
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

    bool console_games = app->active_row == VIEW_GAMES && app->library_depth == LIBRARY_GAMES;
    int console_seen = 0;
    for (int i = 0; i < app->catalog_count; i++) {
        GameItem *it = &app->catalog[i];
        if (!settings_accepts_item(app, it)) continue;

        if (!console_games || library_matches_console(app, it)) {
            int index = console_games ? console_seen++ : 0;
            if ((!console_games || index >= app->library_game_offset) &&
                app->rows[VIEW_GAMES].item_count < MAX_ITEMS_PER_ROW)
                app->rows[VIEW_GAMES].items[app->rows[VIEW_GAMES].item_count++] = it;
        }
        if ((it->state == STATE_DOWNLOADING || it->state == STATE_INSTALLED) &&
            app->rows[VIEW_DOWNLOADS].item_count < MAX_ITEMS_PER_ROW) {
            app->rows[VIEW_DOWNLOADS].items[app->rows[VIEW_DOWNLOADS].item_count++] = it;
        }

        if (it->is_game) {
            int platform=search_platform(it);
            if(platform==4 || platform==5)
                home_insert_item(&app->home_categories[platform==4 ? HOME_CAT_PS4 : HOME_CAT_PS5],
                                 it,NULL);
            home_insert_item(&app->home_categories[HOME_CAT_POPULAR],it,cmp_popular_desc);
            if(it->catalog_added_at || it->release_date[0])
                home_insert_item(&app->home_categories[HOME_CAT_NEWEST],it,cmp_newest_desc);
            home_insert_item(&app->home_categories[HOME_CAT_TOP_RATED],it,cmp_rating_desc);
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
        app->library_game_offset = 0;
        CarouselRow *row = &app->rows[app->active_row];
        row->selected_col = 0;
        row->scroll_x = row->target_scroll_x = 0;
        row->scroll_velocity = 0;
        ui_rebuild_carousels(app);
    }
}

static bool handle_library_input(WolodriveApp *app, const SDL_Event *ev) {
    if (!is_library_view(app->active_row)) return false;
    int count = app->library_depth == LIBRARY_COMPANIES ? library_company_count() :
                 library_console_visible_count(app->library_company, app->active_row == VIEW_EMULATORS);
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
    /* Native Emulators have their own release detail screen, not empty games. */
    if (app->active_row == VIEW_EMULATORS && app->library_depth == LIBRARY_GAMES) {
        const LibraryConsole *console =
            library_console_visible_at(app->library_company, app->library_console_selection, app->active_row == VIEW_EMULATORS);
        const EmulatorReleaseAsset *match = emulator_release_for_console(console);
        bool pressed = action == 5 ||
            (ev->type == SDL_MOUSEBUTTONDOWN && ev->button.button == SDL_BUTTON_LEFT &&
             ev->button.x >= CONTENT_X + 40 && ev->button.x <= CONTENT_X + 700 &&
             ev->button.y >= 590 && ev->button.y <= 690);
        if (pressed) {
            if (match) {
                if (!emulator_start_download(app, match))
                    sys_send_notification(app, "Emulator download already active or unavailable");
                else sys_send_notification(app, "Downloading verified emulator asset", match->filename);
            } else {
                /* Most systems are not tied to a specific asset. Show the
                   repository listing; do not guess a RetroArch core. */
                if (SDL_OpenURL(EMULATOR_RELEASES_URL) != 0)
                    sys_send_notification(app, "Releases URL displayed on screen");
            }
        }
        return true;
    }
    if (app->sidebar_focus) {
        if (action == 2 || action == 5) { app->sidebar_focus = false; return true; }
        if (action == 1) return true;
        return false; /* Up/down still navigate the sidebar. */
    }
    if (app->library_depth == LIBRARY_GAMES) {
        CarouselRow *row = &app->rows[app->active_row];
        if (action == 1 && row->selected_col == 0) {
            if (app->library_game_offset > 0) {
                app->library_game_offset -= MAX_ITEMS_PER_ROW;
                ui_rebuild_carousels(app);
                row->selected_col = SDL_max(0, row->item_count - 1);
                return true;
            }
            app->sidebar_focus = true;
            return true;
        }
        if (action == 2 && row->item_count == MAX_ITEMS_PER_ROW &&
            row->selected_col == row->item_count - 1) {
            const LibraryConsole *console = library_console_visible_at(app->library_company, app->library_console_selection, app->active_row == VIEW_EMULATORS);
            if (library_console_game_count(app, console) > app->library_game_offset + row->item_count) {
                app->library_game_offset += MAX_ITEMS_PER_ROW;
                row->selected_col = 0;
                row->scroll_x = row->target_scroll_x = row->scroll_velocity = 0;
                ui_rebuild_carousels(app);
                return true;
            }
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
        app->sidebar_hovered = (mx >= 0 && (float)mx <= sb_limit); if (!app->sidebar_hovered) app->sidebar_focus = false;
    }

    if (app->active_row == VIEW_FILES && files.modal) { handle_files_input(app, ev); return; }
    if (handle_language_input(app, ev)) return;
    if (app->detail_open) { handle_game_details_input(app, ev); return; }
    if (ev->type == SDL_MOUSEBUTTONDOWN && ev->button.button == SDL_BUTTON_LEFT) {
        int sidebar_hit_w = (app->active_row == VIEW_SETTINGS || app->sidebar_hovered || app->sidebar_focus || app->sidebar_expand > 0.35f)
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
            if (position >= -1 && position <= VIEW_DOWNLOADS + 1) {
                select_sidebar_view(app, position == -1 ? VIEW_SEARCH :
                                    position == VIEW_DOWNLOADS + 1 ? VIEW_SETTINGS : position);
                app->sidebar_focus = true;
            }
            return;
        }
        if (enter) { app->sidebar_focus = false; return; }
        if (left) return;
    }
    if (handle_files_input(app, ev)) return;
    if (handle_settings_input(app, ev)) return;
    if (handle_search_input(app, ev)) return;
    if (handle_library_input(app, ev)) return;

    /* Przyciski akcji w sekcji Hero ([Play/Pobierz], [Więcej informacji], [Przełącznik opisu]) */
    if (ev->type == SDL_MOUSEBUTTONDOWN && ev->button.button == SDL_BUTTON_LEFT &&
        app->active_row != VIEW_SEARCH && app->active_row != VIEW_SETTINGS &&
        !app->home_grid_expanded) {
        GameItem *hero_item = get_focused_item(app);
        if (hero_item) {
            SDL_Rect play_b, info_b, toggle_b;
            get_hero_button_rects(app, hero_item, &play_b, &info_b, &toggle_b);
            if (contains_point(play_b, ev->button.x, ev->button.y)) {
                app->sidebar_focus = false;
                sys_trigger_action(app, hero_item);
                return;
            }
            if (contains_point(info_b, ev->button.x, ev->button.y)) {
                app->sidebar_focus = false;
                ui_open_game(app, hero_item);
                return;
            }
            if (toggle_b.w > 0 && contains_point(toggle_b, ev->button.x, ev->button.y)) {
                app->show_original_description = !app->show_original_description;
                app->description_scroll = 0;
                return;
            }
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

        if (app->ui_theme == UI_THEME_UBIQUITY && !app->home_grid_expanded &&
            my >= category_y(app->renderer) && my <= category_y(app->renderer) + 40) {
            int pos = CONTENT_X + 24;
            for (int cat = 0; cat < HOME_CAT_COUNT; cat++) {
                const char *name = home_category_title(app, cat);
                int width = 0, height = 0;
                if (app->font_micro) TTF_SizeUTF8(app->font_micro, name, &width, &height);
                int slot = SDL_max(80, width + 32);
                if (mx >= pos && mx < pos + slot) {
                    app->sidebar_focus = false;
                    app->home_cat_index = cat;
                    sync_home_active_row(app);
                    track_description_focus(app, get_focused_item(app));
                    return;
                }
                pos += slot + 12;
            }
        }
        for (int cat = 0; cat < HOME_CAT_COUNT && app->ui_theme != UI_THEME_UBIQUITY; cat++) {
            int row_y = CARDS_Y + cat * HOME_ROW_STRIDE - (int)app->home_vertical_scroll;
            int header_y = row_y - 44;
            if (header_y < HERO_HEIGHT - 40 || header_y > SCREEN_HEIGHT) continue;
            SDL_Rect title_hit = {CONTENT_X, header_y, 350, 38};
            SDL_Rect show_all_hit = get_show_all_button_rect(app, header_y);
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
        SDL_Rect hit = app->ui_theme == UI_THEME_UBIQUITY ?
            (SDL_Rect){CONTENT_X + 10, 366, 795, 122} : DESCRIPTION_AREA;
        app->description_hovered = contains_point(hit, ev->motion.x, ev->motion.y);
    }
    if ((ev->type == SDL_KEYDOWN && ev->key.keysym.sym == SDLK_i && !ev->key.repeat) ||
        (ev->type == SDL_CONTROLLERBUTTONDOWN && ev->cbutton.button == SDL_CONTROLLER_BUTTON_RIGHTSTICK)) {
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
    if(language_close_at && SDL_GetTicks()>=language_close_at) {
        app->language_menu_open=false; language_close_at=0;
    }

    files_poll(app);
    /* The sync worker marks every entry dirty. Coalesce expensive ranking
       rebuilds while it runs, but publish the final update immediately. */
    if (app->catalog_dirty) {
        Uint32 now = SDL_GetTicks();
        if (!app->sync_in_progress ||
            (Uint32)(now - app->last_catalog_rebuild_tick) >= 400) {
            ui_rebuild_carousels(app);
            app->last_catalog_rebuild_tick = now;
        }
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
        float target_sb = (app->active_row == VIEW_SETTINGS || app->sidebar_hovered || app->sidebar_focus || app->language_menu_open) ? 1.0f : 0.0f;
        if(target_sb!=sidebar_to) { sidebar_from=app->sidebar_expand;sidebar_to=target_sb;sidebar_started=SDL_GetTicks(); }
        float sb_t=SDL_min(1.0f,(SDL_GetTicks()-sidebar_started)/240.0f);
        app->sidebar_expand=sidebar_from+(sidebar_to-sidebar_from)*glass_sidebar_ease(sb_t);
        if (app->sidebar_expand < 0.001f) app->sidebar_expand = 0.0f;
        if (app->sidebar_expand > 0.999f) app->sidebar_expand = 1.0f;
        app->sidebar_width = (float)SIDEBAR_COLLAPSED_W +
                             ((float)(SIDEBAR_EXPANDED_W - SIDEBAR_COLLAPSED_W) * app->sidebar_expand);

        /* Slide the red rail marker continuously when controller navigation changes view. */
        float marker_target = (float)sidebar_target(app->active_row).y;
        app->sidebar_indicator_y += (marker_target - app->sidebar_indicator_y) * (1.0f - expf(-20.0f * dt));
        if (fabsf(marker_target - app->sidebar_indicator_y) < 0.5f) app->sidebar_indicator_y = marker_target;

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



static const char *action_label(WolodriveApp *app, GameItem *item) {
    if (item->state == STATE_DOWNLOADING) return TR(app, TXT_DOWNLOADING);
    if (item->state == STATE_INSTALLED)
        return TR(app, item->format == FMT_PKG ? TXT_INSTALLED : TXT_MOUNT);
    if (item->format == FMT_CATALOG) return "WEBSITE";
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
        const LibraryConsole *console = library_console_visible_at(app->library_company, app->library_console_selection, app->active_row == VIEW_EMULATORS);
        if (console && !console->title_id_prefix) return TR(app, TXT_CONSOLE_NOT_CONNECTED);
    }
    if (app->active_row == VIEW_EMULATORS || app->active_row == VIEW_FILES ||
        app->active_row == VIEW_PAYLOADS) return TR(app, TXT_SECTION_UNAVAILABLE);
    return TR(app, app->active_row == VIEW_DOWNLOADS ? TXT_NO_DOWNLOADS : TXT_NO_PACKAGES);
}

static const char *view_content_title(WolodriveApp *app) {
    if (app->active_row == VIEW_HOME) return home_category_title(app, app->home_cat_index);
    if (is_library_view(app->active_row) && app->library_depth == LIBRARY_GAMES) {
        const LibraryConsole *console = library_console_visible_at(app->library_company, app->library_console_selection, app->active_row == VIEW_EMULATORS);
        if (console) return console->name;
    }
    return app->rows[app->active_row].title;
}

/* Game-artwork-wide cinematic fades: scanlines and nonoverlapping spans,
   no Playnite/XAML runtime, assets or external service required. */
static void render_ubiquity_fades(SDL_Renderer *r) {
    for (int band = 0; band < 64; band++) {
        float t = (float)band / 64.0f;
        Uint8 opacity = (Uint8)(221.0f * (1.0f-t) * (1.0f-t));
        fill_rect(r, RAIL_WIDTH + band*16, 0, 16, 960, 4, 5, 9, opacity);
    }
    for (int band = 0; band < 54; band++) {
        float t = (float)(band+1) / 54.0f;
        Uint8 opacity = (Uint8)(242.0f * t*t);
        fill_rect(r, RAIL_WIDTH, 528 + band*10, SCREEN_WIDTH - RAIL_WIDTH,
                  10, 5, 6, 9, opacity);
    }
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
                                          RAIL_WIDTH, 0, SCREEN_WIDTH - RAIL_WIDTH, (app->ui_theme == UI_THEME_UBIQUITY ? 960 : 720), true, prev_alpha);
        render_cover_or_placeholder_alpha(app, hero,
                                          RAIL_WIDTH, 0, SCREEN_WIDTH - RAIL_WIDTH, (app->ui_theme == UI_THEME_UBIQUITY ? 960 : 720), true, curr_alpha);
    } else {
        render_cover_or_placeholder(app, hero, RAIL_WIDTH, 0, SCREEN_WIDTH - RAIL_WIDTH, (app->ui_theme == UI_THEME_UBIQUITY ? 960 : 720), true);
    }

    /* Czarny gradient dolny krawędzi sekcji Hero */
    if (app->ui_theme == UI_THEME_UBIQUITY) render_ubiquity_fades(r);
    else fill_vertical_gradient(r, RAIL_WIDTH, 420, SCREEN_WIDTH - RAIL_WIDTH, 132,
                                0, 0, 0, 0, 0, 0, 0, 255, 18);

    {
        int logo_y = app->ui_theme == UI_THEME_UBIQUITY ? 46 : 100;
        int logo_x = app->ui_theme == UI_THEME_UBIQUITY ? CONTENT_X + 10 : CONTENT_X;
        int p_w = draw_text(app, app->font_logo, "O", logo_x, logo_y, (SDL_Color){229, 9, 20, 255}, 0);
        draw_text(app, app->font_logo, "bsidian", logo_x + (p_w > 0 ? p_w + 1 : 22), logo_y, white, 0);
    }
    /* Two title lines fit without displacing metadata or description. */
    SDL_Rect title_clip = app->ui_theme == UI_THEME_UBIQUITY ?
        (SDL_Rect){CONTENT_X + 10, 150, 1100, 138} : (SDL_Rect){CONTENT_X, 142, 940, 136};
    const char *title_text = hero->title;
    char uppercase_title[sizeof(hero->title)];
    if (app->ui_theme == UI_THEME_UBIQUITY) {
        SDL_strlcpy(uppercase_title, hero->title, sizeof(uppercase_title));
        /* Uppercase ASCII without damaging UTF-8 characters in localized titles. */
        for (char *ch = uppercase_title; *ch; ch++)
            if (*ch >= 'a' && *ch <= 'z') *ch = (char)(*ch - 'a' + 'A');
        title_text = uppercase_title;
    }
    SDL_RenderSetClipRect(r, &title_clip);
    draw_text(app, app->ui_theme == UI_THEME_UBIQUITY && app->font_ubiquity_title ?
              app->font_ubiquity_title : app->font_hero_title, title_text,
              title_clip.x, title_clip.y, white, title_clip.w);
    SDL_RenderSetClipRect(r, NULL);
    /* Each field gets its own smoked-glass metadata capsule. */
    int pill_x = CONTENT_X + (app->ui_theme == UI_THEME_UBIQUITY ? 10 : 0);
    int pill_y = app->ui_theme == UI_THEME_UBIQUITY ? 312 : 276;
    char meta[96];
    pill_x = draw_hero_metadata_pill(app, hero->id_code, pill_x, pill_y);
    snprintf(meta, sizeof(meta), "v%s", hero->version);
    pill_x = draw_hero_metadata_pill(app, meta, pill_x, pill_y);
    pill_x = draw_hero_metadata_pill(app, sys_format_short_badge(hero->format), pill_x, pill_y);
    snprintf(meta, sizeof(meta), "%.1f GB", hero->size_gb);
    pill_x = draw_hero_metadata_pill(app, meta, pill_x, pill_y);
    if (hero->rating_score > 0.0f) {
        snprintf(meta, sizeof(meta), "★ %.1f/10", hero->rating_score);
        pill_x = draw_hero_metadata_pill(app, meta, pill_x, pill_y);
    }
    if (hero->download_count) {
        snprintf(meta, sizeof(meta), "↓ %u", hero->download_count);
        pill_x = draw_hero_metadata_pill(app, meta, pill_x, pill_y);
    }
    if (hero->release_date[0]) {
        pill_x = draw_hero_metadata_pill(app, hero->release_date, pill_x, pill_y);
    } else if (hero->catalog_added_at >= 10000000000000ULL) {
        /* Match React's date fallback (YYYYMMDD from the catalogue timestamp). */
        unsigned long long day = (unsigned long long)(hero->catalog_added_at / 1000000ULL);
        snprintf(meta, sizeof(meta), "%04llu-%02llu-%02llu",
                 day / 10000ULL, (day / 100ULL) % 100ULL, day % 100ULL);
        pill_x = draw_hero_metadata_pill(app, meta, pill_x, pill_y);
    }
    SDL_Rect desc_area = app->ui_theme == UI_THEME_UBIQUITY ?
        (SDL_Rect){CONTENT_X + 10, 366, 795, 122} : DESCRIPTION_AREA;
    char description[MAX_TRANSLATED_DESCRIPTION_BYTES];
    description_text(app, hero, description, sizeof(description));
    UiTextEntry *entry = cached_text(app, app->font_body, description, muted, desc_area.w);
    if (entry) {
        int maximum_scroll = SDL_max(0, entry->h - desc_area.h);
        app->description_scroll = SDL_min(app->description_scroll, maximum_scroll);
        {
            SDL_Rect destination = {desc_area.x, desc_area.y - app->description_scroll,
                                    entry->w, entry->h};
            SDL_RenderSetClipRect(r, &desc_area);
            SDL_RenderCopy(r, entry->texture, NULL, &destination);
            SDL_RenderSetClipRect(r, NULL);
        }
        if (maximum_scroll > 0) {
            fill_rect(r, desc_area.x + desc_area.w + 4, desc_area.y, 3, desc_area.h, 65, 65, 75, 255);
            int thumb_y = desc_area.y + (desc_area.h - 12) * app->description_scroll / maximum_scroll;
            fill_rect(r, desc_area.x + desc_area.w + 4, thumb_y, 3, 12, 180, 180, 190, 255);
        }
    }

    /* Dynamiczne przyciski akcji w stylu Netflix Smart TV wyrównane do marginesu CONTENT_X */
    SDL_Rect play_rect, info_rect, toggle_rect;
    get_hero_button_rects(app, hero, &play_rect, &info_rect, &toggle_rect);

    /* Primary: 50px red-gradient capsule with a soft crimson halo. */
    fill_primary_pill(r, play_rect);
    char play_label[128];
    snprintf(play_label, sizeof(play_label), "▶  %s", action_label(app, hero));
    draw_fitted_text(app, app->font_small, play_label,
                     (SDL_Rect){play_rect.x + 14, play_rect.y + 8, play_rect.w - 28, play_rect.h - 16},
                     white, play_rect, true);

    /* Przycisk 2: Więcej informacji */
    drawGlassPill(r,(SDL_FRect){info_rect.x,info_rect.y,info_rect.w,info_rect.h},GLASS_NEUTRAL);
    char info_label[128];
    snprintf(info_label, sizeof(info_label), "ℹ  %s", TR(app, TXT_MORE_INFO));
    draw_fitted_text(app, app->font_small, info_label,
                     (SDL_Rect){info_rect.x + 14, info_rect.y + 8, info_rect.w - 28, info_rect.h - 16},
                     white, info_rect, true);

    /* Przycisk 3: Przełącznik opisu */
    if (toggle_rect.w > 0) {
        drawGlassPill(r,(SDL_FRect){toggle_rect.x,toggle_rect.y,toggle_rect.w,toggle_rect.h},GLASS_NEUTRAL);
        TextId label = app->show_original_description ? TXT_TRANSLATED_DESCRIPTION : TXT_ORIGINAL_DESCRIPTION;
        char toggle_text[160];
        snprintf(toggle_text, sizeof(toggle_text), "%s [I]", TR(app, label));
        draw_fitted_text(app, app->font_small, toggle_text,
                         (SDL_Rect){toggle_rect.x + 14, toggle_rect.y + 8,
                                    toggle_rect.w - 28, toggle_rect.h - 16},
                         white, toggle_rect, true);
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
        card.y = base_card.y - (new_h - CARD_HEIGHT) / 2 - 6;
        card.w = new_w;
        card.h = new_h;
    } else {
        card.h = CARD_HEIGHT;
    }

    render_cover_or_placeholder(app, item, card.x, card.y, card.w, card.h, false);
    /* The outer 18px mask is applied after the title/footer are drawn. */

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
    /* Mask the whole cover/title/footer silhouette, not just the cover texture. */
    {
        int full_h = card.h + CARD_TITLE + CARD_FOOTER;
        const int radius = app->ui_theme == UI_THEME_UBIQUITY ? 5 : 18;
        for (int row = 0; row < radius; ++row) {
            int inset = rounded_inset(row, card.w, full_h, radius);
            if (inset > 0) {
                fill_rect(r, card.x, card.y + row, inset, 1, 0, 0, 0, 255);
                fill_rect(r, card.x + card.w - inset, card.y + row, inset, 1, 0, 0, 0, 255);
                fill_rect(r, card.x, card.y + full_h - 1 - row, inset, 1, 0, 0, 0, 255);
                fill_rect(r, card.x + card.w - inset, card.y + full_h - 1 - row, inset, 1, 0, 0, 0, 255);
            }
        }
        if (selected && !app->sidebar_focus) {
            draw_rounded_glow(r, card.x - 3, card.y - 3, card.w + 6, full_h + 6,
                              radius + 3, 7,
                              app->ui_theme == UI_THEME_UBIQUITY ? 245 : 229,
                              app->ui_theme == UI_THEME_UBIQUITY ? 245 : 9,
                              app->ui_theme == UI_THEME_UBIQUITY ? 250 : 20, 90);
            draw_rounded_border(r, card.x - 3, card.y - 3, card.w + 6, full_h + 6,
                                radius + 3, 3, 245, 245, 250, 255);
            draw_rounded_border(r, card.x, card.y, card.w, full_h,
                                radius, 1, 229, 9, 20, 215);
        }
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
            SDL_Rect back_btn = {SCREEN_WIDTH - 288, TOPBAR_HEIGHT + 12, 240, 36};
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
            /* Fade the full catalogue grid into place without altering layout. */
            if (app->home_grid_anim < 1.0f) {
                Uint8 fade = (Uint8)(170.0f * (1.0f - app->home_grid_anim));
                fill_rect(r, RAIL_WIDTH, 0, SCREEN_WIDTH - RAIL_WIDTH, SCREEN_HEIGHT, 0, 0, 0, fade);
            }
            return;
        }

        SDL_Rect home_clip = {RAIL_WIDTH + 12, CARDS_Y - 50,
                              SCREEN_WIDTH - RAIL_WIDTH - 24, 950 - (CARDS_Y - 50)};
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
            SDL_Rect show_all = get_show_all_button_rect(app, header_y);
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
    SDL_Rect clip = {RAIL_WIDTH + 20, CARDS_Y - 14, SCREEN_WIDTH - RAIL_WIDTH - 32,
                     950 - (CARDS_Y - 14)};
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

    if (app->sidebar_expand > 0.02f) {
        Uint8 dim_alpha = (Uint8)(115.0f * app->sidebar_expand);
        fill_rect(r, sb_w, 0, SCREEN_WIDTH - sb_w, SCREEN_HEIGHT, 0, 0, 0, dim_alpha);
    }

    drawGlassPill(r, app->ui_theme == UI_THEME_UBIQUITY
                  ? (SDL_FRect){0,0,sb_w,ui_height(r)}
                  : (SDL_FRect){0,16,sb_w,ui_height(r)-32},
                  app->ui_theme==UI_THEME_UBIQUITY?GLASS_PANEL_UBQ:GLASS_PANEL);
    SDL_Rect prev_clip; bool was_clipped=SDL_RenderIsClipEnabled(r);
    SDL_RenderGetClipRect(r,&prev_clip);
    int brand_y = 82;
    TTF_Font *p_font = app->font_logo ? app->font_logo : app->font_row_title;
    if (app->ui_theme != UI_THEME_UBIQUITY) {
    draw_text(app, p_font, "O", 33, brand_y + 2, (SDL_Color){90, 0, 5, 220}, 0);
    draw_text(app, p_font, "O", 31, brand_y, (SDL_Color){229, 9, 20, 255}, 0);
    fill_rect(r, 29, brand_y + 38, 24, 3, 229, 9, 20, 255);
    fill_rect(r, 33, brand_y + 42, 16, 2, 160, 10, 18, 200);

    if (sb_w > SIDEBAR_COLLAPSED_W + 5) {
        Uint8 brand_alpha = (Uint8)SDL_min(255, (int)(app->sidebar_expand * 255.0f));
        if (brand_alpha > 0) {
            TTF_Font *sub_font = app->font_row_title ? app->font_row_title : app->font_body;
            draw_text(app, sub_font, "bsidian", 58, brand_y + 8, (SDL_Color){245, 245, 245, brand_alpha}, 0);
        }
    }

    }
    if (app->ui_theme == UI_THEME_UBIQUITY) {
        /* Edge-aligned miniature O emblem, unfolding with the translucent rail. */
        bool expanded = sb_w > SIDEBAR_COLLAPSED_W + 8;
        int bx = expanded ? 10 : 13, by = expanded ? 77 : 80;
        int bw = expanded ? 198 : 38, bh = expanded ? 54 : 38;
        fill_rounded_rect(r, bx, by, bw, bh, 12, 21, 18, 37, 168);
        draw_rounded_border(r, bx, by, bw, bh, 12, 1, 255, 114, 152, 138);
        if (expanded) {
            draw_text(app, app->font_small, "Obsidian", bx + 18, by + 15,
                      (SDL_Color){249, 241, 247, 255}, 0);
        } else {
            draw_text(app, app->font_micro ? app->font_micro : app->font_small,
                      "O", bx + 14, by + 11, (SDL_Color){255, 169, 192, 255}, 0);
        }
    }
    SDL_Rect nav_target=sidebar_target(app->active_row);
    drawHighlight(r,&nav_highlight,(SDL_FRect){
                      sb_w <= SIDEBAR_COLLAPSED_W + 8 ? (float)(sb_w-44)/2 : 8,
                      (float)nav_target.y,
                      sb_w <= SIDEBAR_COLLAPSED_W + 8 ? 44 : (float)sb_w-16,44},
                  fabsf(app->sidebar_width-(float)sb_w)>.01f);
    SDL_Rect sb_clip={8,24,sb_w-16,ui_height(r)-48};
    SDL_RenderSetClipRect(r,&sb_clip);
    for (int row = 0; row < TOTAL_ROWS; row++) {
        SDL_Rect target = sidebar_target(row);
        bool selected = row == app->active_row;

        Uint8 ir = app->ui_theme == UI_THEME_UBIQUITY ? (selected ? 250 : 154) : (selected ? 255 : 215);
        Uint8 ig = app->ui_theme == UI_THEME_UBIQUITY ? (selected ? 250 : 154) : (selected ? 35 : 32);
        Uint8 ib = app->ui_theme == UI_THEME_UBIQUITY ? (selected ? 253 : 162) : (selected ? 45 : 40);
        Uint8 ia = selected ? 255 : 235;

        int x = app->ui_theme == UI_THEME_UBIQUITY ? 18 : 28;
        int y = target.y + (target.h - 30) / 2;
        if (selected) {
            /* The rounded highlight slides independently of the focused label. */
        }

        if (row == VIEW_SEARCH) {
            fill_rect(r, x + 5, y, 12, 3, ir, ig, ib, ia);
            fill_rect(r, x + 5, y + 19, 12, 3, ir, ig, ib, ia);
            fill_rect(r, x, y + 5, 3, 12, ir, ig, ib, ia);
            fill_rect(r, x + 19, y + 5, 3, 12, ir, ig, ib, ia);
            fill_rect(r, x + 2, y + 2, 4, 4, ir, ig, ib, ia);
            fill_rect(r, x + 16, y + 2, 4, 4, ir, ig, ib, ia);
            fill_rect(r, x + 2, y + 16, 4, 4, ir, ig, ib, ia);
            fill_rect(r, x + 16, y + 16, 4, 4, ir, ig, ib, ia);
            for (int h = 0; h < 9; h++) fill_rect(r, x + 18 + h, y + 18 + h, 4, 4, ir, ig, ib, ia);
        } else if (row == VIEW_HOME) {
            fill_rect(r, x + 20, y + 1, 4, 8, ir, ig, ib, ia);
            for (int h = 0; h < 12; h++) {
                fill_rect(r, x + 13 - h, y + h, 4, 3, ir, ig, ib, ia);
                fill_rect(r, x + 13 + h, y + h, 4, 3, ir, ig, ib, ia);
            }
            fill_rect(r, x - 1, y + 11, 30, 3, ir, ig, ib, ia);
            draw_border(r, x + 2, y + 12, 24, 16, 3, ir, ig, ib, ia);
            fill_rect(r, x + 11, y + 18, 6, 10, ir, ig, ib, ia);
        } else if (row == VIEW_GAMES) {
            draw_border(r, x - 2, y + 3, 32, 18, 3, ir, ig, ib, ia);
            fill_rect(r, x - 2, y + 20, 8, 6, ir, ig, ib, ia);
            fill_rect(r, x + 22, y + 20, 8, 6, ir, ig, ib, ia);
            fill_rect(r, x + 3, y + 10, 9, 3, ir, ig, ib, ia);
            fill_rect(r, x + 6, y + 7, 3, 9, ir, ig, ib, ia);
            fill_rect(r, x + 21, y + 7, 3, 3, ir, ig, ib, ia);
            fill_rect(r, x + 21, y + 13, 3, 3, ir, ig, ib, ia);
            fill_rect(r, x + 18, y + 10, 3, 3, ir, ig, ib, ia);
            fill_rect(r, x + 24, y + 10, 3, 3, ir, ig, ib, ia);
        } else if (row == VIEW_EMULATORS) {
            for (int h = 0; h < 6; h++) {
                fill_rect(r, x + 9 - h, y + 5 - h, 3, 2, ir, ig, ib, ia);
                fill_rect(r, x + 17 + h, y + 5 - h, 3, 2, ir, ig, ib, ia);
            }
            draw_border(r, x, y + 6, 28, 18, 3, ir, ig, ib, ia);
            draw_border(r, x + 4, y + 10, 15, 10, 2, ir, ig, ib, ia);
            fill_rect(r, x + 21, y + 11, 3, 3, ir, ig, ib, ia);
            fill_rect(r, x + 21, y + 16, 3, 3, ir, ig, ib, ia);
            fill_rect(r, x + 6, y + 25, 16, 3, ir, ig, ib, ia);
        } else if (row == VIEW_FILES) {
            fill_rect(r, x, y + 1, 12, 5, ir, ig, ib, ia);
            draw_border(r, x, y + 5, 28, 21, 3, ir, ig, ib, ia);
            fill_rect(r, x + 2, y + 11, 24, 3, ir, ig, ib, ia);
        } else if (row == VIEW_PAYLOADS) {
            draw_border(r, x + 4, y + 4, 20, 20, 3, ir, ig, ib, ia);
            fill_rect(r, x + 10, y + 10, 8, 8, ir, ig, ib, ia);
            for (int pin = 0; pin < 3; pin++) {
                int off = 7 + pin * 6;
                fill_rect(r, x + off, y, 3, 5, ir, ig, ib, ia);
                fill_rect(r, x + off, y + 23, 3, 5, ir, ig, ib, ia);
                fill_rect(r, x, y + off, 5, 3, ir, ig, ib, ia);
                fill_rect(r, x + 23, y + off, 5, 3, ir, ig, ib, ia);
            }
        } else if (row == VIEW_DOWNLOADS) {
            fill_rect(r, x + 11, y, 6, 14, ir, ig, ib, ia);
            for (int h = 0; h < 8; h++)
                fill_rect(r, x + 14 - (7 - h) * 2, y + 12 + h, (7 - h) * 4 + 1, 2, ir, ig, ib, ia);
            fill_rect(r, x, y + 23, 28, 4, ir, ig, ib, ia);
            fill_rect(r, x, y + 19, 3, 7, ir, ig, ib, ia);
            fill_rect(r, x + 25, y + 19, 3, 7, ir, ig, ib, ia);
        } else if (row == VIEW_SETTINGS) {
            fill_rect(r, x, y + 3, 28, 3, ir, ig, ib, ia);
            fill_rect(r, x, y + 13, 28, 3, ir, ig, ib, ia);
            fill_rect(r, x, y + 23, 28, 3, ir, ig, ib, ia);
            fill_rect(r, x + 5, y, 6, 9, ir, ig, ib, ia);
            fill_rect(r, x + 17, y + 10, 6, 9, ir, ig, ib, ia);
            fill_rect(r, x + 9, y + 20, 6, 9, ir, ig, ib, ia);
        }

        if (sb_w > SIDEBAR_COLLAPSED_W + 5) {
            Uint8 text_alpha = (Uint8)SDL_min(255, (int)(app->sidebar_expand * 255.0f));
            if (text_alpha > 0) {
                const char *label_text = sidebar_item_title(app, row);
                if (label_text && label_text[0] != '\0') {
                    TTF_Font *font = app->font_small ? app->font_small : app->font_body;
                    SDL_Color text_color = selected ? (SDL_Color){255, 255, 255, 255} : (SDL_Color){225, 140, 145, 255};
                    UiTextEntry *entry = cached_text(app, font, label_text, text_color, 0);
                    if (entry && entry->texture) {
                        int text_x = 60;
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
    Uint8 ir = app->language_menu_open ? 255 : 215;
    Uint8 ig = app->language_menu_open ? 35  : 32;
    Uint8 ib = app->language_menu_open ? 45  : 40;
    Uint8 ia = 255;

    if (app->ui_theme == UI_THEME_UBIQUITY) {
        SDL_Rect box = language_button_rect(app);
        int radius = sb_w <= SIDEBAR_COLLAPSED_W + 8 ? 11 : 16;
        fill_rounded_rect(r, box.x, box.y, box.w, box.h, radius,
                          46, 51, 70, 100);
        draw_rounded_border(r, box.x, box.y, box.w, box.h, radius, 1,
                            226, 232, 255, 72);
    }
    int cx = app->ui_theme == UI_THEME_UBIQUITY ? 32 : 34;
    int cy = LANGUAGE_BUTTON.y + LANGUAGE_BUTTON.h / 2;
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, ir, ig, ib, ia);

    for (int dy = -15; dy <= 15; dy++) {
        for (int dx = -15; dx <= 15; dx++) {
            float d = sqrtf((float)(dx * dx + dy * dy));
            if (d >= 12.5f && d <= 15.5f) SDL_RenderDrawPoint(r, cx + dx, cy + dy);
            float md = sqrtf((float)(dx * dx) * 4.2f + (float)(dy * dy));
            if (md >= 12.3f && md <= 15.5f) SDL_RenderDrawPoint(r, cx + dx, cy + dy);
        }
    }
    fill_rect(r, cx - 14, cy - 1, 29, 3, ir, ig, ib, ia);
    fill_rect(r, cx - 12, cy - 7, 25, 2, ir, ig, ib, ia);
    fill_rect(r, cx - 12, cy + 6, 25, 2, ir, ig, ib, ia);
    fill_rect(r, cx - 1, cy - 14, 3, 29, ir, ig, ib, ia);

    Uint8 c = app->language_menu_open ? 255 : 220;
    if (sb_w > SIDEBAR_COLLAPSED_W + 28) {
        SDL_Rect label = {60, LANGUAGE_BUTTON.y, sb_w - 100, 44};
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
    SDL_Renderer *r = app->renderer;
    SDL_Color red = {229, 9, 20, 255};
    SDL_Color white = {245, 245, 245, 255};
    SDL_Color muted = {165, 165, 175, 255};

    if (ubiquity_carousel_enabled(app)) {

        draw_text(app, app->font_micro, "[X] Details    [Triangle] Download / Install    [O] Back",
                  SCREEN_WIDTH - 600, category_y(r)+12, muted, 580);
        return;
    }
    fill_rect(r, 0, 1020, SCREEN_WIDTH, 60, 10, 10, 14, 248);
    fill_rect(r, 0, 1020, SCREEN_WIDTH, 2, 229, 9, 20, 255);

    int cur_x = CONTENT_X;
    if (app->active_row != VIEW_SEARCH && app->active_row != VIEW_SETTINGS) {
        int w1 = draw_text(app, app->font_micro, "[←→↑↓] Nawigacja  [X] Wybierz", cur_x, 1041, muted, 0);
        cur_x += (w1 > 0 ? w1 + 20 : 260);
        int w2 = draw_text(app, app->font_micro, TR(app, TXT_BACK_HINT), cur_x, 1041, muted, 0);
        cur_x += (w2 > 0 ? w2 + 20 : 140);
        int w3 = draw_text(app, app->font_micro, TR(app, TXT_REFRESH_HINT), cur_x, 1041, muted, 0);
        cur_x += (w3 > 0 ? w3 + 24 : 180);
    }

    char sync_info[256];
    SDL_LockMutex(app->catalog_mutex);
    switch (app->sync_status_id) {
        case TXT_SYNC_SUBCATALOG:
            snprintf(sync_info, sizeof(sync_info), TR(app, TXT_SYNC_SUBCATALOG), app->sync_current, app->sync_total);
            break;
        case TXT_SYNC_COVER:
            snprintf(sync_info, sizeof(sync_info), TR(app, TXT_SYNC_COVER), app->sync_title, app->sync_current, app->sync_total);
            break;
        case TXT_SYNC_DONE:
            snprintf(sync_info, sizeof(sync_info), TR(app, TXT_SYNC_DONE), app->catalog_count, app->sync_sources);
            break;
        default:
            snprintf(sync_info, sizeof(sync_info), "%s", TR(app, app->sync_status_id));
            break;
    }
    SDL_UnlockMutex(app->catalog_mutex);

    char clean_sync[256];
    if (strncmp(sync_info, "Pegasus-DL", 10) == 0)
        snprintf(clean_sync, sizeof(clean_sync), "Katalogi%s", sync_info + 10);
    else
        snprintf(clean_sync, sizeof(clean_sync), "%s", sync_info);

    draw_single_line(app, app->font_micro, clean_sync, cur_x, 1041, (SDL_Color){210, 210, 215, 255},
                     (SDL_Rect){cur_x, 1036, 1210 - cur_x, 28});

    render_language_control(app);

    char telemetry[256];
#ifdef __PS5__
    snprintf(telemetry, sizeof(telemetry), "data: %.0fGB | ext0: %.0fGB | CPU: %d°C | GPU: %d°C",
             app->stats.data_free_gb, app->stats.ext0_free_gb, app->stats.cpu_temp_c, app->stats.gpu_temp_c);
#else
    snprintf(telemetry, sizeof(telemetry), "data: null | ext0: null | CPU: null | GPU: null");
#endif
    draw_single_line(app, app->font_micro, telemetry, 1225, 1041, muted, (SDL_Rect){1225, 1036, 520, 28});

    {
        int fp_w = draw_text(app, app->font_small, "O", SCREEN_WIDTH - 135, 1039, red, 0);
        draw_text(app, app->font_small, "bsidian", SCREEN_WIDTH - 135 + (fp_w > 0 ? fp_w + 1 : 14), 1039, white, 0);
    }
}

static void render_language_menu(WolodriveApp *app) {
    if (!app->language_menu_open) { language_highlight.initialized=false; return; }
    SDL_Renderer *r = app->renderer;
    SDL_Color white = {245, 245, 250, 255};
    SDL_Color muted = {170, 170, 180, 255};
    drawGlassPill(r,(SDL_FRect){LANGUAGE_MENU.x,LANGUAGE_MENU.y,LANGUAGE_MENU.w,LANGUAGE_MENU.h},GLASS_PANEL);
    drawHighlight(r,&language_highlight,(SDL_FRect){LANGUAGE_MENU.x+8,LANGUAGE_MENU.y+8+app->language_menu_index*44,LANGUAGE_MENU.w-16,44},false);
    for (int i = 0; i < LANG_COUNT; i++) {
        int y = LANGUAGE_MENU.y + 8 + i * 44;
        bool selected = SDL_AtomicGet(&app->language) == (int)LANGUAGE_ORDER[i];
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
    if (!console) return 0;
    int count = 0;
    SDL_LockMutex(app->catalog_mutex);
    for (int i = 0; i < app->catalog_count; i++) {
        GameItem *item = &app->catalog[i];
        if (item->is_game && settings_accepts_item(app, item) && (library_console_matches_id(console, item->id_code) ||
            (item->format == FMT_CATALOG && library_console_matches_platform(console, item->platform)))) count++;
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
    const LibraryConsole *console = library_console_visible_at(app->library_company, app->library_console_selection, app->active_row == VIEW_EMULATORS);
    snprintf(path, sizeof(path), "%s / %s%s%s", library_section_title(app), company->name,
             app->library_depth == LIBRARY_GAMES ? " / " : "",
             app->library_depth == LIBRARY_GAMES ? console->name : "");
    draw_single_line(app, app->font_small, path, CONTENT_X + 204, 36, muted,
                     (SDL_Rect){CONTENT_X + 204, 24, 1150, 50});
}

/* Library company tiles, console images and emulator logos all reside in the
   public Obsidian-archive. Use the existing asynchronous 48 MiB texture cache
   instead of synchronous IMG_Load / writing image files to PS5 storage. */
static bool render_library_artwork(WolodriveApp *app, const char *path, SDL_Rect bounds) {
    if (!path || !path[0] || strncmp(path, "assets/library/", 15) != 0 ||
        strstr(path, "..") || strlen(path) > 180) return false;
    char url[320];
    snprintf(url, sizeof(url),
             "https://raw.githubusercontent.com/gradwahl/Obsidian-archive/main/%s", path);
    int width = 0, height = 0;
    bool failed = false;
    SDL_Texture *texture = artwork_get_media(app, url, 95, &width, &height, &failed);
    if (!texture && failed) {
        /* Support existing locally bundled art when the network is down. */
        texture = artwork_get_media(app, path, 50, &width, &height, &failed);
    }
    if (!texture || width <= 0 || height <= 0) return false;
    float scale = fminf((float)bounds.w / width, (float)bounds.h / height);
    int w = SDL_max(1, (int)(width * scale));
    int h = SDL_max(1, (int)(height * scale));
    SDL_Rect destination = {bounds.x + (bounds.w - w) / 2,
                            bounds.y + (bounds.h - h) / 2, w, h};
    SDL_RenderCopy(app->renderer, texture, NULL, &destination);
    return true;
}

static void render_library_grid(WolodriveApp *app) {
    SDL_Renderer *r = app->renderer;
    SDL_Color white = {245, 245, 245, 255}, muted = {165, 165, 165, 255};
    bool companies = app->library_depth == LIBRARY_COMPANIES;
    const LibraryCompany *company = library_company_at(app->library_company);
    int selected = companies ? app->library_company_selection : app->library_console_selection;
    int count = companies ? library_company_count() :
        library_console_visible_count(app->library_company, app->active_row == VIEW_EMULATORS);
    int first = selected / LIBRARY_PAGE_SIZE * LIBRARY_PAGE_SIZE;
    draw_text(app, app->font_hero_title, companies ? library_section_title(app) : company->name,
              CONTENT_X, 100, white, 1300);
    draw_text(app, app->font_small, TR(app, companies ? TXT_CHOOSE_COMPANY : TXT_CHOOSE_CONSOLE),
              CONTENT_X, 178, muted, 1200);
    for (int i = first; i < count && i < first + LIBRARY_PAGE_SIZE; i++) {
        SDL_Rect tile = library_tile_target(i);
        const LibraryCompany *brand = companies ? library_company_at(i) : company;
        const LibraryConsole *console = companies ? NULL : library_console_visible_at(app->library_company, i, app->active_row == VIEW_EMULATORS);
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
        if (companies) {
            /* React library cards show the brand name even when art is present. */
            SDL_Rect name = {tile.x + 16, tile.y + 218, tile.w - 32, 30};
            draw_fitted_text(app, app->font_small, brand->name, name, white, name, true);
        } else {
            SDL_Rect logo = {tile.x + 36, tile.y + 220, tile.w - 72, 36};
            if (console->artwork_path) render_library_artwork(app, console->logo_path, logo);
            SDL_Rect title = {tile.x + 16, tile.y + 264, tile.w - 32, 24};
            draw_fitted_text(app, app->font_micro, console->name, title, white, title, true);
        }
        char subtitle[100];
        if (!companies && app->active_row == VIEW_EMULATORS) {
            SDL_strlcpy(subtitle, emulator_release_for_console(console) ?
                        "Release download" : "Browse GitHub releases", sizeof(subtitle));
        } else {
            TextId count_label = companies ? TXT_CONSOLE_COUNT : TXT_LIBRARY_GAME_COUNT;
            snprintf(subtitle, sizeof(subtitle), TR(app, count_label),
                     companies ? library_console_visible_count(i, app->active_row == VIEW_EMULATORS) :
                     library_console_game_count(app, console));
        }
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

/* (u)biquity is intentionally a different composition, not recolored
   Netflix controls. The cover row remains bound to the original D-pad state. */
static void render_ubiquity_home(WolodriveApp *app) {
    SDL_Renderer *r = app->renderer;
    SDL_Color white = {246, 246, 249, 255};
    SDL_Color subdued = {210, 211, 217, 255};
    GameItem *hero = get_focused_item(app);
    CarouselRow *row = active_home_row(app);
    if (!hero || !row->item_count) {
        draw_text(app, app->font_hero_title, view_content_title(app),
                  CONTENT_X + 28, 168, white, 900);
        draw_text(app, app->font_body, empty_view_message(app),
                  CONTENT_X + 28, 320, subdued, 720);
        return;
    }
    if (app->hero_prev_item && app->hero_prev_item != hero && app->hero_crossfade < 1.0f) {
        render_cover_or_placeholder_alpha(app, app->hero_prev_item, 0, 0,
                                          SCREEN_WIDTH, SCREEN_HEIGHT, true,
                                          (Uint8)(255.0f * (1.0f - app->hero_crossfade)));
        render_cover_or_placeholder_alpha(app, hero, 0, 0,
                                          SCREEN_WIDTH, SCREEN_HEIGHT, true,
                                          (Uint8)(255.0f * app->hero_crossfade));
    } else {
        render_cover_or_placeholder(app, hero, 0, 0,
                                    SCREEN_WIDTH, SCREEN_HEIGHT, true);
    }
    render_ubiquity_fades(r);
    /* Upper-right translucent Obsidian capsule, matching the web Home header. */
    {
        const int cap_w = 144, cap_h = 42, cap_x = SCREEN_WIDTH - 30 - cap_w, cap_y = 18;
        draw_rounded_glow(r, cap_x, cap_y, cap_w, cap_h, 21, 6, 8, 8, 14, 70);
        fill_rounded_rect(r, cap_x, cap_y, cap_w, cap_h, 21, 35, 40, 62, 116);
        draw_rounded_border(r, cap_x, cap_y, cap_w, cap_h, 21, 1, 255, 255, 255, 76);
        draw_text(app, app->font_small, "Obsidian", cap_x + 18, cap_y + 10, white, 0);
    }
    char upper[sizeof(hero->title)];
    SDL_strlcpy(upper, hero->title, sizeof(upper));
    for (char *p = upper; *p; p++)
        if (*p >= 'a' && *p <= 'z') *p = (char)(*p - 'a' + 'A');
    SDL_Rect title = {CONTENT_X - 8, 52, 1120, 174};
    draw_fitted_text(app, app->font_ubiquity_title ?
                     app->font_ubiquity_title : app->font_hero_title,
                     upper, title, white, title, false);

    /* Keep the same six visible metadata fields as the browser Ubiquity view. */
    char metadata[6][256];
    snprintf(metadata[0], sizeof(metadata[0]), "%s", hero->source_name);
    snprintf(metadata[1], sizeof(metadata[1]), "%s", hero->id_code);
    snprintf(metadata[2], sizeof(metadata[2]), "v%s", hero->version);
    snprintf(metadata[3], sizeof(metadata[3]), "%s", sys_format_short_badge(hero->format));
    snprintf(metadata[4], sizeof(metadata[4]), "%.1f GB", hero->size_gb);
    snprintf(metadata[5], sizeof(metadata[5]), "* %.1f/10", hero->rating_score);
    int meta_x = CONTENT_X - 8, meta_y = 242;
    for (int field = 0; field < 6; field++) {
        int tw = 0, th = 0;
        if (app->font_small) TTF_SizeUTF8(app->font_small, metadata[field], &tw, &th);
        if (meta_x + tw + 34 > CONTENT_X + 1080) { meta_x = CONTENT_X - 8; meta_y += 40; }
        meta_x = draw_hero_metadata_pill(app, metadata[field], meta_x, meta_y);
    }

    const char *description = hero->description[0] ? hero->description : "";
    SDL_Rect desc_box = {CONTENT_X - 8, 296, 780, 75};
    draw_fitted_text(app, app->font_body, description,
                     desc_box, subdued, desc_box, false);
    const LibraryConsole *selected_console =
        app->active_row == VIEW_GAMES && app->library_depth == LIBRARY_GAMES ?
            library_console_visible_at(app->library_company, app->library_console_selection, app->active_row == VIEW_EMULATORS) : NULL;
    draw_text(app, app->font_row_title,
              app->active_row == VIEW_HOME ?
                  home_category_title(app, app->home_cat_index) :
                  selected_console ? selected_console->name : view_content_title(app),
              CONTENT_X + 24, 708, white, 780);

    /* Widen clip rectangle so cards, focus pill, and glow have full breathing room without clipping */
    SDL_Rect clip = {CONTENT_X - 28, 690, SCREEN_WIDTH - (CONTENT_X - 28), 330};
    SDL_RenderSetClipRect(r, &clip);

    /* Update gliding focus pill position for smooth floating ring transition */
    if (row->item_count > 0) {
        SDL_Rect target_card = app->active_row == VIEW_HOME ?
            home_row_card_target(app, app->home_cat_index, row->selected_col) : card_target(app, row->selected_col);
        float target_pill_x = (float)target_card.x;
        if (row->focus_pill_x == 0.0f) row->focus_pill_x = target_pill_x;
        row->focus_pill_x += (target_pill_x - row->focus_pill_x) * (1.0f - expf(-16.0f * 0.016f));
        if (fabsf(target_pill_x - row->focus_pill_x) < 0.2f) row->focus_pill_x = target_pill_x;
    }

    /* Draw floating crimson focus pill around active card position
       (2.6s double strobe timing:
       float phase = (float)(SDL_GetTicks() % 2600u) / 2600.0f;
       float first = (phase - 0.09f) / 0.038f;
       float second = (phase - 0.21f) / 0.038f;
       float burst = fmaxf(expf(-first * first), expf(-second * second));) */
    if (!app->sidebar_focus && row->item_count > 0) {
        int pill_w = 262;
        int pill_h = 262;
        int pill_x = (int)row->focus_pill_x - 15;
        int pill_y = 755 - 19;

        /* Soft multi-layer crimson glow */
        draw_rounded_glow(r, pill_x - 16, pill_y - 16, pill_w + 32, pill_h + 32, 36, 16, 201, 24, 74, 150);
        draw_rounded_glow(r, pill_x - 8, pill_y - 8, pill_w + 16, pill_h + 16, 30, 10, 89, 13, 34, 120);

        /* Crimson gradient wrapping capsule border */
        draw_rounded_border(r, pill_x, pill_y, pill_w, pill_h, 24, 4, 201, 24, 74, 255);
        draw_rounded_border(r, pill_x + 1, pill_y + 1, pill_w - 2, pill_h - 2, 23, 2, 255, 51, 102, 230);
        draw_rounded_border(r, pill_x - 1, pill_y - 1, pill_w + 2, pill_h + 2, 25, 2, 89, 13, 34, 200);
    }

    for (int col = 0; col < row->item_count; col++) {
        SDL_Rect base_card = app->active_row == VIEW_HOME ?
            home_row_card_target(app, app->home_cat_index, col) : card_target(app, col);
        if (base_card.x + base_card.w < clip.x || base_card.x > clip.x + clip.w) continue;

        bool is_selected = (col == row->selected_col) && !app->sidebar_focus;
        float scale = is_selected ? 1.08f : 0.92f;
        int card_w = (int)(base_card.w * scale);
        int card_h = (int)(base_card.h * scale);
        int card_x = base_card.x - (card_w - base_card.w) / 2;
        int card_y = base_card.y - (card_h - base_card.h) / 2 - (is_selected ? 4 : 0);
        SDL_Rect card = {card_x, card_y, card_w, card_h};

        GameItem *item = row->items[col];
        int iw = 0, ih = 0;
        SDL_Texture *cover = artwork_get(app, item, false, 80, &iw, &ih);
        fill_rect(r, card.x, card.y, card.w, card.h, 15, 17, 23, is_selected ? 255 : 210);
        /* Original reference: 232px square tile. */
        SDL_Rect cover_rect = {card.x, card.y, card.w, card.h};
        if (cover && iw > 0 && ih > 0) {
            int square = SDL_min(iw, ih);
            SDL_Rect src = {(iw - square) / 2, (ih - square) / 2, square, square};
            SDL_RenderCopy(r, cover, &src, &cover_rect);
        } else {
            SDL_Rect fallback = {cover_rect.x + 8, cover_rect.y + 26,
                                 cover_rect.w - 16, cover_rect.h - 52};
            draw_fitted_text(app, app->font_small, item->title, fallback,
                             white, fallback, true);
        }

        const bool downloadable =
            item->state == STATE_AVAILABLE && item->format != FMT_CATALOG;
        SDL_Rect footer = {card.x, card.y + card.h - 40, card.w, 40};
        fill_rect(r, footer.x, footer.y, footer.w, footer.h, 9, 11, 16, is_selected ? 240 : 200);
        fill_rect(r, footer.x, footer.y, footer.w, 1, 71, 72, 80, 145);
        SDL_Rect caption = {footer.x + 30, footer.y + 4, footer.w - 60, footer.h - 8};
        draw_fitted_text(app, app->font_small, item->title,
                         caption, white, footer, true);
        if (downloadable) {
            int ax = footer.x + footer.w - 18;
            int ay = footer.y + 10;
            SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
            SDL_SetRenderDrawColor(r, 243, 244, 249, 255);
            SDL_RenderDrawLine(r, ax, ay, ax, ay + 13);
            SDL_RenderDrawLine(r, ax + 1, ay, ax + 1, ay + 13);
            SDL_RenderDrawLine(r, ax - 5, ay + 8, ax, ay + 13);
            SDL_RenderDrawLine(r, ax + 6, ay + 8, ax + 1, ay + 13);
        }
        const int ubq_radius = (int)(18.0f * scale);
        for (int row_i = 0; row_i < ubq_radius; ++row_i) {
            int inset = rounded_inset(row_i, card.w, card.h, ubq_radius);
            if (inset > 0) {
                fill_rect(r, card.x, card.y + row_i, inset, 1, 0, 0, 0, 255);
                fill_rect(r, card.x + card.w - inset, card.y + row_i, inset, 1, 0, 0, 0, 255);
                fill_rect(r, card.x, card.y + card.h - 1 - row_i, inset, 1, 0, 0, 0, 255);
                fill_rect(r, card.x + card.w - inset, card.y + card.h - 1 - row_i, inset, 1, 0, 0, 0, 255);
            }
        }
        if (col == row->selected_col && !app->sidebar_focus) {
            /* Match web's 2.6-second double-strobe, with a steady single white
               outline. Only the halo pulses; the cover never flashes. */
            float phase = (float)(SDL_GetTicks() % 2600u) / 2600.0f;
            float first = (phase - 0.09f) / 0.038f;
            float second = (phase - 0.21f) / 0.038f;
            float burst = fmaxf(expf(-first * first), expf(-second * second));
            Uint8 red_alpha = (Uint8)(39.0f + 107.0f * burst);
            Uint8 white_alpha = (Uint8)(42.0f + 110.0f * burst);
            draw_rounded_glow(r, card.x, card.y, card.w, card.h,
                              2, 13, 229, 9, 20, red_alpha);
            draw_rounded_glow(r, card.x, card.y, card.w, card.h,
                              2, 8, 248, 248, 255, white_alpha);
            draw_rounded_border(r, card.x, card.y, card.w, card.h,
                                2, 2, 250, 250, 255, 255);
        }
    }
    SDL_RenderSetClipRect(r, NULL);
    char count[48];
    snprintf(count, sizeof(count), "%d / %d", row->selected_col + 1, row->item_count);
    draw_text(app, app->font_micro, count, SCREEN_WIDTH - 175, 719, subdued, 0);
    if (app->active_row == VIEW_HOME) {
        int pos = CONTENT_X + 24;
        SDL_Rect targets[HOME_CAT_COUNT];
        for (int cat=0;cat<HOME_CAT_COUNT;cat++) {
            int width=0,height=0;
            TTF_SizeUTF8(app->font_micro,home_category_title(app,cat),&width,&height);
            targets[cat]=(SDL_Rect){pos,category_y(r),SDL_max(80,width+32),40};
            if(cat!=app->home_cat_index) drawGlassPill(r,(SDL_FRect){pos,category_y(r),targets[cat].w,40},GLASS_NEUTRAL);
            pos+=targets[cat].w+12;
        }
        SDL_Rect chosen=targets[app->home_cat_index];
        drawHighlight(r,&category_highlight,(SDL_FRect){chosen.x,chosen.y,chosen.w,chosen.h},false);
        for(int cat=0;cat<HOME_CAT_COUNT;cat++) {
            SDL_Rect box=targets[cat]; box.x+=16;box.w-=32;
            draw_fitted_text(app,app->font_micro,home_category_title(app,cat),box,
                             cat==app->home_cat_index?white:subdued,box,true);
        }
    }
}

/* Emulators are listed by console; specific builds come from validated
   Obsidian GitHub release assets. Unmatched consoles link to the hub only. */
static void render_emulator_release_screen(WolodriveApp *app) {
    SDL_Renderer *renderer = app->renderer;
    SDL_Color white={246,246,249,255}, muted={184,186,194,255};
    const LibraryConsole *console =
        library_console_visible_at(app->library_company, app->library_console_selection, app->active_row == VIEW_EMULATORS);
    if (!console) return;
    const EmulatorReleaseAsset *match=emulator_release_for_console(console);
    fill_rect(renderer,RAIL_WIDTH,0,SCREEN_WIDTH-RAIL_WIDTH,SCREEN_HEIGHT,
              11,13,20,255);
    SDL_Rect image={CONTENT_X+40,170,360,340};
    if (!render_library_artwork(app,console->artwork_path, image))
        draw_fitted_text(app,app->font_body,console->name,image,white,image,true);
    SDL_Rect name={CONTENT_X+430,176,980,100};
    draw_fitted_text(app,app->font_hero_title,console->name,name,white,name,false);
    draw_text(app,app->font_small,"Obsidian-Emulators / GitHub Releases",
              CONTENT_X+430,300,muted,930);
    if(match) {
        draw_text(app,app->font_row_title,match->filename,CONTENT_X+430,375,white,1060);
        char size[160];
        snprintf(size,sizeof(size),"Verified release %s  /  %.1f MB",
                 match->label,(double)match->size/(1024.0*1024.0));
        draw_text(app,app->font_small,size,CONTENT_X+430,434,muted,960);
        draw_text(app,app->font_micro,"SHA-256 recorded in bundled release metadata",
                  CONTENT_X+430,481,muted,960);
        if (match == &EMULATOR_RETROARCH_ASSET)
            draw_text(app,app->font_micro,
                "Shared RetroArch ZIP; individual PS5 cores not verified",
                CONTENT_X+430,528,muted,1000);
    }else{
        SDL_Rect msg={CONTENT_X+430,370,1080,100};
        draw_fitted_text(app,app->font_body,
          "No individual emulator asset is verified for this console. Browse GitHub releases.",
          msg,muted,msg,false);
    }
    SDL_Rect button={CONTENT_X+40,590,660,100};
    fill_rounded_rect(renderer,button.x,button.y,button.w,button.h,11,30,33,43,250);
    draw_rounded_border(renderer,button.x,button.y,button.w,button.h,
                        11,2,246,246,249,245);
    SDL_Rect label={button.x+15,button.y+8,button.w-30,button.h-16};
    draw_fitted_text(app,app->font_row_title,
      match ? "X  Download verified file" : "X  Open releases page",
      label,white,label,true);
    draw_text(app,app->font_micro,EMULATOR_RELEASES_URL,CONTENT_X+40,738,muted,1400);
    draw_text(app,app->font_micro,
      "Downloaded files are saved only. No automatic extraction, installation or launch.",
      CONTENT_X+40,785,muted,1400);
    if(match) {
        int status=emulator_download_status(app);
        char report[180];
        if(status==1) snprintf(report,sizeof(report),"Downloading: %d%%",
                                  emulator_download_percent(app));
        else if(status==2) SDL_strlcpy(report,"SHA-256 VERIFIED / SAVED",sizeof(report));
        else if(status==3) SDL_strlcpy(report,"Download failed or checksum mismatch",sizeof(report));
        else if(status==4) SDL_strlcpy(report,"Download cancelled",sizeof(report));
        else SDL_strlcpy(report,"Ready to download",sizeof(report));
        draw_text(app,app->font_body,report,CONTENT_X+40,848,white,970);
    }
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

    if (app->active_row == VIEW_FILES) {
        render_files(app);
    } else if (app->active_row == VIEW_SETTINGS) {
        render_settings(app);
    } else if (app->active_row == VIEW_SEARCH) {
        render_search(app);
    } else if (is_library_view(app->active_row) && app->library_depth != LIBRARY_GAMES) {
        render_library_grid(app);
    } else if (app->active_row == VIEW_EMULATORS && app->library_depth == LIBRARY_GAMES) {
        render_emulator_release_screen(app);
    } else {
        if (ubiquity_carousel_enabled(app)) {
            render_ubiquity_home(app);
        } else {
            render_hero_banner(app);
            render_carousels(app);
        }
        if (is_library_view(app->active_row)) render_library_breadcrumb(app);
    }
    render_sidebar(app);
    render_top_bar(app);
    if (app->active_row == VIEW_FILES) render_files_dialog(app);
    if (app->detail_open) render_game_details(app);
    render_toast_notification(app);
    render_language_menu(app);

    SDL_RenderPresent(r);
}
