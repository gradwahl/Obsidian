/* Independent source and format checkbox lists. Caller holds catalog_mutex when
   collecting names; include saved sources even if they are currently offline. */
enum { SETTINGS_SOURCE_PAGE = 7 };
static int settings_sources(WolodriveApp *app, const char **names) {
    int count = 0;
    for (int i = 0; i < app->catalog_count + app->filters.source_count; i++) {
        const char *name = i < app->catalog_count ? app->catalog[i].source_name : app->filters.sources[i-app->catalog_count];
        if (!name[0]) continue;
        bool duplicate = false;
        for (int j = 0; j < count; j++) if (SDL_strcasecmp(name, names[j]) == 0) { duplicate = true; break; }
        if (!duplicate) names[count++] = name;
    }
    return count;
}
static SDL_Rect settings_target(int column, int visible_row) {
    return (SDL_Rect){CONTENT_X+30+column*860, column && visible_row == 4 ? 720 : 280+visible_row*70,
                      column ? 590 : 820, 60};
}
static void settings_changed(WolodriveApp *app) {
    ui_rebuild_carousels(app);
    if (!settings_save(app)) sys_send_notification(app, TR(app, TXT_SETTINGS_SAVE_FAILED));
}
static void settings_toggle(WolodriveApp *app) {
    SDL_LockMutex(app->catalog_mutex);
    if (app->settings_column == 1) {
        int row = app->settings_selection;
        if (row == 4) memset(&app->filters, 0, sizeof(app->filters));
        else if (row == 0) app->filters.disabled_formats = app->filters.disabled_formats == 0 ? 7 : 0;
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
            for (int i = 0; i < count; i++) if (SDL_strcasecmp(choice, names[i]) != 0)
                SDL_strlcpy(app->filters.sources[app->filters.source_count++], names[i], 64);
        } else {
            int found = -1;
            for (int i = 0; i < app->filters.source_count; i++)
                if (SDL_strcasecmp(choice, app->filters.sources[i]) == 0) { found = i; break; }
            if (found >= 0) {
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
    app->settings_selection = SDL_min(app->settings_selection, app->settings_column ? 4 : count);
    int start = app->settings_selection > 0 ? (app->settings_selection-1)/SETTINGS_SOURCE_PAGE*SETTINGS_SOURCE_PAGE : 0;
    if (ev->type == SDL_MOUSEBUTTONDOWN && ev->button.button == SDL_BUTTON_LEFT) {
        for (int col = 0; col < 2; col++) {
            int last = col ? 4 : SDL_min(SETTINGS_SOURCE_PAGE, count-start);
            if (app->settings_column) start = 0;
            for (int row = 0; row <= last; row++) if (contains_point(settings_target(col, row), ev->button.x, ev->button.y)) {
                app->sidebar_focus = false;
                app->settings_column = col;
                app->settings_selection = col || row == 0 ? row : start+row;
                settings_toggle(app);
                return true;
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
        app->settings_selection = SDL_max(0, SDL_min(count, app->settings_selection-ev->wheel.y*direction*3));
    }
    if (action == 1 && app->settings_selection > 0) app->settings_selection--;
    else if (action == 2 && app->settings_selection < (app->settings_column ? 4 : count)) app->settings_selection++;
    else if (action == 3) settings_toggle(app);
    else if (action == 4 && app->settings_column == 0) { app->settings_column = 1; app->settings_selection = SDL_min(app->settings_selection, 4); }
    else if (action == 5 && app->settings_column == 1) { app->settings_column = 0; app->settings_selection = SDL_min(app->settings_selection, count); }
    else if (action == 5 || action == 6) app->sidebar_focus = true;
    return true;
}
static void settings_draw_checkbox(WolodriveApp *app, SDL_Rect box, const char *label, bool checked, bool mixed, bool focus) {
    SDL_Renderer *r = app->renderer;
    SDL_Color white = {245,245,245,255};
    fill_rect(r, box.x, box.y, box.w, box.h, 24,24,24,255);
    int x = box.x+16, y = box.y+14;
    draw_border(r, x, y, 32, 32, 2,210,210,210,255);
    SDL_SetRenderDrawColor(r, 245,245,245,255);
    if (checked) {
        SDL_RenderDrawLine(r, x+6, y+16, x+13, y+24);
        SDL_RenderDrawLine(r, x+13, y+24, x+26, y+7);
    } else if (mixed) fill_rect(r, x+7, y+14, 18, 4,245,245,245,255);
    SDL_Rect area = {box.x+65, box.y+10, box.w-85, 42};
    draw_fitted_text(r, app->font_small, label, area, white, area, false);
    if (focus) draw_border(r, box.x, box.y, box.w, box.h, 3,245,245,245,255);
}
static void render_settings(WolodriveApp *app) {
    SDL_Renderer *r = app->renderer;
    SDL_Color white = {245,245,245,255}, muted = {170,170,170,255};
    draw_text(r, app->font_hero_title, TR(app, TXT_NAV_SETTINGS), CONTENT_X+30, 75, white, 0);
    draw_text(r, app->font_body, TR(app, TXT_SETTINGS_HELP), CONTENT_X+30, 175, muted, 1450);
    draw_text(r, app->font_row_title, TR(app, TXT_SETTINGS_SOURCE), CONTENT_X+30, 230, white, 0);
    draw_text(r, app->font_row_title, TR(app, TXT_SETTINGS_FORMAT), CONTENT_X+890, 230, white, 0);
    const char *names[MAX_CATALOG_ITEMS*2];
    SDL_LockMutex(app->catalog_mutex);
    int count = settings_sources(app, names);
    int selected = app->settings_column ? 0 : SDL_min(app->settings_selection, count);
    int start = selected > 0 ? (selected-1)/SETTINGS_SOURCE_PAGE*SETTINGS_SOURCE_PAGE : 0;
    settings_draw_checkbox(app, settings_target(0,0), TR(app, TXT_SETTINGS_ALL_SOURCES), !app->filters.sources_limited,
        app->filters.sources_limited && app->filters.source_count > 0, !app->sidebar_focus && !app->settings_column && selected == 0);
    for (int i = start; i < count && i < start+SETTINGS_SOURCE_PAGE; i++)
        settings_draw_checkbox(app, settings_target(0,i-start+1), names[i], settings_source_enabled(app,names[i]), false,
            !app->sidebar_focus && !app->settings_column && selected == i+1);
    SDL_UnlockMutex(app->catalog_mutex);
    for (int row = 0; row < 4; row++) {
        const char *label = row ? sys_format_to_string(app, (PackageFormat)(row-1)) : TR(app, TXT_SETTINGS_ALL_FORMATS);
        bool checked = row ? !(app->filters.disabled_formats & (1u << (row-1))) : !app->filters.disabled_formats;
        settings_draw_checkbox(app, settings_target(1,row), label, checked,
            row == 0 && app->filters.disabled_formats > 0 && app->filters.disabled_formats < 7,
            !app->sidebar_focus && app->settings_column && app->settings_selection == row);
    }
    SDL_Rect reset = settings_target(1,4);
    fill_rect(r, reset.x, reset.y, reset.w, reset.h, 35,35,35,255);
    draw_fitted_text(r, app->font_small, TR(app, TXT_SETTINGS_RESET), reset, white, reset, true);
    if (!app->sidebar_focus && app->settings_column && app->settings_selection == 4) draw_border(r,reset.x,reset.y,reset.w,reset.h,3,245,245,245,255);
    if (count > SETTINGS_SOURCE_PAGE) {
        char page[64]; snprintf(page,sizeof(page),"%d / %d", start/SETTINGS_SOURCE_PAGE+1, (count+SETTINGS_SOURCE_PAGE-1)/SETTINGS_SOURCE_PAGE);
        draw_text(r, app->font_micro, page, CONTENT_X+30, 860, muted, 0);
    }
    draw_text(r, app->font_small, TR(app, TXT_SETTINGS_HINT), CONTENT_X+30, 930, muted, 1450);
}
