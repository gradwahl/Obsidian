#include "wolodrive.h"
#include "emulator_download.h"
#include "frame_timing.h"
#include "startup_log.h"
#include "startup_splash.h"
#include <curl/curl.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int main(int argc, char *argv[]) {
#ifdef __PS5__
    FILE *perf_log = NULL;
#endif
    startup_log_init();
    startup_log_stage("Entered main (startup-log-v1)");
    (void)argc;
    (void)argv;

    WolodriveApp *app = calloc(1, sizeof(*app));
    if (!app) {
        startup_log_stage("App allocation failed");
        return 1;
    }
    app->running = true;

    /* Get the first visible frame up before initializing network, image
       decoders, fonts and controller discovery. VideoOut itself must be
       created by SDL first; no payload can draw earlier than that. */
    startup_log_stage("Initializing SDL video and timer");
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
        fprintf(stderr, "[OBSIDIAN ERROR] SDL_Init: %s\n", SDL_GetError());
        free(app);
        return 1;
    }

    startup_log_stage("Creating catalog mutex");
    app->catalog_mutex = SDL_CreateMutex();
    if (!app->catalog_mutex) {
        fprintf(stderr, "[OBSIDIAN ERROR] SDL_CreateMutex: %s\n", SDL_GetError());
        SDL_Quit();
        free(app);
        return 1;
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

    startup_log_stage("Creating window");
    app->window = SDL_CreateWindow(
        "Obsidian",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        win_w, win_h, win_flags
    );

    if (!app->window) {
        fprintf(stderr, "[OBSIDIAN ERROR] SDL_CreateWindow: %s\n", SDL_GetError());
        SDL_DestroyMutex(app->catalog_mutex);
        TTF_Quit();
        IMG_Quit();
        SDL_Quit();
        free(app);
        return 1;
    }

    int exit_code = 1;
    startup_log_stage("Creating renderer");
    if (!ui_create_renderer(app)) goto cleanup;
    startup_log_stage("Displaying loading screen");
    startup_splash_draw(app->renderer, 0);

    startup_log_stage("Initializing curl");
    if (curl_global_init(CURL_GLOBAL_ALL) != CURLE_OK) {
        fprintf(stderr, "[OBSIDIAN ERROR] curl_global_init failed\n");
        goto cleanup;
    }
    startup_splash_draw(app->renderer, 1);

    startup_log_stage("Initializing image decoders");
    int img_flags = IMG_INIT_PNG | IMG_INIT_JPG | IMG_INIT_WEBP;
    if ((IMG_Init(img_flags) & img_flags) == 0)
        fprintf(stderr, "[OBSIDIAN WARN] IMG_Init: %s\n", IMG_GetError());
    startup_splash_draw(app->renderer, 2);

    startup_log_stage("Initializing font library");
    if (TTF_Init() != 0) {
        fprintf(stderr, "[OBSIDIAN ERROR] TTF_Init: %s\n", TTF_GetError());
        goto cleanup;
    }
    startup_splash_draw(app->renderer, 3);

    startup_log_stage("Initializing controllers");
    if (SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER) != 0)
        fprintf(stderr, "[OBSIDIAN WARN] Gamecontroller initialization: %s\n", SDL_GetError());
    else for (int i = 0; i < SDL_NumJoysticks(); i++) {
        if (SDL_IsGameController(i)) {
            SDL_GameControllerOpen(i);
            break;
        }
    }
    startup_splash_draw(app->renderer, 4);

    startup_log_stage("Initializing system/catalog state");
    sys_init(app);
    startup_splash_draw(app->renderer, 5);
    startup_log_stage("Loading UI fonts and state");
    if (!ui_init(app)) goto cleanup;
    startup_splash_draw(app->renderer, 6);
    startup_log_stage("Creating translation service");
    app->translations = translation_create();
    startup_splash_draw(app->renderer, 7);

    /* Automatyczne pobranie default-sources.json (Pegasus-DL) oraz okładek w tle przy starcie */
    startup_log_stage("Starting catalog sync");
    catalog_start_async_sync(app);

    Uint64 frequency = SDL_GetPerformanceFrequency();
    Uint64 last_frame = SDL_GetPerformanceCounter();
    Uint64 frame_deadline = last_frame;
    app->last_stats_update = SDL_GetTicks();
#ifdef __PS5__
    /* Log measured update/render/present cost on hardware, independently of
       the 60 Hz frame cap. Keep logging off the per-frame hot path. */
    perf_log = fopen("/data/wolodrive-performance.log", "w");
    if (perf_log) fprintf(perf_log, "Obsidian PS5 native frame timings (ms)\n");
    Uint64 perf_window_start = last_frame;
    Uint64 perf_count = 0;
    double perf_update_sum = 0, perf_render_sum = 0, perf_render_max = 0;
#endif

    startup_log_stage("Entering render loop");
#ifdef __PS5__
    bool startup_first_frame = true;
#endif
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

#ifdef __PS5__
        Uint64 perf_update_begin = SDL_GetPerformanceCounter();
#endif
        ui_update_animations(app, dt);
#ifdef __PS5__
        Uint64 perf_render_begin = SDL_GetPerformanceCounter();
#endif
        ui_render(app);
#ifdef __PS5__
        Uint64 perf_render_end = SDL_GetPerformanceCounter();
        double update_ms = 1000.0 * (perf_render_begin - perf_update_begin) / frequency;
        double render_ms = 1000.0 * (perf_render_end - perf_render_begin) / frequency;
        perf_update_sum += update_ms;
        perf_render_sum += render_ms;
        if (render_ms > perf_render_max) perf_render_max = render_ms;
        perf_count++;
        if (perf_log && (perf_render_end - perf_window_start) >= 5 * frequency) {
            double seconds = (double)(perf_render_end - perf_window_start) / frequency;
            fprintf(perf_log, "FPS %.1f | update avg %.2f | render avg %.2f | render max %.2f | samples %llu\n",
                    perf_count / seconds, perf_update_sum / perf_count,
                    perf_render_sum / perf_count, perf_render_max,
                    (unsigned long long)perf_count);
            fflush(perf_log);
            perf_window_start = perf_render_end;
            perf_count = 0;
            perf_update_sum = perf_render_sum = perf_render_max = 0;
        }
        if (startup_first_frame) {
            startup_log_stage("First render returned (display not independently verified)");
            startup_first_frame = false;
        }
#endif
        if (app->running) {
            frame_deadline = frame_next_deadline(frame_deadline, SDL_GetPerformanceCounter(), frequency);
            frame_wait_until(frame_deadline, frequency);
        }
    }

    exit_code = 0;
cleanup:
#ifdef __PS5__
    if (perf_log) fclose(perf_log);
#endif
    startup_log_stage(exit_code ? "Cleaning up after startup failure" : "Cleaning up after app exit");
    app->running = false;
    catalog_stop_sync(app);
    emulator_download_shutdown(app);
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
