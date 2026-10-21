/* Search owns its query, full-catalog result set and keyboard/grid focus.
   Included by ui_netflix.c to share its rendering and hit-test primitives. */
enum { SEARCH_COLUMNS = 1, SEARCH_PAGE = 9, SEARCH_KEYS = 38 };
static const char SEARCH_LETTERS[] = "abcdefghijklmnopqrstuvwxyz1234567890";

/* Source typography and package tags are not separate game identities. Keep
   edition/DLC words and the console family so meaningful differences survive. */
static void search_title_key(const char *title, char key[128]) {
    while (*title == ' ') title++;
    const char *tags[] = {"[exfat]", "[ffpkg]", "[pkg]"};
    for (int i = 0; i < 3; i++) {
        size_t n = strlen(tags[i]);
        if (SDL_strncasecmp(title, tags[i], n) == 0) { title += n; break; }
    }
    size_t length = 0;
    while (*title && length < 127) {
        const unsigned char *s = (const unsigned char *)title;
        if (strncmp(title, "®", strlen("®")) == 0) { title += strlen("®"); continue; }
        if (strncmp(title, "™", strlen("™")) == 0) { title += strlen("™"); continue; }
        if (strncmp(title, "’", strlen("’")) == 0) { title += strlen("’"); continue; }
        if (strncmp(title, "—", strlen("—")) == 0) { title += strlen("—"); continue; }
        if (*s >= 128 || SDL_isalnum(*s)) key[length++] = (char)(*s < 128 ? SDL_tolower(*s) : *s);
        title++;
    }
    key[length] = '\0';
}

static int search_platform(const GameItem *item) {
    if (SDL_strncasecmp(item->id_code, "CUSA", 4) == 0) return 4;
    if (SDL_strncasecmp(item->id_code, "PPSA", 4) == 0) return 5;
    return 0;
}

static bool search_same_game(const GameItem *a, const GameItem *b) {
    char first[128], second[128];
    if (a->is_game != b->is_game || a->is_dlc != b->is_dlc) return false;
    if (a->platform[0] || b->platform[0]) {
        if (strcmp(a->platform, b->platform) != 0) return false;
    } else if (search_platform(a) != search_platform(b)) return false;
    search_title_key(a->title, first);
    search_title_key(b->title, second);
    return first[0] && strcmp(first, second) == 0;
}

static uint64_t search_title_hash(const GameItem *item) {
    char key[128];
    search_title_key(item->title, key);
    uint64_t hash = 14695981039346656037ULL;
    for (const unsigned char *s = (const unsigned char *)key; *s; s++) {
        hash ^= *s;
        hash *= 1099511628211ULL;
    }
    if (item->platform[0]) {
        for (const unsigned char *s = (const unsigned char *)item->platform; *s; s++) {
            hash ^= *s;
            hash *= 1099511628211ULL;
        }
    } else {
        hash ^= (unsigned)search_platform(item);
        hash *= 1099511628211ULL;
    }
    hash ^= (unsigned)item->is_game | ((unsigned)item->is_dlc << 1);
    return hash;
}

static bool search_contains(const char *text, const char *query) {
    size_t length = strlen(query);
    if (!length) return true;
    for (; *text; text++) if (SDL_strncasecmp(text, query, length) == 0) return true;
    return false;
}

static void search_rebuild_locked(WolodriveApp *app) {
    GameItem *previous = app->search_count ? app->search_results[app->search_selected] : NULL;
    /* Open-addressing prevents quadratic grouping when a full 20k-title
       console catalogue is loaded. Fall back to linear comparisons if OOM. */
    enum { SEARCH_HASH_SLOTS = 65536 };
    uint32_t *slots = calloc(SEARCH_HASH_SLOTS, sizeof(*slots));
    uint64_t hashes[MAX_CATALOG_ITEMS];
    app->search_count = 0;
    for (int i = 0; i < app->catalog_count; i++) {
        GameItem *item = &app->catalog[i];
        if (!settings_accepts_item(app, item)) continue;
        if (!search_contains(item->title, app->search_query) && !search_contains(item->id_code, app->search_query)) continue;
        bool duplicate = false;
        uint64_t hash = search_title_hash(item);
        uint32_t slot = (uint32_t)hash & (SEARCH_HASH_SLOTS - 1);
        if (slots) {
            while (slots[slot]) {
                int existing = (int)slots[slot] - 1;
                if (hashes[existing] == hash && search_same_game(app->search_results[existing], item)) {
                    duplicate = true;
                    break;
                }
                slot = (slot + 1) & (SEARCH_HASH_SLOTS - 1);
            }
        } else {
            for (int j = 0; j < app->search_count; j++)
                if (hashes[j] == hash && search_same_game(app->search_results[j], item)) {
                    duplicate = true;
                    break;
                }
        }
        if (!duplicate) {
            hashes[app->search_count] = hash;
            app->search_results[app->search_count++] = item;
            if (slots) slots[slot] = (uint32_t)app->search_count;
        }
    }
    free(slots);
    app->search_selected = 0;
    for (int i = 0; i < app->search_count; i++)
        if (previous && settings_accepts_item(app, previous) && search_same_game(app->search_results[i], previous)) {
            app->search_selected = i;
            app->search_results[i] = previous;
        }
    if (!app->search_count) {
        app->search_results_focus = false;
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
    return (SDL_Rect){640, 210 + slot * 72, 1100, 64};
}

static SDL_Rect search_page_button(bool next) {
    return (SDL_Rect){next ? 1624 : 1560, 931, 48, 48};
}

static void search_change_page(WolodriveApp *app, int direction) {
    int pages=(app->search_count+SEARCH_PAGE-1)/SEARCH_PAGE;
    if (pages<2) return;
    int page=app->search_selected/SEARCH_PAGE;
    int new_page=SDL_max(0,SDL_min(pages-1,page+direction));
    app->search_selected=new_page*SEARCH_PAGE;
    app->search_results_focus=true;
}

static void search_type_key(WolodriveApp *app, int key) {
    char text[] = {key < 36 ? SEARCH_LETTERS[key] : ' ', '\0'};
    search_edit(app, text, key == 37);
}

static bool handle_search_input(WolodriveApp *app, const SDL_Event *ev) {
    if (app->active_row != VIEW_SEARCH) return false;
    if (ev->type == SDL_TEXTINPUT && !app->sidebar_focus) {
        search_edit(app, ev->text.text, false);
        return true;
    }
    if (ev->type == SDL_MOUSEBUTTONDOWN && ev->button.button == SDL_BUTTON_LEFT) {
        if (app->search_count > SEARCH_PAGE) {
            if (contains_point(search_page_button(false),ev->button.x,ev->button.y)) {
                search_change_page(app,-1); return true;
            }
            if (contains_point(search_page_button(true),ev->button.x,ev->button.y)) {
                search_change_page(app,1); return true;
            }
        }
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
                ui_open_game(app, get_focused_item(app));
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
            case SDLK_BACKSPACE: search_edit(app, "", true); return true;
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
                search_edit(app, "", true);
                return true;
            default: break;
        }
    }
    if (action == 6) { app->sidebar_focus = true; return true; }
    if (app->search_results_focus) {
        int *selected = &app->search_selected;
        if (action == 1) app->search_results_focus = false;
        else if (action == 3 && *selected > 0) (*selected)--;
        else if (action == 4 && *selected + 1 < app->search_count) (*selected)++;
        else if (action == 5 && app->search_count) ui_open_game(app, get_focused_item(app));
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
    draw_text(app, app->font_row_title, TR(app, TXT_NAV_SEARCH), CONTENT_X, 70, white, 0);
    SDL_Rect query = {CONTENT_X, 130, 1540, 52};
    fill_rect(r, query.x, query.y, query.w, query.h, 24,24,24,255);
    draw_single_line(app, app->font_body, app->search_query[0] ? app->search_query : TR(app, TXT_SEARCH_PROMPT),
                     query.x + 12, query.y + 8, white, query);
    for (int k = 0; k < SEARCH_KEYS; k++) {
        SDL_Rect box = search_key_target(k);
        bool focus = !app->sidebar_focus && !app->search_results_focus && k == app->search_key;
        fill_rect(r, box.x, box.y, box.w, box.h, focus ? 240 : 30, focus ? 240 : 30, focus ? 240 : 30, 255);
        char letter[] = {k < 36 ? SEARCH_LETTERS[k] : ' ', '\0'};
        const char *label = k == 36 ? TR(app, TXT_SEARCH_SPACE) : k == 37 ? TR(app, TXT_SEARCH_DELETE) : letter;
        SDL_Color color = focus ? (SDL_Color){10,10,10,255} : white;
        draw_fitted_text(app, app->font_body, label, box, color, box, true);
    }
    char count[80];
    snprintf(count, sizeof(count), TR(app, TXT_SEARCH_COUNT), app->search_count);
    draw_text(app, app->font_small, count, 1560, 140, muted, 0);
    draw_text(app, app->font_micro, TR(app, TXT_SEARCH_HINT), CONTENT_X, 750, muted, 455);
    render_game_list_headers(app,(SDL_Rect){640,184,1100,24});
    int start = app->search_selected / SEARCH_PAGE * SEARCH_PAGE;
    for (int i = start; i < app->search_count && i < start + SEARCH_PAGE; i++) {
        SDL_Rect box = search_result_target(i);
        GameItem *item = app->search_results[i];
        bool selected = !app->sidebar_focus && app->search_results_focus && i == app->search_selected;
        fill_rounded_rect(r,box.x,box.y,box.w,box.h,8,selected?72:23,26,selected?48:36,255);
        render_game_list_columns(app,item,box);
        if(selected) draw_rounded_border(r,box.x,box.y,box.w,box.h,8,2,201,24,74,255);
    }
    if (!app->search_count) draw_text(app, app->font_body,
        TR(app, settings_filters_active(app) ? TXT_SETTINGS_NO_MATCHES : TXT_SEARCH_EMPTY),
        640, 230, muted, 950);
    if (app->search_count) {
        if (app->search_count > SEARCH_PAGE) {
            int page=app->search_selected/SEARCH_PAGE;
            int pages=(app->search_count+SEARCH_PAGE-1)/SEARCH_PAGE;
            snprintf(count,sizeof(count),"Page %d of %d",page+1,pages);
            draw_text(app,app->font_micro,count,640,945,muted,0);
            SDL_Rect prev=search_page_button(false),next=search_page_button(true);
            fill_rect(r,prev.x,prev.y,prev.w,prev.h,30,30,38,255);
            fill_rect(r,next.x,next.y,next.w,next.h,30,30,38,255);
            draw_fitted_text(app,app->font_row_title,"<",prev,
                             page ? white : muted,prev,true);
            draw_fitted_text(app,app->font_row_title,">",next,
                             page+1<pages ? white : muted,next,true);
        } else {
            snprintf(count,sizeof(count),"%d / %d",app->search_selected+1,app->search_count);
            draw_text(app,app->font_micro,count,640,945,muted,0);
        }
    }
}
