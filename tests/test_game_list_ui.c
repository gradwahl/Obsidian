/* Offline list rendering, hit tests and bounded page navigation. */
#include <assert.h>
#include "../src/ui_netflix.c"
#include "../src/catalog_pegasus.c"

int main(void) {
    assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
    assert(TTF_Init() == 0);
    WolodriveApp *app = calloc(1, sizeof(*app));
    assert(app);
    app->catalog_mutex = SDL_CreateMutex();
    app->window = SDL_CreateWindow("Game list test", 0, 0, 1920, 1080, SDL_WINDOW_HIDDEN);
    app->renderer = SDL_CreateRenderer(app->window, -1, SDL_RENDERER_SOFTWARE);
    assert(app->renderer && app->catalog_mutex);
    SDL_RenderSetLogicalSize(app->renderer, SCREEN_WIDTH, SCREEN_HEIGHT);
    SDL_AtomicSet(&app->language, LANG_EN);
    app->ui_theme = UI_THEME_UBIQUITY;
    sys_import_native_snapshot(app);
    assert(ui_init(app));
    assert(game_list_view(app));
    assert(GAME_TABS[GAME_INSTALL] == TXT_GAME_SOURCES);
    assert(strcmp(TR(app, TXT_GAME_SOURCES), "Sources") == 0);
    assert(HOME_TAB_COUNT == 2);
    assert(HOME_TABS[0] == HOME_CAT_NEWEST && HOME_TABS[1] == HOME_CAT_POPULAR);
    assert(app->home_cat_index == HOME_CAT_NEWEST);
    assert(strcmp(home_category_title(app,HOME_CAT_NEWEST),"Newest Releases") == 0);
    SDL_Event tab_event = {0}; tab_event.type = SDL_CONTROLLERBUTTONDOWN;
    tab_event.cbutton.button = SDL_CONTROLLER_BUTTON_DPAD_RIGHT;
    assert(handle_game_list_input(app,&tab_event));
    assert(app->home_cat_index == HOME_CAT_POPULAR);
    assert(handle_game_list_input(app,&tab_event));
    assert(app->home_cat_index == HOME_CAT_NEWEST);
    tab_event.type = SDL_MOUSEBUTTONDOWN; tab_event.button.button = SDL_BUTTON_LEFT;
    tab_event.button.x = game_list_category_target(1).x + 10; tab_event.button.y = 150;
    assert(handle_game_list_input(app,&tab_event));
    assert(app->home_cat_index == HOME_CAT_POPULAR);
    tab_event.button.x = game_list_category_target(0).x + 10;
    assert(handle_game_list_input(app,&tab_event));
    assert(app->home_cat_index == HOME_CAT_NEWEST);
    GameItem metadata = {0}; char size[64];
    SDL_strlcpy(metadata.id_code,"PPSA00001",sizeof(metadata.id_code));
    assert(strcmp(game_list_console(&metadata),"PlayStation 5")==0);
    SDL_strlcpy(metadata.platform,"nintendo-switch",sizeof(metadata.platform));
    assert(strcmp(game_list_console(&metadata),"Nintendo Switch")==0);
    metadata.format=FMT_EXFAT; assert(!strcmp(game_list_format(&metadata),"EXFAT"));
    metadata.format=FMT_PKG; assert(!strcmp(game_list_format(&metadata),"PKG"));
    metadata.format=FMT_FFPKG; assert(!strcmp(game_list_format(&metadata),"FFPKG"));
    metadata.format=FMT_CATALOG; assert(!strcmp(game_list_format(&metadata),"—"));
    game_source_metadata(app,&metadata,size,sizeof(size)); assert(!strcmp(size,"Format not provided - Version not provided"));
    SDL_strlcpy(metadata.version,"01.008 / fpkg",sizeof(metadata.version));
    game_source_metadata(app,&metadata,size,sizeof(size)); assert(!strcmp(size,"FPKG - 01.008"));
    SDL_strlcpy(metadata.version,"01.060 / FFPFS",sizeof(metadata.version));
    game_source_metadata(app,&metadata,size,sizeof(size)); assert(!strcmp(size,"FFPFS - 01.060"));
    SDL_strlcpy(metadata.version," / 7Z",sizeof(metadata.version));
    game_source_metadata(app,&metadata,size,sizeof(size)); assert(!strcmp(size,"7Z - Version not provided"));
    metadata.format=FMT_EXFAT;
    SDL_strlcpy(metadata.version,"v01.008",sizeof(metadata.version));
    game_source_metadata(app,&metadata,size,sizeof(size)); assert(!strcmp(size,"EXFAT - v01.008"));

    game_list_size(&metadata,size); assert(strcmp(size,"—")==0);
    metadata.size_gb=-2; game_list_size(&metadata,size); assert(strcmp(size,"—")==0);
    metadata.size_gb=42; game_list_size(&metadata,size); assert(strcmp(size,"42.0 GB")==0);
    metadata.size_gb=0.033f; game_list_size(&metadata,size); assert(!strcmp(size,"33.0 MB"));
    metadata.size_gb=0.0000015f; game_list_size(&metadata,size); assert(!strcmp(size,"1.5 KB"));
    metadata.size_gb=0.000000001f; game_list_size(&metadata,size); assert(!strcmp(size,"1 B"));
    metadata.size_gb=1500; game_list_size(&metadata,size); assert(!strcmp(size,"1.5 TB"));
    assert(GAME_LIST_COLUMN_WEIGHTS[0]==17 && GAME_LIST_COLUMN_WEIGHTS[1]==39 &&
           GAME_LIST_COLUMN_WEIGHTS[2]==32 && GAME_LIST_COLUMN_WEIGHTS[3]==12);
    SDL_Rect first = game_list_row_target(0), next_page = game_list_row_target(10);
    assert(first.x == 130 && first.y == 220 && first.h == 64);
    assert(first.x == next_page.x && first.y == next_page.y);
    assert(game_list_row_target(9).y + GAME_LIST_ROW_HEIGHT < 980);
    assert(library_tile_target(7).y + library_tile_target(7).h < 910);
    assert(search_result_target(8).y + search_result_target(8).h < 931);
    SDL_Event event = {0}; event.type = SDL_KEYDOWN; event.key.keysym.sym = SDLK_DOWN;
    for(int i=0;i<10;i++) assert(handle_game_list_input(app,&event));
    assert(app->rows[VIEW_HOME].selected_col == 10);
    event.key.keysym.sym = SDLK_UP;
    assert(handle_game_list_input(app,&event));
    assert(app->rows[VIEW_HOME].selected_col == 9);
    event.type = SDL_MOUSEBUTTONDOWN; event.button.button = SDL_BUTTON_LEFT;
    event.button.x = 140; event.button.y = 286; /* Gap, never an invisible old tile. */
    assert(handle_game_list_input(app,&event) && !app->detail_open);

    event.button.x = 140; event.button.y = 230;
    assert(handle_game_list_input(app,&event));
    assert(app->detail_open && app->detail_item == app->rows[VIEW_HOME].items[0]);
    app->detail_open = false;
    render_game_list(app);
    render_sidebar(app); render_top_bar(app);
    assert(!app->artwork && !app->screenshots);
    const char *output = getenv("OBSIDIAN_LIST_SCREENSHOT");
    if(output) {
        SDL_Surface *shot = SDL_CreateRGBSurfaceWithFormat(0,1920,1080,32,SDL_PIXELFORMAT_RGBA32);
        assert(shot && SDL_RenderReadPixels(app->renderer,NULL,shot->format->format,shot->pixels,shot->pitch)==0);
        assert(IMG_SavePNG(shot,output)==0); SDL_FreeSurface(shot);
    }
    app->detail_item = app->rows[VIEW_HOME].items[0];
    render_ubiquity_game_details(app);
    assert(!app->artwork && !app->screenshots);
    app->active_row = VIEW_GAMES; app->library_depth = LIBRARY_COMPANIES;
    render_library_grid(app);
    assert(!app->artwork);
    app->active_row = VIEW_SEARCH;
    search_rebuild_locked(app);
    render_search(app);
    assert(!app->artwork);
    assert(GAME_TAB_COUNT == 3); /* No artwork/screenshot tab. */
    WolodriveApp *parsed=calloc(1,sizeof(*parsed)); assert(parsed);
    parsed->catalog_mutex=SDL_CreateMutex(); assert(parsed->catalog_mutex);
    const char *size_json="{\"name\":\"Sizes\",\"packages\":["
        "{\"id\":\"PPSA90001\",\"title\":\"Missing size\",\"format\":\"pkg\"},"
        "{\"id\":\"PPSA90002\",\"title\":\"Small file\",\"sizeBytes\":33000000,\"format\":\"pkg\"},"
        "{\"id\":\"PPSA90003\",\"title\":\"Decimal GB\",\"sizeBytes\":1500000000,\"format\":\"pkg\"},"
        "{\"id\":\"PPSA90004\",\"title\":\"One byte\",\"sizeBytes\":1,\"format\":\"pkg\"},"
        "{\"id\":\"PPSA90005\",\"title\":\"Ambiguous size\",\"size\":5000,\"format\":\"pkg\"},"
        "{\"id\":\"PPSA90006\",\"title\":\"Explicit GB\",\"sizeGB\":1.5,\"format\":\"pkg\"}]}";
    assert(parse_catalog_json(parsed,size_json,"Sizes",NULL,NULL,0)==6);
    assert(parsed->catalog[0].size_gb==0);
    assert(fabs(parsed->catalog[1].size_gb-0.033)<0.000001);
    assert(fabs(parsed->catalog[2].size_gb-1.5)<0.000001);
    assert(parsed->catalog[3].size_gb>0 && parsed->catalog[3].size_gb<1e-8);
    assert(parsed->catalog[4].size_gb==0);
    assert(parsed->catalog[5].size_gb==1.5f);
    SDL_DestroyMutex(parsed->catalog_mutex); free(parsed);
    ui_cleanup(app);
    SDL_DestroyRenderer(app->renderer); SDL_DestroyWindow(app->window);
    SDL_DestroyMutex(app->catalog_mutex); free(app); TTF_Quit(); SDL_Quit();
    puts("PASS: text-only list geometry, paging, selection, details and zero artwork requests");
    return 0;
}
