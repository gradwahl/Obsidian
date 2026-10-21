/* Offline snapshot parity and source filter regression test.
   No PS5 SDK, network access, or graphical session needed. */
#include "wolodrive.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "native_catalog_snapshot.h"

int main(void) {
    WolodriveApp *app = calloc(1, sizeof(*app));
    assert(app);
    int added = sys_import_native_snapshot(app);
    assert(added == NATIVE_CATALOG_SEED_COUNT);
    assert(app->catalog_count == NATIVE_CATALOG_SEED_COUNT);
    assert(app->catalog_source_count == NATIVE_SOURCE_COUNT);
    assert(sys_import_native_snapshot(app) == 0);
    assert(app->catalog_count == NATIVE_CATALOG_SEED_COUNT);
    int pippo = 0, games = 0;
    bool archive = false, axekin = false, nps = false;
    for (int i=0; i<app->catalog_source_count; i++) {
        const char *src = app->catalog_source_names[i];
        if (!strcmp(src, "Archive.org / AlvRo")) archive = true;
        if (!strcmp(src, "Axekin")) axekin = true;
        if (!strcmp(src, "NoPayStation")) nps = true;
    }
    assert(archive && axekin && nps);
    for (int i=0; i<app->catalog_count; i++) {
        GameItem *it = &app->catalog[i];
        games += it->is_game;
        if (!strcmp(it->source_name, "Pippo .exFAT")) {
            pippo++;
            assert(it->format == FMT_EXFAT);
            assert(it->catalog_added_at != 0);
            assert(it->release_date[0]);
        }
    }
    assert(pippo >= 800 && games > 800);
    for(int i=0;i<app->catalog_count;i++) {
        GameItem *it=&app->catalog[i];
        if(!strcmp(it->title,"Unknown 9 Awakening Deluxe Edition")) assert(it->size_gb==58);
        if(!strcmp(it->title,"NAIAD")) assert(it->size_gb==1);
        if(!strcmp(it->title,"Lies of P: Overture")) assert(it->size_gb==72);
        if(!strcmp(it->source_name,"Pegasus-DL / Obsidian")) assert(it->size_gb==0);
    }
    assert(SDL_Init(0) == 0);
    app->catalog_mutex=SDL_CreateMutex();
    assert(app->catalog_mutex);
    SDL_AtomicSet(&app->language,LANG_EN);
    ui_rebuild_carousels(app);
    assert(app->home_categories[HOME_CAT_PS5].item_count == MAX_ITEMS_PER_ROW);
    assert(app->home_categories[HOME_CAT_POPULAR].item_count == MAX_ITEMS_PER_ROW);
    for(int i=1;i<app->home_categories[HOME_CAT_POPULAR].item_count;i++)
        assert(app->home_categories[HOME_CAT_POPULAR].items[i-1]->download_count >=
               app->home_categories[HOME_CAT_POPULAR].items[i]->download_count);
    for(int i=1;i<app->home_categories[HOME_CAT_NEWEST].item_count;i++)
        assert(strcmp(app->home_categories[HOME_CAT_NEWEST].items[i-1]->release_date,
                      app->home_categories[HOME_CAT_NEWEST].items[i]->release_date) >= 0);
    app->active_row = VIEW_HOME;
    app->home_cat_index = HOME_CAT_NEWEST;
    app->sidebar_focus = false;
    app->rows[VIEW_HOME].selected_col = 0;
    app->home_categories[HOME_CAT_NEWEST].selected_col = 0;
    SDL_Event event = {0};
    event.type = SDL_KEYDOWN; event.key.keysym.sym = SDLK_DOWN;
    for(int i=0;i<10;i++) ui_handle_input(app,&event);
    assert(app->rows[VIEW_HOME].selected_col == 10);
    assert(app->home_categories[HOME_CAT_NEWEST].selected_col == 10);
    event.type = SDL_CONTROLLERBUTTONDOWN; event.cbutton.button = SDL_CONTROLLER_BUTTON_DPAD_UP;
    ui_handle_input(app,&event);
    assert(app->rows[VIEW_HOME].selected_col == 9);
    event.cbutton.button = SDL_CONTROLLER_BUTTON_DPAD_RIGHT;
    ui_handle_input(app,&event);
    assert(app->home_cat_index == HOME_CAT_POPULAR);
    event.cbutton.button = SDL_CONTROLLER_BUTTON_DPAD_LEFT;
    ui_handle_input(app,&event);
    assert(app->sidebar_focus);
    event.cbutton.button = SDL_CONTROLLER_BUTTON_DPAD_RIGHT;
    ui_handle_input(app,&event);
    assert(!app->sidebar_focus);
    event.type = SDL_MOUSEBUTTONDOWN; event.button.button = SDL_BUTTON_LEFT;
    event.button.x = 140; event.button.y = 230;
    ui_handle_input(app,&event);
    assert(app->detail_open);
    assert(app->detail_item == app->rows[VIEW_HOME].items[0]);
    app->detail_open = false;
    app->filters.sources_limited = true;
    app->filters.source_count = 1;
    SDL_strlcpy(app->filters.sources[0], "Pippo .exFAT", 64);
    for (int i=0; i<app->catalog_count; i++)
        assert(settings_accepts_item(app, &app->catalog[i]) ==
               (strcmp(app->catalog[i].source_name,"Pippo .exFAT") == 0));
    app->filters.disabled_formats = 1u << FMT_EXFAT;
    for (int i=0; i<app->catalog_count; i++)
        assert(!settings_accepts_item(app, &app->catalog[i]));
    printf("Native snapshot OK: %d items, %d sources, %d Pippo entries\n",
           app->catalog_count, app->catalog_source_count, pippo);
    SDL_DestroyMutex(app->catalog_mutex);
    free(app);
    SDL_Quit();
    return 0;
}
