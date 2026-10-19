/* Offline software-renderer benchmark. Run from the repository working directory. */
#include "wolodrive.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
    assert(TTF_Init() == 0);
    WolodriveApp *app = calloc(1, sizeof(*app));
    assert(app);
    app->window = SDL_CreateWindow("UI benchmark", 0, 0, 1280, 720, SDL_WINDOW_HIDDEN);
    app->renderer = SDL_CreateRenderer(app->window, -1, SDL_RENDERER_SOFTWARE);
    assert(app->renderer);
    SDL_RenderSetLogicalSize(app->renderer, SCREEN_WIDTH, SCREEN_HEIGHT);
    assert(ui_init(app));
    for (int i = 0; i < 128; i++) {
        GameItem *item = &app->catalog[i];
        snprintf(item->id_code, sizeof(item->id_code), "PPSA%05d", i);
        snprintf(item->title, sizeof(item->title), "Navigation benchmark game %03d", i);
        item->is_game = true;
        item->catalog_added_at = 20261019000000ULL + i;
        item->description_id = TEXT_COUNT;
        SDL_strlcpy(item->description, "Offline catalog description with several lines of readable text. "
                    "Navigate between games while artwork overrides and labels are resolved.", sizeof(item->description));
    }
    app->catalog_count = 128;
    ui_rebuild_carousels(app);
    for (int moving = 0; moving < 2; moving++) {
        for (int i = 0; i < 5; i++) ui_render(app);
        Uint64 start = SDL_GetPerformanceCounter();
        for (int i = 0; i < 60; i++) {
            if (moving) app->rows[VIEW_HOME].selected_col = i / 3;
            ui_update_animations(app, 1.0f / 60);
            ui_render(app);
        }
        double ms = 1000.0 * (SDL_GetPerformanceCounter() - start) / SDL_GetPerformanceFrequency() / 60;
        printf("%s: %.2f ms/frame (%.1f FPS), software 1280x720, 128 offline games\n",
               moving ? "Navigation" : "Stationary", ms, 1000.0/ms);
    }
    ui_cleanup(app);
    SDL_DestroyRenderer(app->renderer);
    SDL_DestroyWindow(app->window);
    free(app);
    TTF_Quit();
    SDL_Quit();
    return 0;
}
