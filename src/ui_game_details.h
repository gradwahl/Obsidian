/* Shared game panel used by every catalog entry point. */
enum { GAME_INSTALL, GAME_DLC, GAME_DETAILS, GAME_TAB_COUNT, GAME_SCREENSHOTS };
static const SDL_Rect GAME_CLOSE = {1820, 215, 60, 60};
static const TextId GAME_TABS[] = {TXT_GAME_INSTALL, TXT_GAME_DLC, TXT_GAME_DETAILS};

static SDL_Rect game_tab_target(int tab) {
    return (SDL_Rect){CONTENT_X + tab * 380, 850, 360, 65};
}
static SDL_Rect game_entry_target(int index) {
    return (SDL_Rect){CONTENT_X + 20 + index % 3 * 500, 420, 470, 330};
}
static void ui_open_game(WolodriveApp *app, GameItem *item) {
    if (!item) return;
    app->screenshot_viewer = false;
    app->detail_item = item;
    app->detail_parent = NULL;
    app->detail_open = true;
    app->detail_ubiquity_options = false;
    app->detail_tab = GAME_INSTALL;
    app->detail_selection = app->detail_scroll = 0;
    app->detail_content_focus = false;
    track_description_focus(app, item);
}
static void game_back(WolodriveApp *app) {
    if (app->ui_theme == UI_THEME_UBIQUITY && app->detail_ubiquity_options) {
        app->detail_ubiquity_options = false;
        app->detail_selection = 1;
        return;
    }
    if (app->detail_parent) {
        GameItem *parent = app->detail_parent;
        ui_open_game(app, parent);
        app->detail_tab = GAME_DLC;
    } else app->detail_open = false;
}
static int game_entries(WolodriveApp *app, GameItem **entries) {
    GameItem *base = app->detail_item;
    int count = 0;
    SDL_LockMutex(app->catalog_mutex);
    for (int i = 0; i < app->catalog_count; i++) {
        GameItem *candidate = &app->catalog[i];
        if (!settings_accepts_item(app, candidate)) continue;
        bool related_dlc = candidate->is_dlc && !base->is_dlc &&
            base->id_code[0] && strcmp(base->id_code, "PPSA00001") != 0 &&
            SDL_strcasecmp(candidate->parent_title_id[0] ? candidate->parent_title_id : candidate->id_code,
                           base->id_code) == 0;
        if ((app->detail_tab == GAME_INSTALL && search_same_game(base, candidate)) ||
            (app->detail_tab == GAME_DLC && related_dlc)) entries[count++] = candidate;
    }
    SDL_UnlockMutex(app->catalog_mutex);
    return count;
}
static void game_open_screenshot(WolodriveApp *app, int index) {
    ScreenshotSet shots;
    screenshots_get(app,app->detail_item,&shots);
    if (index < 0 || index >= shots.count) return;
    app->detail_selection = index;
    app->screenshot_viewer = true;
}
static void game_activate_entry(WolodriveApp *app, GameItem **entries, int count) {
    if (app->detail_selection < 0 || app->detail_selection >= count) return;
    if (app->detail_tab == GAME_INSTALL) sys_trigger_action(app, entries[app->detail_selection]);
    else if (app->detail_tab == GAME_DLC) {
        GameItem *parent = app->detail_item;
        ui_open_game(app, entries[app->detail_selection]);
        app->detail_parent = parent;
    } else if (app->detail_tab == GAME_SCREENSHOTS) game_open_screenshot(app, app->detail_selection);
}
static void game_select_tab(WolodriveApp *app, int tab) {
    app->screenshot_viewer = false;
    app->detail_tab = tab;
    app->detail_selection = app->detail_scroll = 0;
    app->detail_content_focus = false;
}
static void handle_game_details_input(WolodriveApp *app, const SDL_Event *ev) {
    if (app->ui_theme == UI_THEME_UBIQUITY && !app->detail_ubiquity_options &&
        !app->screenshot_viewer) {
        int command = 0; /* -1 previous, +1 next, 2 activate, 3 back */
        if (ev->type == SDL_MOUSEBUTTONDOWN && ev->button.button == SDL_BUTTON_LEFT) {
            int mx = ev->button.x, my = ev->button.y;
            if (my < 100 && mx < 120) command = 3;
            else if (my >= 902 && my < 974) {
                if (mx >= 169 && mx < 470) { app->detail_selection = 0; command = 2; }
                else if (mx >= 485 && mx < 559) { app->detail_selection = 1; command = 2; }
                else if (mx >= 574 && mx < 648) { app->detail_selection = 2; command = 2; }
            }
        } else if (ev->type == SDL_KEYDOWN) {
            switch (ev->key.keysym.sym) {
                case SDLK_LEFT: case SDLK_a: command = -1; break;
                case SDLK_RIGHT: case SDLK_d: command = 1; break;
                case SDLK_RETURN: case SDLK_SPACE: command = 2; break;
                case SDLK_ESCAPE: case SDLK_BACKSPACE: case SDLK_b: command = 3; break;
                case SDLK_PAGEUP: app->detail_scroll = SDL_max(0, app->detail_scroll - 90); return;
                case SDLK_PAGEDOWN: app->detail_scroll += 90; return;
                case SDLK_i: app->show_original_description = !app->show_original_description; return;
                default: break;
            }
            if (ev->key.repeat && command >= 2) return;
        } else if (ev->type == SDL_CONTROLLERBUTTONDOWN) {
            switch (ev->cbutton.button) {
                case SDL_CONTROLLER_BUTTON_DPAD_LEFT: command = -1; break;
                case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: command = 1; break;
                case SDL_CONTROLLER_BUTTON_A: command = 2; break;
                case SDL_CONTROLLER_BUTTON_B: command = 3; break;
                case SDL_CONTROLLER_BUTTON_RIGHTSTICK:
                    app->show_original_description = !app->show_original_description;
                    return;
                default: break;
            }
        } else if (ev->type == SDL_MOUSEWHEEL) {
            int dir = ev->wheel.direction == SDL_MOUSEWHEEL_FLIPPED ? -1 : 1;
            app->detail_scroll = SDL_max(0, app->detail_scroll - ev->wheel.y * dir * 48);
            return;
        }
        if (command == -1) app->detail_selection = SDL_max(0, app->detail_selection - 1);
        if (command == 1) app->detail_selection = SDL_min(2, app->detail_selection + 1);
        if (command == 3) { game_back(app); return; }
        if (command == 2) {
            if (app->detail_selection == 0) {
                if (app->detail_item->format == FMT_CATALOG) {
                    app->detail_ubiquity_options = true;
                    game_select_tab(app, GAME_DETAILS);
                } else sys_trigger_action(app, app->detail_item);
            } else if (app->detail_selection == 1) {
                app->detail_ubiquity_options = true;
                game_select_tab(app, GAME_INSTALL);
            } else game_back(app);
        }
        return;
    }
    GameItem *entries[MAX_CATALOG_ITEMS];
    ScreenshotSet shots = {0};
    if (app->detail_tab == GAME_SCREENSHOTS) screenshots_get(app,app->detail_item,&shots);
    int count = app->detail_tab == GAME_SCREENSHOTS ? shots.count : game_entries(app, entries);
    app->detail_selection = count ? SDL_min(app->detail_selection, count - 1) : 0;
    if (ev->type == SDL_MOUSEBUTTONDOWN && ev->button.button == SDL_BUTTON_LEFT) {
        if (app->screenshot_viewer) {
            if (contains_point((SDL_Rect){1820,20,60,60},ev->button.x,ev->button.y)) app->screenshot_viewer = false;
            else if (count && ev->button.x < 110) app->detail_selection = (app->detail_selection+count-1)%count;
            else if (count && ev->button.x > 1810) app->detail_selection = (app->detail_selection+1)%count;
            return;
        }
        if (contains_point(GAME_CLOSE, ev->button.x, ev->button.y)) { app->detail_open = false; return; }
        for (int tab = 0; tab < GAME_TAB_COUNT; tab++)
            if (contains_point(game_tab_target(tab), ev->button.x, ev->button.y)) { game_select_tab(app, tab); return; }
        int start = app->detail_selection / 3 * 3;
        for (int i = start; i < count && i < start + 3; i++)
            if (contains_point(game_entry_target(i), ev->button.x, ev->button.y)) {
                app->detail_selection = i;
                app->detail_content_focus = true;
                game_activate_entry(app, entries, count);
                return;
            }
        return;
    }
    int action = 0;
    if (ev->type == SDL_KEYDOWN) {
        switch (ev->key.keysym.sym) {
            case SDLK_LEFT: case SDLK_a: action = 1; break;
            case SDLK_RIGHT: case SDLK_d: action = 2; break;
            case SDLK_UP: case SDLK_w: action = 3; break;
            case SDLK_DOWN: case SDLK_s: action = 4; break;
            case SDLK_RETURN: case SDLK_SPACE: action = 5; break;
            case SDLK_ESCAPE: case SDLK_BACKSPACE: case SDLK_b: action = 6; break;
            case SDLK_PAGEUP: app->detail_scroll = SDL_max(0, app->detail_scroll - 80); return;
            case SDLK_PAGEDOWN: app->detail_scroll += 80; return;
            case SDLK_i: app->show_original_description = !app->show_original_description; return;
            default: break;
        }
        if (ev->key.repeat && (action == 5 || action == 6)) return;
    } else if (ev->type == SDL_CONTROLLERBUTTONDOWN) {
        switch (ev->cbutton.button) {
            case SDL_CONTROLLER_BUTTON_DPAD_LEFT: action = 1; break;
            case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: action = 2; break;
            case SDL_CONTROLLER_BUTTON_DPAD_UP: action = 3; break;
            case SDL_CONTROLLER_BUTTON_DPAD_DOWN: action = 4; break;
            case SDL_CONTROLLER_BUTTON_A: action = 5; break;
            case SDL_CONTROLLER_BUTTON_B: action = 6; break;
            case SDL_CONTROLLER_BUTTON_RIGHTSTICK: app->show_original_description = !app->show_original_description; return;
            default: break;
        }
    } else if (ev->type == SDL_MOUSEWHEEL) {
        int direction = ev->wheel.direction == SDL_MOUSEWHEEL_FLIPPED ? -1 : 1;
        if (app->screenshot_viewer && count && ev->wheel.y)
            app->detail_selection = (app->detail_selection + (ev->wheel.y * direction > 0 ? count-1 : 1)) % count;
        else app->detail_scroll = SDL_max(0, app->detail_scroll - ev->wheel.y * direction * 40);
        return;
    }
    if (app->screenshot_viewer) {
        if (action == 6) app->screenshot_viewer = false;
        else if (count && (action == 1 || action == 2))
            app->detail_selection = (app->detail_selection + (action == 1 ? count-1 : 1)) % count;
        return;
    }
    if (action == 6) { game_back(app); return; }
    if (!app->detail_content_focus) {
        if (action == 1 || action == 2) game_select_tab(app, (app->detail_tab + (action == 1 ? GAME_TAB_COUNT - 1 : 1)) % GAME_TAB_COUNT);
        else if (action == 4 || action == 5) app->detail_content_focus = true;
    } else if (action == 3) app->detail_content_focus = false;
    else if (app->detail_tab == GAME_DETAILS && action == 4) app->detail_scroll += 60;
    else if (action == 1 && app->detail_selection > 0) app->detail_selection--;
    else if (action == 2 && app->detail_selection + 1 < count) app->detail_selection++;
    else if (action == 5) game_activate_entry(app, entries, count);
}
static void render_screenshot_image(WolodriveApp *app, const char *uri, SDL_Rect box, bool selected) {
    int width = 0, height = 0;
    bool failed = false;
    SDL_Texture *texture = artwork_get_media(app,uri,selected ? 110 : 80,&width,&height,&failed);
    fill_rect(app->renderer,box.x,box.y,box.w,box.h,8,8,10,255);
    if (texture && width > 0 && height > 0) {
        SDL_Rect destination = artwork_fit(width,height,box,false);
        SDL_RenderCopy(app->renderer,texture,NULL,&destination);
    } else draw_text(app,app->font_small,TR(app,failed ? TXT_SCREEN_FAILED : TXT_SCREEN_IMAGE_LOADING),
                     box.x+16,box.y+box.h/2-20,(SDL_Color){190,190,195,255},box.w-32);
}
static void render_screenshot_viewer(WolodriveApp *app, const ScreenshotSet *shots) {
    SDL_Renderer *r = app->renderer;
    SDL_Color white = {245,245,245,255};
    fill_rect(r,0,0,SCREEN_WIDTH,SCREEN_HEIGHT,0,0,0,255);
    draw_single_line(app,app->font_small,app->detail_item->title,60,24,white,(SDL_Rect){60,20,1700,45});
    if (shots->count) render_screenshot_image(app,shots->images[app->detail_selection],(SDL_Rect){120,90,1680,850},true);
    draw_text(app,app->font_row_title,"X",1835,22,white,0);
    draw_text(app,app->font_row_title,"<",35,490,white,0);
    draw_text(app,app->font_row_title,">",1835,490,white,0);
    char label[600];
    snprintf(label,sizeof(label),"%d / %d",shots->count ? app->detail_selection+1 : 0,shots->count);
    draw_text(app,app->font_small,label,60,960,white,0);
    snprintf(label,sizeof(label),TR(app,TXT_SCREEN_SOURCE),shots->source);
    draw_text(app,app->font_micro,label,300,960,white,1450);
    draw_text(app,app->font_small,TR(app,TXT_SCREEN_VIEWER_HINT),60,1010,white,1750);
}

#include "ui_ubiquity_details.h"

static void render_game_details(WolodriveApp *app) {
    if (app->ui_theme == UI_THEME_UBIQUITY && !app->detail_ubiquity_options &&
        !app->screenshot_viewer) {
        render_ubiquity_game_details(app);
        return;
    }
    SDL_Renderer *r = app->renderer;
    GameItem *item = app->detail_item;
    SDL_Color white = {245,245,245,255}, muted = {175,175,175,255};
    ScreenshotSet shots = {0};
    if (app->detail_tab == GAME_SCREENSHOTS) screenshots_get(app,item,&shots);
    if (app->screenshot_viewer) {
        app->detail_selection = shots.count ? SDL_min(app->detail_selection,shots.count-1) : 0;
        render_screenshot_viewer(app,&shots);
        return;
    }

    fill_rect(r, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, 0,0,0,220);
    fill_rect(r, RAIL_WIDTH+10, 205, SCREEN_WIDTH-RAIL_WIDTH-20, 735, 11,13,20,255);
    SDL_Rect title_box = {CONTENT_X+20, 240, 1400, 105};
    draw_fitted_text(app, app->font_hero_title, item->title, title_box, white, title_box, false);
    char metadata[256];
    char size_label[64]; game_list_size(item,size_label);
    snprintf(metadata, sizeof(metadata), "%s   |   %s   |   %s   |   %s", item->id_code,
             sys_format_short_badge(item->format), item->version, size_label);
    draw_text(app, app->font_small, metadata, CONTENT_X+20, 365, muted, 1350);
    draw_text(app, app->font_row_title, "X", GAME_CLOSE.x+15, GAME_CLOSE.y+8, white, 0);
    if (app->detail_tab == GAME_DETAILS) {
        char description[MAX_TRANSLATED_DESCRIPTION_BYTES];
        description_text(app, item, description, sizeof(description));
        UiTextEntry *entry = cached_text(app, app->font_body, description, white, 920);
        SDL_Rect clip = {CONTENT_X+20, 430, 920, 320};
        if (entry) {
            app->detail_scroll = SDL_min(app->detail_scroll, SDL_max(0, entry->h - clip.h));
            {
                SDL_Rect dest = {clip.x, clip.y-app->detail_scroll, entry->w, entry->h};
                SDL_RenderSetClipRect(r, &clip);
                SDL_RenderCopy(r, entry->texture, NULL, &dest);
                SDL_RenderSetClipRect(r, NULL);
            }
        }
        snprintf(metadata, sizeof(metadata), "%s\n%s\n%s\n%s", item->source_name, item->id_code, item->version, sys_format_short_badge(item->format));
        draw_text(app, app->font_small, metadata, 1330, 440, muted, 480);
    } else {
        GameItem *entries[MAX_CATALOG_ITEMS];
        int total = app->detail_tab == GAME_SCREENSHOTS ? shots.count : game_entries(app, entries);
        app->detail_selection = total ? SDL_min(app->detail_selection, total-1) : 0;
        int start = app->detail_selection / 3 * 3;
        if (!total) {
            TextId message = app->detail_tab == GAME_DLC ? TXT_GAME_NO_DLC :
                shots.state == SHOTS_PENDING ? TXT_SCREEN_LOOKUP : shots.state == SHOTS_FAILED ? TXT_SCREEN_FAILED : TXT_GAME_NO_SCREENSHOTS;
            draw_text(app,app->font_body,TR(app,message),CONTENT_X+20,460,muted,1350);
        }
        for (int i = start; i < total && i < start+3; i++) {
            SDL_Rect box = game_entry_target(i);
            fill_rect(r, box.x, box.y, box.w, box.h, 24,24,24,245);
            if (app->detail_tab == GAME_SCREENSHOTS) {
                snprintf(metadata, sizeof(metadata), TR(app, TXT_GAME_OPEN_SCREENSHOT), i+1);
                render_screenshot_image(app,shots.images[i],(SDL_Rect){box.x+10,box.y+10,box.w-20,240},i == app->detail_selection);
                draw_text(app, app->font_small, metadata, box.x+18, box.y+270, white, box.w-36);
            } else {
                GameItem *package = entries[i];
                render_cover_or_placeholder(app, package, box.x+(box.w-110)/2, box.y, 110, 155, false);
                snprintf(metadata, sizeof(metadata), "%s  |  %s  |  %s", package->id_code,
                         sys_format_short_badge(package->format), package->version);
                SDL_Rect label = {box.x+14, box.y+165, box.w-28, 48};
                draw_fitted_text(app, app->font_small, app->detail_tab == GAME_DLC ? package->title : package->source_name, label, white, label, false);
                draw_text(app, app->font_micro, metadata, box.x+14, box.y+220, muted, box.w-28);
                draw_text(app, app->font_small, app->detail_tab == GAME_DLC ? TR(app, TXT_GAME_DETAILS) : action_label(app, package), box.x+14, box.y+285, white, box.w-28);
            }
            if (i == app->detail_selection && app->detail_content_focus)
                draw_border(r, box.x-3, box.y-3, box.w+6, box.h+6, 3,245,245,245,255);
        }
        if (total) {
            snprintf(metadata, sizeof(metadata), "%d / %d", app->detail_selection+1, total);
            draw_text(app, app->font_micro, metadata, CONTENT_X+20, 765, muted, 0);
        }
        if (app->detail_tab == GAME_SCREENSHOTS && total) {
            snprintf(metadata,sizeof(metadata),TR(app,TXT_SCREEN_SOURCE),shots.source);
            draw_text(app,app->font_micro,metadata,CONTENT_X+160,765,muted,1300);
            if (shots.page[0]) draw_text(app,app->font_micro,shots.page,CONTENT_X+20,795,muted,1450);
        }
        if (app->detail_tab == GAME_INSTALL) draw_text(app, app->font_micro, TR(app, TXT_ACTION_UNAVAILABLE), CONTENT_X+20, 795, muted, 1490);
    }
    for (int tab = 0; tab < GAME_TAB_COUNT; tab++) {
        SDL_Rect box = game_tab_target(tab);
        draw_fitted_text(app, app->font_small, TR(app, GAME_TABS[tab]), box, tab == app->detail_tab ? white : muted, box, true);
        if (tab == app->detail_tab) {
            fill_rect(r, box.x+20, box.y+box.h-5, box.w-40, 5,229,9,20,255);
            if (!app->detail_content_focus) draw_border(r, box.x, box.y, box.w, box.h, 1,140,140,140,255);
        }
    }
    draw_text(app, app->font_micro, TR(app, TXT_GAME_PANEL_HINT), CONTENT_X+20, 955, white, 1470);
}
