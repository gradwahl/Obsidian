/* Settings shares the catalog's source labels and format enum. */
static SDL_Rect settings_target(int row) {
    return (SDL_Rect){CONTENT_X+30, 280+row*150, 1460, 110};
}
static void settings_changed(WolodriveApp *app) {
    ui_rebuild_carousels(app);
    if (!settings_save(app)) sys_send_notification(app, TR(app, TXT_SETTINGS_SAVE_FAILED));
}
static void settings_cycle_source(WolodriveApp *app, int direction) {
    const char *sources[MAX_CATALOG_ITEMS+2];
    int count = 1, selected = 0;
    sources[0] = "";
    SDL_LockMutex(app->catalog_mutex);
    for (int i = 0; i < app->catalog_count; i++) {
        const char *name = app->catalog[i].source_name;
        if (!name[0]) continue;
        bool duplicate = false;
        for (int j = 1; j < count; j++) if (SDL_strcasecmp(name, sources[j]) == 0) { duplicate = true; break; }
        if (!duplicate) sources[count++] = name;
    }
    bool found = !app->filter_source[0];
    for (int i = 1; i < count; i++) if (SDL_strcasecmp(app->filter_source, sources[i]) == 0) { selected = i; found = true; break; }
    if (!found) { selected = count; sources[count++] = app->filter_source; }
    int next = (selected + direction + count) % count;
    char choice[64];
    SDL_strlcpy(choice, sources[next], sizeof(choice));
    SDL_strlcpy(app->filter_source, choice, sizeof(app->filter_source));
    SDL_UnlockMutex(app->catalog_mutex);
    settings_changed(app);
}
static void settings_adjust(WolodriveApp *app, int direction) {
    if (app->settings_selection == 0) { settings_cycle_source(app, direction); return; }
    SDL_LockMutex(app->catalog_mutex);
    if (app->settings_selection == 1) app->filter_format = (app->filter_format + direction + 4) % 4;
    else { app->filter_source[0] = '\0'; app->filter_format = 0; }
    SDL_UnlockMutex(app->catalog_mutex);
    settings_changed(app);
}
static bool handle_settings_input(WolodriveApp *app, const SDL_Event *ev) {
    if (app->active_row != VIEW_SETTINGS) return false;
    if (ev->type == SDL_MOUSEBUTTONDOWN && ev->button.button == SDL_BUTTON_LEFT) {
        for (int row = 0; row < 3; row++) if (contains_point(settings_target(row), ev->button.x, ev->button.y)) {
            app->sidebar_focus = false;
            app->settings_selection = row;
            settings_adjust(app, ev->button.x < CONTENT_X+580 ? -1 : 1);
            return true;
        }
    }
    if (app->sidebar_focus) return true;
    int action = 0;
    if (ev->type == SDL_KEYDOWN) {
        switch (ev->key.keysym.sym) {
            case SDLK_UP: case SDLK_w: action = 1; break;
            case SDLK_DOWN: case SDLK_s: action = 2; break;
            case SDLK_RETURN: case SDLK_SPACE: action = 3; break;
            case SDLK_RIGHT: case SDLK_d: action = 3; break;
            case SDLK_LEFT: case SDLK_a: action = 4; break;
            case SDLK_ESCAPE: case SDLK_BACKSPACE: case SDLK_b: action = 5; break;
            default: break;
        }
        if (ev->key.repeat && action == 3) return true;
    } else if (ev->type == SDL_CONTROLLERBUTTONDOWN) {
        switch (ev->cbutton.button) {
            case SDL_CONTROLLER_BUTTON_DPAD_UP: action = 1; break;
            case SDL_CONTROLLER_BUTTON_DPAD_DOWN: action = 2; break;
            case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: case SDL_CONTROLLER_BUTTON_A: action = 3; break;
            case SDL_CONTROLLER_BUTTON_DPAD_LEFT: action = 4; break;
            case SDL_CONTROLLER_BUTTON_B: action = 5; break;
            default: break;
        }
    }
    if (action == 1 && app->settings_selection > 0) app->settings_selection--;
    else if (action == 2 && app->settings_selection < 2) app->settings_selection++;
    else if (action == 3) settings_adjust(app, 1);
    else if (action == 4 || action == 5) app->sidebar_focus = true;
    return true;
}
static void render_settings(WolodriveApp *app) {
    SDL_Renderer *r = app->renderer;
    SDL_Color white = {245,245,245,255}, muted = {170,170,170,255};
    draw_text(r, app->font_hero_title, TR(app, TXT_NAV_SETTINGS), CONTENT_X+30, 95, white, 0);
    draw_text(r, app->font_body, TR(app, TXT_SETTINGS_HELP), CONTENT_X+30, 200, muted, 1450);
    const TextId labels[] = {TXT_SETTINGS_SOURCE, TXT_SETTINGS_FORMAT, TXT_SETTINGS_RESET};
    for (int row = 0; row < 3; row++) {
        SDL_Rect box = settings_target(row);
        bool focus = !app->sidebar_focus && app->settings_selection == row;
        fill_rect(r, box.x, box.y, box.w, box.h, 24,24,24,255);
        SDL_Rect label = {box.x+24, box.y+28, row == 2 ? 1380 : 430, 55};
        draw_fitted_text(r, app->font_small, TR(app, labels[row]), label, white, label, false);
        if (row < 2) {
            const char *value = row == 0 ? (app->filter_source[0] ? app->filter_source : TR(app, TXT_SETTINGS_ALL_SOURCES)) :
                app->filter_format ? sys_format_to_string(app, (PackageFormat)(app->filter_format-1)) : TR(app, TXT_SETTINGS_ALL_FORMATS);
            SDL_Rect area = {box.x+510, box.y+28, 840, 55};
            draw_fitted_text(r, app->font_small, value, area, white, area, true);
            draw_text(r, app->font_small, "<", box.x+475, box.y+36, muted, 0);
            draw_text(r, app->font_small, ">", box.x+1400, box.y+36, muted, 0);
        }
        if (focus) draw_border(r, box.x, box.y, box.w, box.h, 3,245,245,245,255);
    }
    draw_text(r, app->font_small, TR(app, TXT_SETTINGS_HINT), CONTENT_X+30, 800, muted, 1450);
}
