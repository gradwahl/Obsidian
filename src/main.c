#include "wolodrive.h"
#include "frame_timing.h"
#include <curl/curl.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    WolodriveApp *app = calloc(1, sizeof(*app));
    if (!app) return 1;
    app->running = true;

    curl_global_init(CURL_GLOBAL_ALL);

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER | SDL_INIT_TIMER) != 0) {
        fprintf(stderr, "[POLDRIVE ERROR] SDL_Init: %s\n", SDL_GetError());
        free(app);
        return 1;
    }

    int img_flags = IMG_INIT_PNG | IMG_INIT_JPG | IMG_INIT_WEBP;
    if ((IMG_Init(img_flags) & img_flags) == 0) {
        fprintf(stderr, "[POLDRIVE WARN] IMG_Init: %s\n", IMG_GetError());
    }

    if (TTF_Init() != 0) {
        fprintf(stderr, "[POLDRIVE ERROR] TTF_Init: %s\n", TTF_GetError());
        IMG_Quit();
        SDL_Quit();
        free(app);
        return 1;
    }

    app->catalog_mutex = SDL_CreateMutex();

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

    app->window = SDL_CreateWindow(
        "PolDrive",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        win_w, win_h, win_flags
    );

    if (!app->window) {
        fprintf(stderr, "[POLDRIVE ERROR] SDL_CreateWindow: %s\n", SDL_GetError());
        SDL_DestroyMutex(app->catalog_mutex);
        TTF_Quit();
        IMG_Quit();
        SDL_Quit();
        free(app);
        return 1;
    }

    int exit_code = 1;
    if (!ui_create_renderer(app)) goto cleanup;

    sys_init(app);
    if (!ui_init(app)) goto cleanup;
    app->translations = translation_create();

    /* Automatyczne pobranie default-sources.json (Pegasus-DL) oraz okładek w tle przy starcie */
    catalog_start_async_sync(app);

    Uint64 frequency = SDL_GetPerformanceFrequency();
    Uint64 last_frame = SDL_GetPerformanceCounter();
    Uint64 frame_deadline = last_frame;
    app->last_stats_update = SDL_GetTicks();

    while (app->running) {
        uint32_t now = SDL_GetTicks();
        Uint64 frame_start = SDL_GetPerformanceCounter();
        float dt = (float)((double)(frame_start - last_frame) / frequency);
        if (dt > 0.1f) dt = 0.1f;
        last_frame = frame_start;

        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_QUIT) {
                app->running = false;
            } else if (ev.type == SDL_CONTROLLERDEVICEADDED) {
                SDL_GameControllerOpen(ev.cdevice.which);
                sys_send_notification(app, TR(app, TXT_CONTROLLER));
            } else {
                ui_handle_input(app, &ev);
            }
        }

        if (now - app->last_stats_update >= 2000) {
            sys_update_stats(&app->stats);
            app->last_stats_update = now;
        }

        ui_update_animations(app, dt);
        ui_render(app);
        if (app->running) {
            frame_deadline = frame_next_deadline(frame_deadline, SDL_GetPerformanceCounter(), frequency);
            frame_wait_until(frame_deadline, frequency);
        }
    }

    exit_code = 0;
cleanup:
    app->running = false;
    catalog_stop_sync(app);
    translation_destroy(app->translations);
    ui_cleanup(app);
    if (app->catalog_mutex) SDL_DestroyMutex(app->catalog_mutex);
    if (app->renderer)      SDL_DestroyRenderer(app->renderer);
    if (app->window)        SDL_DestroyWindow(app->window);
    TTF_Quit();
    IMG_Quit();
    SDL_Quit();
    curl_global_cleanup();
    free(app);
    return exit_code;
}
