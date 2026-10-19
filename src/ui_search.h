/* Search owns its query, full-catalog result set and keyboard/grid focus.
   Included by ui_netflix.c to share its rendering and hit-test primitives. */
enum { SEARCH_COLUMNS = 3, SEARCH_PAGE = 9, SEARCH_KEYS = 38 };
static const char SEARCH_LETTERS[] = "abcdefghijklmnopqrstuvwxyz1234567890";

static bool search_contains(const char *text, const char *query) {
    size_t length = strlen(query);
    if (!length) return true;
    for (; *text; text++) if (SDL_strncasecmp(text, query, length) == 0) return true;
    return false;
}

static void search_rebuild_locked(WolodriveApp *app) {
    GameItem *previous = app->search_count ? app->search_results[app->search_selected] : NULL;
    app->search_count = 0;
    for (int i = 0; i < app->catalog_count; i++) {
        GameItem *item = &app->catalog[i];
        if (search_contains(item->title, app->search_query) || search_contains(item->id_code, app->search_query))
            app->search_results[app->search_count++] = item;
    }
    app->search_selected = 0;
    for (int i = 0; i < app->search_count; i++)
        if (app->search_results[i] == previous) app->search_selected = i;
    if (!app->search_count) {
        app->search_results_focus = false;
        app->search_detail = false;
    }
}

static void search_edit(WolodriveApp *app, const char *text, bool erase) {
    size_t length = strlen(app->search_query);
    if (erase && length) {
        do { length--; } while (length && (app->search_query[length] & 0xc0) == 0x80);
        app->search_query[length] = '\0';
    } else if (!erase) {
        SDL_utf8strlcpy(app->search_query + length, text, sizeof(app->search_query) - length);
    }
    app->search_selected = 0;
    app->search_count = 0;
    SDL_LockMutex(app->catalog_mutex);
    search_rebuild_locked(app);
    SDL_UnlockMutex(app->catalog_mutex);
}

static SDL_Rect search_key_target(int key) {
    if (key >= 36) return (SDL_Rect){CONTENT_X + (key - 36) * 228, 210, 222, 62};
    return (SDL_Rect){CONTENT_X + key % 6 * 76, 280 + key / 6 * 70, 70, 64};
}

static SDL_Rect search_result_target(int index) {
    int slot = index % SEARCH_PAGE;
    return (SDL_Rect){820 + slot % 3 * 350, 210 + slot / 3 * 238, 330, 220};
}

static void search_type_key(WolodriveApp *app, int key) {
    char text[] = {key < 36 ? SEARCH_LETTERS[key] : ' ', '\0'};
    search_edit(app, text, key == 37);
}

static bool handle_search_input(WolodriveApp *app, const SDL_Event *ev) {
    if (app->active_row != VIEW_SEARCH) return false;
    if (ev->type == SDL_TEXTINPUT && !app->sidebar_focus && !app->search_detail) {
        search_edit(app, ev->text.text, false);
        return true;
    }
    if (app->search_detail && ev->type == SDL_MOUSEBUTTONDOWN && ev->button.button == SDL_BUTTON_LEFT) {
        if (contains_point((SDL_Rect){CONTENT_X, 24, 180, 50}, ev->button.x, ev->button.y)) {
            app->search_detail = false;
            return true;
        } else if (contains_point((SDL_Rect){CONTENT_X, 915, 1100, 65}, ev->button.x, ev->button.y)) {
            sys_trigger_action(app, get_focused_item(app));
            return true;
        }
        return false;
    }
    if (ev->type == SDL_MOUSEBUTTONDOWN && ev->button.button == SDL_BUTTON_LEFT && !app->search_detail) {
        for (int k = 0; k < SEARCH_KEYS; k++) if (contains_point(search_key_target(k), ev->button.x, ev->button.y)) {
            app->sidebar_focus = app->search_results_focus = false;
            app->search_key = k;
            search_type_key(app, k);
            return true;
        }
        int start = app->search_selected / SEARCH_PAGE * SEARCH_PAGE;
        for (int i = start; i < app->search_count && i < start + SEARCH_PAGE; i++)
            if (contains_point(search_result_target(i), ev->button.x, ev->button.y)) {
                app->sidebar_focus = false;
                app->search_results_focus = true;
                app->search_selected = i;
                app->search_detail = true;
                return true;
            }
    }
    if (app->sidebar_focus) return true;
    int action = 0;
    if (ev->type == SDL_KEYDOWN) {
        switch (ev->key.keysym.sym) {
            case SDLK_LEFT: action = 1; break;
            case SDLK_RIGHT: action = 2; break;
            case SDLK_UP: action = 3; break;
            case SDLK_DOWN: action = 4; break;
            case SDLK_RETURN: action = 5; break;
            case SDLK_ESCAPE: action = 6; break;
            case SDLK_BACKSPACE:
                if (app->search_detail) app->search_detail = false;
                else search_edit(app, "", true);
                return true;
            case SDLK_F5: catalog_start_async_sync(app); return true;
            default: break;
        }
        if (ev->key.repeat && action == 5) return true;
    } else if (ev->type == SDL_CONTROLLERBUTTONDOWN) {
        switch (ev->cbutton.button) {
            case SDL_CONTROLLER_BUTTON_DPAD_LEFT: action = 1; break;
            case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: action = 2; break;
            case SDL_CONTROLLER_BUTTON_DPAD_UP: action = 3; break;
            case SDL_CONTROLLER_BUTTON_DPAD_DOWN: action = 4; break;
            case SDL_CONTROLLER_BUTTON_A: action = 5; break;
            case SDL_CONTROLLER_BUTTON_B: action = 6; break;
            case SDL_CONTROLLER_BUTTON_Y:
                if (!app->search_detail) search_edit(app, "", true);
                return true;
            default: break;
        }
    }
    if (app->search_detail) {
        if (action == 6) app->search_detail = false;
        else if (action == 5) sys_trigger_action(app, get_focused_item(app));
        return action != 0; /* Reuse description scrolling and Original controls. */
    }
    if (action == 6) { app->sidebar_focus = true; return true; }
    if (app->search_results_focus) {
        int *selected = &app->search_selected;
        if (action == 1) {
            if (*selected % 3) (*selected)--;
            else app->search_results_focus = false;
        } else if (action == 2 && *selected % 3 < 2 && *selected + 1 < app->search_count) (*selected)++;
        else if (action == 3 && *selected >= 3) *selected -= 3;
        else if (action == 4 && (*selected / 3 + 1) * 3 < app->search_count)
            *selected = SDL_min(*selected + 3, app->search_count - 1);
        else if (action == 5 && app->search_count) app->search_detail = true;
    } else {
        int *key = &app->search_key;
        if (action == 1) {
            if (*key == 36 || (*key < 36 && *key % 6 == 0)) app->sidebar_focus = true;
            else (*key)--;
        } else if (action == 2) {
            if (*key == 37 || (*key < 36 && *key % 6 == 5)) app->search_results_focus = app->search_count > 0;
            else (*key)++;
        } else if (action == 3) {
            if (*key < 6) *key = 36 + (*key >= 3);
            else if (*key < 36) *key -= 6;
        } else if (action == 4) {
            if (*key >= 36) *key = (*key - 36) * 3;
            else if (*key < 30) *key += 6;
        } else if (action == 5) search_type_key(app, *key);
    }
    return true;
}

static void render_search(WolodriveApp *app) {
    SDL_Renderer *r = app->renderer;
    SDL_Color white = {245,245,245,255}, muted = {170,170,170,255};
    if (app->search_detail) {
        GameItem *item = get_focused_item(app);
        if (item) render_cover_or_placeholder(app, item, CONTENT_X, 600, 530, 300, false);
        draw_text(r, app->font_small, TR(app, TXT_SEARCH_DETAIL_HINT), CONTENT_X, 930, white, 0);
        draw_text(r, app->font_small, TR(app, TXT_LIBRARY_BACK), CONTENT_X, 28, white, 0);
        return;
    }
    draw_text(r, app->font_row_title, TR(app, TXT_NAV_SEARCH), CONTENT_X, 70, white, 0);
    SDL_Rect query = {CONTENT_X, 130, 1540, 52};
    fill_rect(r, query.x, query.y, query.w, query.h, 24,24,24,255);
    draw_single_line(r, app->font_body, app->search_query[0] ? app->search_query : TR(app, TXT_SEARCH_PROMPT),
                     query.x + 12, query.y + 8, white, query);
    for (int k = 0; k < SEARCH_KEYS; k++) {
        SDL_Rect box = search_key_target(k);
        bool focus = !app->sidebar_focus && !app->search_results_focus && k == app->search_key;
        fill_rect(r, box.x, box.y, box.w, box.h, focus ? 240 : 30, focus ? 240 : 30, focus ? 240 : 30, 255);
        char letter[] = {k < 36 ? SEARCH_LETTERS[k] : ' ', '\0'};
        const char *label = k == 36 ? TR(app, TXT_SEARCH_SPACE) : k == 37 ? TR(app, TXT_SEARCH_DELETE) : letter;
        SDL_Color color = focus ? (SDL_Color){10,10,10,255} : white;
        draw_fitted_text(r, app->font_body, label, box, color, box, true);
    }
    char count[80];
    snprintf(count, sizeof(count), TR(app, TXT_SEARCH_COUNT), app->search_count);
    draw_text(r, app->font_small, count, CONTENT_X, 740, muted, 0);
    draw_text(r, app->font_micro, TR(app, TXT_SEARCH_HINT), CONTENT_X, 800, muted, 455);
    int start = app->search_selected / SEARCH_PAGE * SEARCH_PAGE;
    for (int i = start; i < app->search_count && i < start + SEARCH_PAGE; i++) {
        SDL_Rect box = search_result_target(i);
        GameItem *item = app->search_results[i];
        render_cover_or_placeholder(app, item, box.x, box.y, box.w, 186, false);
        SDL_Rect label = {box.x, box.y + 188, box.w, 30};
        draw_single_line(r, app->font_micro, item->title, label.x, label.y, white, label);
        if (!app->sidebar_focus && app->search_results_focus && i == app->search_selected)
            draw_border(r, box.x - 3, box.y - 3, box.w + 6, box.h + 6, 3,245,245,245,255);
    }
    if (!app->search_count) draw_text(r, app->font_body, TR(app, TXT_SEARCH_EMPTY), 820, 230, muted, 950);
    if (app->search_count) {
        snprintf(count, sizeof(count), "%d / %d", app->search_selected + 1, app->search_count);
        draw_text(r, app->font_micro, count, 820, 950, muted, 0);
    }
}
