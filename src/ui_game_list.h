/* Text-only catalogs. Coordinates are mirrored by GAME_LIST in layout.ts. */
enum { GAME_LIST_TOP = 220, GAME_LIST_ROW_HEIGHT = 64, GAME_LIST_ROW_GAP = 8,
       GAME_LIST_PAGE_SIZE = 10 };
static bool game_list_view(WolodriveApp *app) {
    return app->active_row == VIEW_HOME || app->active_row == VIEW_PAYLOADS ||
           app->active_row == VIEW_DOWNLOADS ||
           (app->active_row == VIEW_GAMES && app->library_depth == LIBRARY_GAMES);
}
static SDL_Rect game_list_row_target(int index) {
    return (SDL_Rect){CONTENT_X, GAME_LIST_TOP + (index % GAME_LIST_PAGE_SIZE) *
        (GAME_LIST_ROW_HEIGHT + GAME_LIST_ROW_GAP), SCREEN_WIDTH - CONTENT_X - 48,
        GAME_LIST_ROW_HEIGHT};
}
static SDL_Rect game_list_category_target(int index) {
    return (SDL_Rect){CONTENT_X + index * 230, 140, 218, 44};
}
static void game_list_select(WolodriveApp *app, int index) {
    CarouselRow *row = active_home_row(app);
    row->selected_col = SDL_max(0, SDL_min(index, row->item_count - 1));
    if (app->active_row == VIEW_HOME) {
        app->home_categories[app->home_cat_index].selected_col = row->selected_col;
        sync_home_active_row(app);
    }
}
static bool handle_game_list_input(WolodriveApp *app, const SDL_Event *ev) {
    if (!game_list_view(app) || app->sidebar_focus) return false;
    CarouselRow *row = active_home_row(app);
    int action = 0, delta = 0;
    if (ev->type == SDL_KEYDOWN) {
        switch(ev->key.keysym.sym) {
            case SDLK_UP: case SDLK_w: delta = -1; break;
            case SDLK_DOWN: case SDLK_s: delta = 1; break;
            case SDLK_LEFT: case SDLK_a: action = 1; break;
            case SDLK_RIGHT: case SDLK_d: action = 2; break;
            case SDLK_RETURN: case SDLK_SPACE: action = 3; break;
            case SDLK_PAGEUP: delta = -GAME_LIST_PAGE_SIZE; break;
            case SDLK_PAGEDOWN: delta = GAME_LIST_PAGE_SIZE; break;
            case SDLK_ESCAPE: case SDLK_BACKSPACE: case SDLK_b:
                if (app->active_row == VIEW_GAMES) return false;
                action = 4; break;
            case SDLK_g: return true; /* Grid presentation has been removed. */
            default: break;
        }
    } else if (ev->type == SDL_CONTROLLERBUTTONDOWN) {
        switch(ev->cbutton.button) {
            case SDL_CONTROLLER_BUTTON_DPAD_UP: delta = -1; break;
            case SDL_CONTROLLER_BUTTON_DPAD_DOWN: delta = 1; break;
            case SDL_CONTROLLER_BUTTON_DPAD_LEFT: action = 1; break;
            case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: action = 2; break;
            case SDL_CONTROLLER_BUTTON_A: action = 3; break;
            case SDL_CONTROLLER_BUTTON_B:
                if (app->active_row == VIEW_GAMES) return false;
                action = 4; break;
            case SDL_CONTROLLER_BUTTON_X: return true;
            default: break;
        }
    } else if (ev->type == SDL_MOUSEWHEEL) {
        int direction = ev->wheel.direction == SDL_MOUSEWHEEL_FLIPPED ? -1 : 1;
        delta = -ev->wheel.y * direction;
    } else if (ev->type == SDL_MOUSEBUTTONDOWN && ev->button.button == SDL_BUTTON_LEFT) {
        if (app->active_row == VIEW_HOME) {
            for (int i = 0; i < HOME_TAB_COUNT; i++)
                if (contains_point(game_list_category_target(i), ev->button.x, ev->button.y)) {
                    app->home_cat_index = HOME_TABS[i]; sync_home_active_row(app); return true;
                }
        }
        int first = row->selected_col / GAME_LIST_PAGE_SIZE * GAME_LIST_PAGE_SIZE;
        for(int i = first; i < row->item_count && i < first + GAME_LIST_PAGE_SIZE; i++)
            if(contains_point(game_list_row_target(i), ev->button.x, ev->button.y)) {
                game_list_select(app, i); ui_open_game(app, row->items[i]); return true;
            }
        if (contains_point((SDL_Rect){SCREEN_WIDTH-164,980,50,44},ev->button.x,ev->button.y)) delta=-(row->selected_col % GAME_LIST_PAGE_SIZE)-GAME_LIST_PAGE_SIZE;
        if (contains_point((SDL_Rect){SCREEN_WIDTH-98,980,50,44},ev->button.x,ev->button.y)) delta=GAME_LIST_PAGE_SIZE-(row->selected_col % GAME_LIST_PAGE_SIZE);
    }
    if (delta && app->active_row == VIEW_GAMES) {
        /* Preserve access to console catalogs larger than a native row batch. */
        bool previous = delta < 0 && row->selected_col == 0 && app->library_game_offset > 0;
        bool next = delta > 0 && row->selected_col + delta >= row->item_count && row->item_count == MAX_ITEMS_PER_ROW;
        if (previous || next) {
            if (next) row->selected_col = row->item_count - 1;
            SDL_Event page = {0}; page.type = SDL_KEYDOWN;
            page.key.keysym.sym = next ? SDLK_RIGHT : SDLK_LEFT;
            handle_library_input(app, &page);
            return true;
        }
    }
    if (delta) game_list_select(app, row->selected_col + delta);
    if (action == 1 || action == 4) app->sidebar_focus = true;
    if (action == 2 && app->active_row == VIEW_HOME) {
        app->home_cat_index = app->home_cat_index == HOME_TABS[0] ? HOME_TABS[1] : HOME_TABS[0];
        sync_home_active_row(app);
    }
    if (action == 3 && row->item_count) ui_open_game(app, row->items[row->selected_col]);
    if (ev->type == SDL_MOUSEBUTTONDOWN && ev->button.button == SDL_BUTTON_LEFT &&
        ev->button.x >= RAIL_WIDTH && !(app->active_row == VIEW_GAMES && ev->button.y < 80))
        return true; /* Never fall through to hidden legacy cover hit targets. */
    return delta != 0 || action != 0;
}
static void render_game_list(WolodriveApp *app) {
    SDL_Renderer *r = app->renderer;
    SDL_Color white={246,246,249,255}, muted={188,190,200,255};
    CarouselRow *row=active_home_row(app);
    const LibraryConsole *console = app->active_row == VIEW_GAMES ?
        library_console_visible_at(app->library_company,app->library_console_selection,false) : NULL;
    fill_rect(r,RAIL_WIDTH,0,SCREEN_WIDTH-RAIL_WIDTH,SCREEN_HEIGHT,11,13,20,255);
    draw_text(app,app->font_hero_title,console ? console->name : view_content_title(app),CONTENT_X,70,white,1600);
    if(app->active_row==VIEW_HOME) for(int i=0;i<HOME_TAB_COUNT;i++) {
        SDL_Rect box=game_list_category_target(i);
        bool selected=HOME_TABS[i]==app->home_cat_index;
        fill_rounded_rect(r,box.x,box.y,box.w,box.h,8,selected?72:23,selected?26:26,selected?48:36,255);
        draw_rounded_border(r,box.x,box.y,box.w,box.h,8,1,selected?201:56,selected?24:58,selected?74:68,255);
        draw_fitted_text(app,app->font_small,home_category_title(app,HOME_TABS[i]),box,white,box,true);
    }
    render_game_list_headers(app,(SDL_Rect){CONTENT_X,190,SCREEN_WIDTH-CONTENT_X-48,24});
    int first=row->selected_col/GAME_LIST_PAGE_SIZE*GAME_LIST_PAGE_SIZE;
    for(int i=first;i<row->item_count && i<first+GAME_LIST_PAGE_SIZE;i++) {
        GameItem *item=row->items[i]; SDL_Rect box=game_list_row_target(i);
        bool selected=!app->sidebar_focus && i==row->selected_col;
        fill_rounded_rect(r,box.x,box.y,box.w,box.h,8,selected?72:23,selected?26:26,selected?48:36,255);
        draw_rounded_border(r,box.x,box.y,box.w,box.h,8,selected?2:1,selected?201:44,selected?24:46,selected?74:56,255);
        render_game_list_columns(app,item,box);
    }
    if(!row->item_count) draw_text(app,app->font_body,empty_view_message(app),CONTENT_X,GAME_LIST_TOP,muted,1500);
    char position[80]; snprintf(position,sizeof(position),"%d / %d",row->item_count?row->selected_col+1:0,row->item_count);
    draw_text(app,app->font_small,position,CONTENT_X,990,muted,0);
    draw_text(app,app->font_small,TR(app,TXT_GAME_DETAILS),SCREEN_WIDTH-520,990,muted,320);
    draw_text(app,app->font_body,"<",SCREEN_WIDTH-150,990,muted,40);
    draw_text(app,app->font_body,">",SCREEN_WIDTH-84,990,muted,40);
}
