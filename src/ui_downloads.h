/* Linux/PS5 SDL2 Downloads Dashboard matching the web preview's 2-column PS5 design. */
#ifndef UI_DOWNLOADS_H
#define UI_DOWNLOADS_H

#include "wolodrive.h"

static void render_downloads(WolodriveApp *app) {
    SDL_Renderer *r = app->renderer;
    SDL_Color white = {245, 245, 245, 255};
    SDL_Color muted = {165, 165, 175, 255};
    SDL_Color red = {229, 9, 20, 255};
    SDL_Color green = {34, 197, 94, 255};

    int start_x = 312;
    int top_y = 142;

    /* View Title & Header */
    draw_text(app, app->font_hero_title, TR(app, TXT_NAV_DOWNLOADS), start_x, top_y, white, 0);
    draw_text(app, app->font_body, TR(app, TXT_DOWNLOADS_HELP),
              start_x, top_y + 60, muted, 1200);

    /* Dynamic filter counts from real app catalog */
    int active_count = 0;
    int completed_count = 0;
    int error_count = 0;

    SDL_LockMutex(app->catalog_mutex);
    for (int i = 0; i < app->catalog_count; i++) {
        if (app->catalog[i].state == STATE_DOWNLOADING) active_count++;
        else if (app->catalog[i].state == STATE_INSTALLED) completed_count++;
    }

    /* Filter Pills in Header */
    int pill_x = 1220;
    int pill_y = top_y + 10;
    char filter0[64], filter1[64], filter2[64];
    snprintf(filter0, sizeof(filter0), "%s · %d", TR(app, TXT_STATUS_ACTIVE), active_count);
    snprintf(filter1, sizeof(filter1), "%s · %d", TR(app, TXT_STATUS_COMPLETED), completed_count);
    snprintf(filter2, sizeof(filter2), "%s · %d", TR(app, TXT_STATUS_ERRORS), error_count);
    const char *filters[3] = {filter0, filter1, filter2};

    for (int i = 0; i < 3; i++) {
        bool active = (app->downloads_filter == i);
        int pw = 180;
        int ph = 42;
        int px = pill_x + i * 200;

        if (active) {
            draw_rounded_glow(r, px, pill_y, pw, ph, 21, 6, 229, 9, 20, 100);
            fill_rounded_rect(r, px, pill_y, pw, ph, 21, 30, 30, 42, 255);
            draw_rounded_border(r, px, pill_y, pw, ph, 21, 2, 229, 9, 20, 255);
        } else {
            fill_rounded_rect(r, px, pill_y, pw, ph, 21, 20, 20, 28, 200);
            draw_rounded_border(r, px, pill_y, pw, ph, 21, 1, 255, 255, 255, 30);
        }

        draw_fitted_text(app, app->font_small, filters[i],
                         (SDL_Rect){px + 10, pill_y + 6, pw - 20, ph - 12},
                         active ? white : muted, (SDL_Rect){px, pill_y, pw, ph}, true);
    }

    /* 2-Column Layout Bounds */
    int left_x = 312;
    int left_w = 980;
    int right_x = 1320;
    int right_w = 540;
    int content_y = 250;

    /* LEFT COLUMN: Filtered Task Cards or Empty Glass Message */
    int card_y = content_y;
    int matching_count = 0;
    GameItem *matching_items[MAX_CATALOG_ITEMS];

    for (int i = 0; i < app->catalog_count; i++) {
        GameItem *it = &app->catalog[i];
        if (app->downloads_filter == 0 && it->state == STATE_DOWNLOADING) {
            matching_items[matching_count++] = it;
        } else if (app->downloads_filter == 1 && it->state == STATE_INSTALLED) {
            matching_items[matching_count++] = it;
        }
    }
    SDL_UnlockMutex(app->catalog_mutex);

    if (matching_count == 0) {
        fill_rounded_rect(r, left_x, card_y, left_w, 140, 18, 20, 20, 28, 220);
        draw_rounded_border(r, left_x, card_y, left_w, 140, 18, 1, 255, 255, 255, 35);
        draw_fitted_text(app, app->font_body, TR(app, TXT_NO_ACTIVE_DOWNLOADS),
                         (SDL_Rect){left_x + 40, card_y + 50, left_w - 80, 40},
                         muted, (SDL_Rect){left_x + 20, card_y + 20, left_w - 40, 100}, true);
    } else {
        int max_show = (matching_count > 5) ? 5 : matching_count;
        for (int i = 0; i < max_show; i++) {
            GameItem *it = matching_items[i];
            int cy = card_y + i * 165;
            int card_h = 150;

            fill_rounded_rect(r, left_x, cy, left_w, card_h, 18, 20, 20, 28, 220);
            draw_rounded_border(r, left_x, cy, left_w, card_h, 18, 1, 255, 255, 255, 35);

            /* Cover Thumbnail */
            SDL_Rect cover_rect = {left_x + 20, cy + 15, 96, 120};
            fill_rounded_rect(r, cover_rect.x, cover_rect.y, cover_rect.w, cover_rect.h, 12, 35, 35, 48, 255);
            SDL_Texture *tex = artwork_get(app, it, ARTWORK_COVER);
            if (tex) SDL_RenderCopy(r, tex, NULL, &cover_rect);
            else draw_fitted_text(app, app->font_small, it->title, cover_rect, white, cover_rect, true);

            /* Title & Size */
            int info_x = left_x + 132;
            draw_text(app, app->font_row_title, it->title, info_x, cy + 16, white, 600);

            char size_buf[32];
            snprintf(size_buf, sizeof(size_buf), "%.1f GB", it->size_gb > 0 ? it->size_gb : 0.0f);
            draw_text(app, app->font_small, size_buf, left_x + left_w - 120, cy + 20, muted, 0);

            /* 4 Stage Pills */
            const char *stages[4] = {
                TR(app, TXT_STAGE_DOWNLOADING),
                TR(app, TXT_STAGE_UNPACKING),
                TR(app, TXT_STAGE_INSTALLING),
                TR(app, TXT_STAGE_READY)
            };
            int stage_y = cy + 50;
            int active_s = (it->state == STATE_INSTALLED) ? 3 : (it->progress < 80 ? 0 : it->progress < 95 ? 1 : 2);

            for (int s = 0; s < 4; s++) {
                bool is_act = (s == active_s);
                int sp_w = 125, sp_h = 28, sp_x = info_x + s * 135;

                if (is_act) {
                    fill_rounded_rect(r, sp_x, stage_y, sp_w, sp_h, 14, 229, 9, 20, 255);
                    draw_rounded_glow(r, sp_x, stage_y, sp_w, sp_h, 14, 4, 229, 9, 20, 120);
                } else {
                    fill_rounded_rect(r, sp_x, stage_y, sp_w, sp_h, 14, 24, 24, 34, 255);
                    draw_rounded_border(r, sp_x, stage_y, sp_w, sp_h, 14, 1, 255, 255, 255, 15);
                }

                draw_fitted_text(app, app->font_micro, stages[s],
                                 (SDL_Rect){sp_x + 6, stage_y + 4, sp_w - 12, sp_h - 8},
                                 is_act ? white : muted, (SDL_Rect){sp_x, stage_y, sp_w, sp_h}, true);
            }

            /* Progress Bar */
            int bar_x = info_x;
            int bar_y = cy + 88;
            int bar_w = left_w - 240;
            int bar_h = 10;
            float prog = (it->state == STATE_INSTALLED) ? 100.0f : it->progress;

            fill_rounded_rect(r, bar_x, bar_y, bar_w, bar_h, 5, 28, 28, 40, 255);
            int fill_w = (int)(bar_w * (prog / 100.0f));
            if (fill_w > 0) {
                if (it->state == STATE_INSTALLED) {
                    fill_rounded_rect(r, bar_x, bar_y, fill_w, bar_h, 5, 34, 197, 94, 255);
                } else {
                    fill_rounded_rect(r, bar_x, bar_y, fill_w, bar_h, 5, 229, 9, 20, 255);
                    draw_rounded_glow(r, bar_x, bar_y, fill_w, bar_h, 5, 4, 229, 9, 20, 100);
                }
            }

            /* Stats Line below Progress Bar */
            char prog_buf[64];
            snprintf(prog_buf, sizeof(prog_buf), "%.0f%%  ·  %s", prog,
                     it->state == STATE_INSTALLED ? TR(app, TXT_STAGE_READY) : "1.10 GB/s");
            draw_text(app, app->font_micro, prog_buf, bar_x, bar_y + 16, white, 0);
        }
    }

    /* RIGHT COLUMN: 2 Telemetry Widgets */
    /* Widget 1: Download Speed Telemetry */
    int w1_y = content_y;
    int w1_h = 320;

    fill_rounded_rect(r, right_x, w1_y, right_w, w1_h, 18, 20, 20, 28, 220);
    draw_rounded_border(r, right_x, w1_y, right_w, w1_h, 18, 1, 255, 255, 255, 35);

    draw_text(app, app->font_row_title, TR(app, TXT_SPEED_DOWNLOAD), right_x + 20, w1_y + 20, red, 0);

    if (active_count > 0) {
        draw_text(app, app->font_hero_title, "1.10 GB/s", right_x + 20, w1_y + 58, white, 0);
        draw_text(app, app->font_small, "Avg: 940 MB/s  ·  Peak: 1.28 GB/s", right_x + 20, w1_y + 112, muted, 0);
    } else {
        draw_text(app, app->font_hero_title, "0.00 MB/s", right_x + 20, w1_y + 58, white, 0);
        draw_text(app, app->font_small, "Avg: 0.00 MB/s  ·  Peak: 0.00 MB/s", right_x + 20, w1_y + 112, muted, 0);
    }

    /* Area Chart inside Speed Telemetry Widget */
    int chart_x = right_x + 20;
    int chart_y = w1_y + 160;
    int chart_w = right_w - 40;
    int chart_h = 130;

    fill_rounded_rect(r, chart_x, chart_y, chart_w, chart_h, 10, 15, 15, 22, 255);

    if (active_count > 0) {
        int sample_points[10] = {30, 45, 70, 55, 80, 65, 90, 75, 95, 85};
        int num_p = 10;
        int step_x = chart_w / (num_p - 1);

        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        for (int i = 0; i < num_p - 1; i++) {
            int x1 = chart_x + i * step_x;
            int x2 = chart_x + (i + 1) * step_x;
            int y1 = chart_y + chart_h - (sample_points[i] * chart_h / 100);
            int y2 = chart_y + chart_h - (sample_points[i + 1] * chart_h / 100);

            for (int px = x1; px <= x2; px++) {
                float t = (float)(px - x1) / (float)(x2 - x1);
                int py = (int)(y1 + t * (y2 - y1));
                fill_rect(r, px, py, 1, (chart_y + chart_h) - py, 229, 9, 20, 60);
            }

            SDL_SetRenderDrawColor(r, 229, 9, 20, 255);
            SDL_RenderDrawLine(r, x1, y1, x2, y2);
            SDL_RenderDrawLine(r, x1, y1 + 1, x2, y2 + 1);
        }
    } else {
        /* Flat line at bottom when no active downloads */
        SDL_SetRenderDrawColor(r, 229, 9, 20, 100);
        SDL_RenderDrawLine(r, chart_x, chart_y + chart_h - 4, chart_x + chart_w, chart_y + chart_h - 4);
    }

    /* Widget 2: Internal SSD Storage */
    int w2_y = w1_y + w1_h + 20;
    int w2_h = 280;

    fill_rounded_rect(r, right_x, w2_y, right_w, w2_h, 18, 20, 20, 28, 220);
    draw_rounded_border(r, right_x, w2_y, right_w, w2_h, 18, 1, 255, 255, 255, 35);

    draw_text(app, app->font_row_title, TR(app, TXT_INTERNAL_SSD), right_x + 20, w2_y + 20, white, 0);

#ifdef __PS5__
    char ssd_title[64];
    snprintf(ssd_title, sizeof(ssd_title), "%.0f GB / %.0f GB", app->stats.data_total_gb - app->stats.data_free_gb, app->stats.data_total_gb);
    draw_text(app, app->font_small, ssd_title, right_x + right_w - 180, w2_y + 22, muted, 0);

    int sbar_x = right_x + 20;
    int sbar_y = w2_y + 60;
    int sbar_w = right_w - 40;
    int sbar_h = 24;

    fill_rounded_rect(r, sbar_x, sbar_y, sbar_w, sbar_h, 12, 25, 25, 35, 255);

    float used_pct = (app->stats.data_total_gb > 0) ? ((app->stats.data_total_gb - app->stats.data_free_gb) / app->stats.data_total_gb) : 0.5f;
    int used_w = (int)(sbar_w * used_pct);

    fill_rounded_rect(r, sbar_x, sbar_y, used_w, sbar_h, 12, 229, 9, 20, 255);
    draw_rounded_glow(r, sbar_x, sbar_y, used_w, sbar_h, 12, 4, 229, 9, 20, 100);

    /* Legend */
    int leg_y = sbar_y + 42;
    fill_rounded_rect(r, right_x + 20, leg_y, 12, 12, 6, 229, 9, 20, 255);
    char leg_buf[128];
    snprintf(leg_buf, sizeof(leg_buf), "%s: %.1f GB", TR(app, TXT_GAMES_APPS), app->stats.data_total_gb - app->stats.data_free_gb);
    draw_text(app, app->font_small, leg_buf, right_x + 40, leg_y - 2, white, 0);

    fill_rounded_rect(r, right_x + 20, leg_y + 32, 12, 12, 6, 28, 28, 40, 255);
    snprintf(leg_buf, sizeof(leg_buf), "%s: %.1f GB", TR(app, TXT_FREE_SPACE), app->stats.data_free_gb);
    draw_text(app, app->font_small, leg_buf, right_x + 40, leg_y + 30, green, 0);
#else
    draw_text(app, app->font_small, TR(app, TXT_CONSOLE_ONLY), right_x + right_w - 200, w2_y + 22, muted, 0);

    int sbar_x = right_x + 20;
    int sbar_y = w2_y + 60;
    int sbar_w = right_w - 40;
    int sbar_h = 24;

    fill_rounded_rect(r, sbar_x, sbar_y, sbar_w, sbar_h, 12, 25, 25, 35, 255);

    int leg_y = sbar_y + 42;
    fill_rounded_rect(r, right_x + 20, leg_y, 12, 12, 6, 80, 80, 100, 255);
    draw_text(app, app->font_small, "Gry / Apps: N/A", right_x + 40, leg_y - 2, muted, 0);

    fill_rounded_rect(r, right_x + 20, leg_y + 32, 12, 12, 6, 28, 28, 40, 255);
    draw_text(app, app->font_small, "Free Space: N/A", right_x + 40, leg_y + 30, muted, 0);
#endif
}

static bool handle_downloads_input(WolodriveApp *app, const SDL_Event *ev) {
    if (app->active_row != VIEW_DOWNLOADS) return false;

    if (ev->type == SDL_KEYDOWN) {
        if (ev->key.keysym.sym == SDLK_LEFT || ev->key.keysym.sym == SDLK_a) {
            if (app->downloads_filter > 0) app->downloads_filter--;
            else app->sidebar_focus = true;
            return true;
        }
        if (ev->key.keysym.sym == SDLK_RIGHT || ev->key.keysym.sym == SDLK_d) {
            if (app->downloads_filter < 2) app->downloads_filter++;
            return true;
        }
        if (ev->key.keysym.sym == SDLK_ESCAPE || ev->key.keysym.sym == SDLK_BACKSPACE || ev->key.keysym.sym == SDLK_b) {
            app->sidebar_focus = true;
            return true;
        }
    } else if (ev->type == SDL_CONTROLLERBUTTONDOWN) {
        if (ev->cbutton.button == SDL_CONTROLLER_BUTTON_DPAD_LEFT) {
            if (app->downloads_filter > 0) app->downloads_filter--;
            else app->sidebar_focus = true;
            return true;
        }
        if (ev->cbutton.button == SDL_CONTROLLER_BUTTON_DPAD_RIGHT) {
            if (app->downloads_filter < 2) app->downloads_filter++;
            return true;
        }
        if (ev->cbutton.button == SDL_CONTROLLER_BUTTON_B) {
            app->sidebar_focus = true;
            return true;
        }
    }
    return false;
}

#endif /* UI_DOWNLOADS_H */
