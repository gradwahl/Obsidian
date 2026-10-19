#include "wolodrive.h"
#include <curl/curl.h>
#include <stdio.h>
#include <string.h>

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    WolodriveApp app;
    memset(&app, 0, sizeof(app));
    app.running = true;

    curl_global_init(CURL_GLOBAL_ALL);

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER | SDL_INIT_TIMER) != 0) {
        fprintf(stderr, "[WOLODRIVE ERROR] SDL_Init: %s\n", SDL_GetError());
        return 1;
    }

    int img_flags = IMG_INIT_PNG | IMG_INIT_JPG | IMG_INIT_WEBP;
    if ((IMG_Init(img_flags) & img_flags) == 0) {
        fprintf(stderr, "[WOLODRIVE WARN] IMG_Init: %s\n", IMG_GetError());
    }

    if (TTF_Init() != 0) {
        fprintf(stderr, "[WOLODRIVE ERROR] TTF_Init: %s\n", TTF_GetError());
        IMG_Quit();
        SDL_Quit();
        return 1;
    }

    app.catalog_mutex = SDL_CreateMutex();

    for (int i = 0; i < SDL_NumJoysticks(); i++) {
        if (SDL_IsGameController(i)) {
            SDL_GameControllerOpen(i);
            break;
        }
    }

#ifdef __PS5__
    uint32_t win_flags = SDL_WINDOW_FULLSCREEN;
    int win_w = SCREEN_WIDTH;
    int win_h = SCREEN_HEIGHT;
#else
    uint32_t win_flags = SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE;
    int win_w = 1280;
    int win_h = 720;
#endif

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");

    app.window = SDL_CreateWindow(
        "Wolodrive 2.0 - PS5 Homebrew Store & Pegasus-DL (Netflix UI)",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        win_w, win_h, win_flags
    );

    if (!app.window) {
        fprintf(stderr, "[WOLODRIVE ERROR] SDL_CreateWindow: %s\n", SDL_GetError());
        SDL_DestroyMutex(app.catalog_mutex);
        TTF_Quit();
        IMG_Quit();
        SDL_Quit();
        return 1;
    }

    app.renderer = SDL_CreateRenderer(app.window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!app.renderer) {
        app.renderer = SDL_CreateRenderer(app.window, -1, SDL_RENDERER_SOFTWARE);
    }

    SDL_RenderSetLogicalSize(app.renderer, SCREEN_WIDTH, SCREEN_HEIGHT);

    sys_init(&app);
    ui_init(&app);

    /* Automatyczne pobranie default-sources.json (Pegasus-DL) oraz okładek w tle przy starcie */
    catalog_start_async_sync(&app);

    uint32_t last_tick = SDL_GetTicks();
    app.last_stats_update = last_tick;

    while (app.running) {
        uint32_t now = SDL_GetTicks();
        float dt = (now - last_tick) / 1000.0f;
        if (dt > 0.1f) dt = 0.1f;
        last_tick = now;

        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_QUIT) {
                app.running = false;
            } else if (ev.type == SDL_CONTROLLERDEVICEADDED) {
                SDL_GameControllerOpen(ev.cdevice.which);
                sys_send_notification(&app, "Podłączono kontroler DualSense");
            } else {
                ui_handle_input(&app, &ev);
            }
        }

        if (now - app.last_stats_update >= 2000) {
            sys_update_stats(&app.stats);
            app.last_stats_update = now;
        }

        sys_update_downloads(&app, dt);
        ui_update_animations(&app, dt);
        ui_render(&app);
    }

    ui_cleanup(&app);
    if (app.catalog_mutex) SDL_DestroyMutex(app.catalog_mutex);
    if (app.renderer)      SDL_DestroyRenderer(app.renderer);
    if (app.window)        SDL_DestroyWindow(app.window);
    TTF_Quit();
    IMG_Quit();
    SDL_Quit();
    curl_global_cleanup();
    return 0;
}
