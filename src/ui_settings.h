/* Linux/PS5 SDL2 Settings layout matching the web preview's two-panel design.
   Sources are scrollable and all controls retain keyboard, D-pad and mouse input. */
enum { SETTINGS_SOURCE_ROWS = 11, SETTINGS_FIRST_Y = 340, SETTINGS_STEP = 61,
       SETTINGS_LEFT = 312, SETTINGS_RIGHT = 1172 };

static int settings_sources(WolodriveApp *app, const char **names) {
    int count = 0;
    for (int i = 0; i < app->catalog_count + app->filters.source_count + app->catalog_source_count; i++) {
        const char *name = i < app->catalog_count ? app->catalog[i].source_name :
            i < app->catalog_count + app->filters.source_count ? app->filters.sources[i-app->catalog_count] :
            app->catalog_source_names[i-app->catalog_count-app->filters.source_count];
        if (!name[0]) continue;
        bool duplicate = false;
        for (int j = 0; j < count; j++)
            if (SDL_strcasecmp(name, names[j]) == 0) { duplicate = true; break; }
        if (!duplicate && count < MAX_CATALOG_ITEMS*2) names[count++] = name;
    }
    /* Deterministic alphabetical order, like getDistinctSources in React. */
    for (int i = 1; i < count; i++) {
        const char *current = names[i];
        int j = i;
        while (j > 0 && SDL_strcasecmp(names[j-1], current) > 0) {
            names[j] = names[j-1];
            j--;
        }
        names[j] = current;
    }
    return count;
}

static SDL_Rect settings_target(int column, int row) {
    if (column && row == 7) return (SDL_Rect){SETTINGS_RIGHT+20, 939, 550, 54};
    if (column && row == 5) return (SDL_Rect){SETTINGS_RIGHT+20, 725, 550, 54};
    if (column && row == 6) return (SDL_Rect){SETTINGS_RIGHT+20, 793, 550, 54};
    return (SDL_Rect){(column ? SETTINGS_RIGHT : SETTINGS_LEFT)+20,
                      SETTINGS_FIRST_Y + row*SETTINGS_STEP,
                      column ? 550 : 780, 54};
}

static void settings_keep_visible(WolodriveApp *app, int count) {
    int max_scroll = SDL_max(0, count+1-SETTINGS_SOURCE_ROWS);
    app->settings_scroll = SDL_max(0, SDL_min(app->settings_scroll, max_scroll));
    if (app->settings_column == 0) {
        app->settings_selection = SDL_max(0, SDL_min(app->settings_selection, count));
        if (app->settings_selection < app->settings_scroll)
            app->settings_scroll = app->settings_selection;
        if (app->settings_selection >= app->settings_scroll+SETTINGS_SOURCE_ROWS)
            app->settings_scroll = app->settings_selection-SETTINGS_SOURCE_ROWS+1;
    } else {
        app->settings_selection = SDL_max(0, SDL_min(app->settings_selection, 7));
    }
}

static void settings_changed(WolodriveApp *app) {
    ui_rebuild_carousels(app);
    if (!settings_save(app)) sys_send_notification(app, TR(app, TXT_SETTINGS_SAVE_FAILED));
}

static void settings_toggle(WolodriveApp *app) {
    SDL_LockMutex(app->catalog_mutex);
    if (app->settings_column == 1) {
        int row = app->settings_selection;
        if (row == 7) memset(&app->filters, 0, sizeof(app->filters));
        else if (row == 5 || row == 6) {
            int theme = row == 6 ? UI_THEME_UBIQUITY : UI_THEME_NETFLIX;
            app->ui_theme = theme;
            SDL_UnlockMutex(app->catalog_mutex);
            if (!theme_save(app)) sys_send_notification(app, TR(app, TXT_SETTINGS_SAVE_FAILED));
            return;
        }
        else if (row == 0) app->filters.disabled_formats = app->filters.disabled_formats ? 0 : 15;
        else app->filters.disabled_formats ^= 1u << (row-1);
    } else if (app->settings_selection == 0) {
        app->filters.sources_limited = !app->filters.sources_limited;
        app->filters.source_count = 0;
    } else {
        const char *names[MAX_CATALOG_ITEMS*2];
        int count = settings_sources(app, names);
        int index = app->settings_selection-1;
        if (index >= count) { SDL_UnlockMutex(app->catalog_mutex); return; }
        char choice[64];
        SDL_strlcpy(choice, names[index], sizeof(choice));
        if (!app->filters.sources_limited) {
            app->filters.sources_limited = true;
            app->filters.source_count = 0;
            for (int i = 0; i < count && app->filters.source_count < MAX_CATALOG_ITEMS; i++)
                if (SDL_strcasecmp(choice, names[i]) != 0)
                    SDL_strlcpy(app->filters.sources[app->filters.source_count++], names[i], 64);
        } else {
            int found = -1;
            for (int i = 0; i < app->filters.source_count; i++)
                if (SDL_strcasecmp(choice, app->filters.sources[i]) == 0) { found = i; break; }
            if (found >= 0) {
                if (found + 1 < app->filters.source_count)
                    memmove(app->filters.sources[found], app->filters.sources[found+1],
                            (size_t)(app->filters.source_count-found-1)*64);
                app->filters.source_count--;
            } else if (app->filters.source_count < MAX_CATALOG_ITEMS)
                SDL_strlcpy(app->filters.sources[app->filters.source_count++], choice, 64);
        }
    }
    SDL_UnlockMutex(app->catalog_mutex);
    settings_changed(app);
}

static bool handle_settings_input(WolodriveApp *app, const SDL_Event *ev) {
    if (app->active_row != VIEW_SETTINGS) return false;
    const char *names[MAX_CATALOG_ITEMS*2];
    SDL_LockMutex(app->catalog_mutex);
    int count = settings_sources(app, names);
    SDL_UnlockMutex(app->catalog_mutex);
    settings_keep_visible(app, count);

    if (ev->type == SDL_MOUSEBUTTONDOWN && ev->button.button == SDL_BUTTON_LEFT) {
        for (int col = 0; col < 2; col++) {
            int last = col ? 7 : SETTINGS_SOURCE_ROWS-1;
            for (int row = 0; row <= last; row++) {
                int index = col ? row : app->settings_scroll+row;
                if (!col && index > count) break;
                if (contains_point(settings_target(col,row), ev->button.x, ev->button.y)) {
                    app->sidebar_focus = false;
                    app->settings_column = col;
                    app->settings_selection = index;
                    settings_toggle(app);
                    return true;
                }
            }
        }
    }
    if (app->sidebar_focus) return true;
    int action = 0;
    if (ev->type == SDL_KEYDOWN) {
        switch (ev->key.keysym.sym) {
            case SDLK_UP: case SDLK_w: action = 1; break;
            case SDLK_DOWN: case SDLK_s: action = 2; break;
            case SDLK_RETURN: case SDLK_SPACE: action = 3; break;
            case SDLK_RIGHT: case SDLK_d: action = 4; break;
            case SDLK_LEFT: case SDLK_a: action = 5; break;
            case SDLK_ESCAPE: case SDLK_BACKSPACE: case SDLK_b: action = 6; break;
            default: break;
        }
        if (ev->key.repeat && action == 3) return true;
    } else if (ev->type == SDL_CONTROLLERBUTTONDOWN) {
        switch (ev->cbutton.button) {
            case SDL_CONTROLLER_BUTTON_DPAD_UP: action = 1; break;
            case SDL_CONTROLLER_BUTTON_DPAD_DOWN: action = 2; break;
            case SDL_CONTROLLER_BUTTON_A: action = 3; break;
            case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: action = 4; break;
            case SDL_CONTROLLER_BUTTON_DPAD_LEFT: action = 5; break;
            case SDL_CONTROLLER_BUTTON_B: action = 6; break;
            default: break;
        }
    } else if (ev->type == SDL_MOUSEWHEEL && app->settings_column == 0) {
        int direction = ev->wheel.direction == SDL_MOUSEWHEEL_FLIPPED ? -1 : 1;
        int maximum = SDL_max(0, count+1-SETTINGS_SOURCE_ROWS);
        app->settings_scroll = SDL_max(0, SDL_min(maximum, app->settings_scroll-ev->wheel.y*direction*3));
        app->settings_selection = SDL_max(app->settings_scroll,
            SDL_min(app->settings_selection, app->settings_scroll+SETTINGS_SOURCE_ROWS-1));
        return true;
    }
    if (action == 1 && app->settings_selection > 0) app->settings_selection--;
    else if (action == 2 && app->settings_selection < (app->settings_column ? 7 : count))
        app->settings_selection++;
    else if (action == 3) settings_toggle(app);
    else if (action == 4 && app->settings_column == 0) {
        app->settings_column = 1;
        app->settings_selection = SDL_min(app->settings_selection, 7);
    } else if (action == 5 && app->settings_column == 1) {
        app->settings_column = 0;
        app->settings_selection = SDL_min(app->settings_selection, count);
    } else if (action == 5 || action == 6) app->sidebar_focus = true;
    settings_keep_visible(app, count);
    return true;
}

static void settings_rounded(SDL_Renderer *r, SDL_Rect rect, Uint8 red, Uint8 green, Uint8 blue) {
    fill_rect(r, rect.x+4, rect.y, rect.w-8, rect.h, red, green, blue, 255);
    fill_rect(r, rect.x+2, rect.y+2, rect.w-4, rect.h-4, red, green, blue, 255);
    fill_rect(r, rect.x, rect.y+4, rect.w, rect.h-8, red, green, blue, 255);
}

static void settings_draw_checkbox(WolodriveApp *app, SDL_Rect box, const char *label,
                                   bool checked, bool mixed, bool focus, bool all_row) {
    SDL_Renderer *r = app->renderer;
    settings_rounded(r, box, all_row ? 24 : 22, all_row ? 24 : 22, all_row ? 31 : 27);
    draw_border(r, box.x+1, box.y+1, box.w-2, box.h-2, 1, 40,40,52,255);
    SDL_Rect check = {box.x+14,box.y+16,20,20};
    if (checked) settings_rounded(r, check, 229,9,20);
    else draw_border(r,check.x,check.y,check.w,check.h,2,90,90,103,255);
    if (checked) {
        SDL_SetRenderDrawColor(r,255,255,255,255);
        SDL_RenderDrawLine(r,check.x+4,check.y+10,check.x+8,check.y+14);
        SDL_RenderDrawLine(r,check.x+8,check.y+14,check.x+16,check.y+5);
    } else if (mixed) fill_rect(r,check.x+5,check.y+9,10,2,245,245,245,255);
    SDL_Rect label_bounds = {box.x+58,box.y+7,box.w-74,box.h-14};
    draw_fitted_text(app,all_row ? app->font_small : app->font_body,label,label_bounds,
                     all_row ? (SDL_Color){245,245,245,255} : (SDL_Color){200,200,210,255},
                     label_bounds,false);
    if (focus) draw_border(r,box.x,box.y,box.w,box.h,2,245,245,245,255);
}

static void render_settings(WolodriveApp *app) {
    SDL_Renderer *r = app->renderer;
    SDL_Color white = {245,245,245,255}, muted = {185,185,185,255};
    draw_text(app,app->font_hero_title,TR(app,TXT_NAV_SETTINGS),SETTINGS_LEFT,142,white,0);
    draw_text(app,app->font_body,TR(app,TXT_SETTINGS_HELP),SETTINGS_LEFT,218,muted,1470);
    fill_rect(r,SETTINGS_LEFT,270,820,734,17,17,19,255);
    draw_border(r,SETTINGS_LEFT,270,820,734,1,40,40,40,255);
    fill_rect(r,SETTINGS_RIGHT,270,590,734,17,17,19,255);
    draw_border(r,SETTINGS_RIGHT,270,590,734,1,40,40,40,255);
    draw_text(app,app->font_row_title,TR(app,TXT_SETTINGS_SOURCE),SETTINGS_LEFT+20,294,white,0);
    draw_text(app,app->font_row_title,TR(app,TXT_SETTINGS_FORMAT),SETTINGS_RIGHT+20,294,white,0);

    const char *names[MAX_CATALOG_ITEMS*2];
    SDL_LockMutex(app->catalog_mutex);
    int count = settings_sources(app,names);
    settings_keep_visible(app,count);
    SDL_Rect clip = {SETTINGS_LEFT+16, SETTINGS_FIRST_Y, 788, 664};
    SDL_RenderSetClipRect(r,&clip);
    for (int row = 0; row < SETTINGS_SOURCE_ROWS; row++) {
        int index = app->settings_scroll + row;
        if (index > count) break;
        bool all = index == 0;
        const char *name = all ? TR(app,TXT_SETTINGS_ALL_SOURCES) : names[index-1];
        settings_draw_checkbox(app,settings_target(0,row),name,
                               all ? !app->filters.sources_limited : settings_source_enabled(app,name),
                               all && app->filters.sources_limited && app->filters.source_count > 0,
                               !app->sidebar_focus && app->settings_column == 0 && app->settings_selection == index,all);
    }
    SDL_RenderSetClipRect(r,NULL);
    SDL_UnlockMutex(app->catalog_mutex);

    for (int row = 0; row < 5; row++) {
        const char *name = row ? sys_format_to_string(app,(PackageFormat)(row-1)) :
                                 TR(app,TXT_SETTINGS_ALL_FORMATS);
        bool checked = row ? !(app->filters.disabled_formats & (1u << (row-1))) : !app->filters.disabled_formats;
        settings_draw_checkbox(app,settings_target(1,row),name,checked,
                               row == 0 && app->filters.disabled_formats > 0 && app->filters.disabled_formats < 15,
                               !app->sidebar_focus && app->settings_column == 1 && app->settings_selection == row,
                               row == 0);
    }
    draw_text(app, app->font_row_title, "Appearance / Wygląd", SETTINGS_RIGHT+20, 676, white, 0);
    for (int theme = 0; theme < 2; theme++) {
        int row = 5 + theme;
        SDL_Rect rect = settings_target(1, row);
        bool active = app->ui_theme == theme;
        fill_rounded_rect(r, rect.x, rect.y, rect.w, rect.h, 12,
                          active ? 49 : 27, active ? 22 : 27, active ? 33 : 33, 255);
        draw_rounded_border(r, rect.x, rect.y, rect.w, rect.h, 12, 1,
                            active ? 229 : 72, active ? 9 : 72, active ? 20 : 82, 255);
        const char *label = theme == 0 ? "Netflix / Glass" : "(u)biquity";
        draw_fitted_text(app, app->font_small, label,
                         (SDL_Rect){rect.x+18, rect.y+5, rect.w-75, rect.h-10},
                         white, rect, false);
        draw_text(app, app->font_small, active ? "●" : "○",
                  rect.x + rect.w - 46, rect.y+12, white, 0);
        if (!app->sidebar_focus && app->settings_column == 1 && app->settings_selection == row)
            draw_rounded_border(r, rect.x, rect.y, rect.w, rect.h, 12, 2,
                                245, 245, 245, 255);
    }
    SDL_Rect reset = settings_target(1,7);
    settings_rounded(r,reset,36,36,46);
    draw_border(r,reset.x,reset.y,reset.w,reset.h,1,52,52,68,255);
    draw_fitted_text(app,app->font_small,TR(app,TXT_SETTINGS_RESET),reset,white,reset,true);
    if (!app->sidebar_focus && app->settings_column == 1 && app->settings_selection == 7)
        draw_border(r,reset.x,reset.y,reset.w,reset.h,2,245,245,245,255);
}
