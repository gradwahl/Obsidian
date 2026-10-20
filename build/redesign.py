from pathlib import Path
p = Path('src/ui_netflix.c')
s = p.read_text()
def replace_function(name, next_name, body):
    global s
    start = s.index(name)
    end = s.index(next_name, start)
    s = s[:start] + body + '\n\n' + s[end:]

# Remove decoration helpers superseded by the TV reference's flat surfaces.
replace_function('static void draw_glow_frame', 'static int draw_text', '')
replace_function('static int draw_glass_badge', 'static void ensure_cover_texture_loaded', '')
s = s.replace('FONT_CANDIDATES_BOLD, 46', 'FONT_CANDIDATES_BOLD, 52')
s = s.replace('FONT_CANDIDATES_BOLD, 22', 'FONT_CANDIDATES_BOLD, 28')
s = s.replace('FONT_CANDIDATES_REGULAR, 19', 'FONT_CANDIDATES_REGULAR, 26')
s = s.replace('FONT_CANDIDATES_BOLD, 15', 'FONT_CANDIDATES_BOLD, 20')
s = s.replace('FONT_CANDIDATES_BOLD, 13', 'FONT_CANDIDATES_BOLD, 16')
start = s.index('    const float card_stride')
end = s.index('static void render_hero_banner', start)
s = s[:start] + '''    const float card_stride = CARD_WIDTH + CARD_GAP;
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
        Uint8 alpha = (Uint8)(255.0f * (1.0f - t) * (1.0f - t));
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

''' + s[end:]
# Keep the existing translated, scrollable description renderer.
desc_start = s.index('    char description[MAX_TRANSLATED_DESCRIPTION_BYTES];', s.index('static void render_hero_banner'))
desc_end = s.index('    /* Docelowa', desc_start)
description = s[desc_start:desc_end]
replace_function('static void render_hero_banner', 'static void render_top_bar', '''static const char *action_label(WolodriveApp *app, GameItem *item) {
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
    fill_rect(r, RAIL_WIDTH, 0, 730, 540, 0, 0, 0, 85);
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
''' + description + '''}

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
        int tw = 0, th = 0;
        TTF_SizeUTF8(app->font_small, status, &tw, &th);
        SDL_Rect footer_clip = {SDL_max(card.x + 8, clip.x), footer_y, 0, CARD_FOOTER};
        footer_clip.w = SDL_max(0, SDL_min(card.x + card.w - 8, SCREEN_WIDTH) - footer_clip.x);
        draw_single_line(r, app->font_small, status, card.x + SDL_max(8, (card.w - tw) / 2),
                         footer_y + (CARD_FOOTER - th) / 2,
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

''')
# Replace the dense top bar with small overlays and bottom status.
start = s.index('    /* Wielowarstwowy', s.index('static void render_top_bar'))
end = s.index('    char sync_info', start)
s = s[:start] + '''    SDL_Color red = {229, 9, 20, 255};
    SDL_Color white = {245, 245, 245, 255};
    SDL_Color muted = {155, 155, 155, 255};
    draw_text(r, app->font_logo, "WOLODRIVE", 1620, 956, red, 0);
    draw_text(r, app->font_micro, "← → / D-pad    X / Enter", CONTENT_X, 984, muted, 0);
    draw_text(r, app->font_micro, TR(app, TXT_REFRESH_HINT), 500, 984, muted, 0);

''' + s[end:]
s = s.replace('draw_single_line(r, app->font_small, sync_info, 305, 27, muted, (SDL_Rect){305, 14, 650, 44});',
'''draw_single_line(r, app->font_micro, sync_info, CONTENT_X, 1032, muted,
                     (SDL_Rect){CONTENT_X, 1032, 1040, 32});''')
s = s.replace('draw_single_line(r, app->font_micro, telemetry, 980, 28, white, (SDL_Rect){980, 14, 510, 44});',
'''draw_single_line(r, app->font_micro, telemetry, 1200, 1032, muted,
                     (SDL_Rect){1200, 1032, 670, 32});''')
s = s.replace('    render_background_and_ambient(app);\n', '')
s = s.replace('    render_vignette(r);', '    render_sidebar(app);')
s = s.replace('SDL_SetRenderDrawColor(r, COLOR_BG_TOP_R, COLOR_BG_TOP_G, COLOR_BG_TOP_B, 255);',
              'SDL_SetRenderDrawColor(r, 0, 0, 0, 255);')
p.write_text(s)
