#include "wolodrive.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void test_locale_and_preferences(void) {
    unsetenv("LC_ALL");
    unsetenv("LC_MESSAGES");
    unsetenv("LANG");
    assert(i18n_detect_language() == LANG_EN);
    setenv("LANG", "es_ES.UTF-8", 1);
    assert(i18n_detect_language() == LANG_ES);
    setenv("LC_MESSAGES", "pl-PL", 1);
    assert(i18n_detect_language() == LANG_PL);
    setenv("LC_ALL", "EN_gb.UTF-8", 1);
    assert(i18n_detect_language() == LANG_EN);
    setenv("LC_ALL", "de_DE.UTF-8", 1);
    assert(i18n_detect_language() == LANG_EN);
    setenv("LC_ALL", "C", 1);
    assert(i18n_detect_language() == LANG_EN);
    setenv("LC_ALL", "", 1);
    assert(i18n_detect_language() == LANG_PL);
    setenv("LC_ALL", "plausible", 1);
    assert(i18n_detect_language() == LANG_EN);

    char path[1024];
    snprintf(path, sizeof(path), "%s/preference", getenv("XDG_DATA_HOME"));
    assert(i18n_load_language(path, LANG_PL) == LANG_PL);
    for (int lang = 0; lang < LANG_COUNT; lang++) {
        assert(i18n_save_language(path, (Language)lang));
        assert(i18n_load_language(path, LANG_EN) == (Language)lang);
    }
    FILE *file = fopen(path, "w");
    assert(file);
    fputs("invalid preference\n", file);
    fclose(file);
    assert(i18n_load_language(path, LANG_ES) == LANG_ES);
    assert(!i18n_save_language("/nonexistent/wolodrive/language", LANG_PL));
    assert(!i18n_save_language(path, LANG_COUNT));
    assert(i18n_load_language(NULL, LANG_COUNT) == LANG_EN);
    assert(strcmp(i18n_text(LANG_COUNT, TXT_DOWNLOAD),
                  i18n_text(LANG_EN, TXT_DOWNLOAD)) == 0);
    assert(strcmp(i18n_text(LANG_EN, TEXT_COUNT), "") == 0);
    for (int id = 0; id < TEXT_COUNT; id++) {
        for (int lang = 0; lang < LANG_COUNT; lang++) {
            assert(i18n_text((Language)lang, (TextId)id)[0]);
        }
    }
    unlink(path);
}

static void test_live_switching(void) {
    assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
    assert(TTF_Init() == 0);
    IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG | IMG_INIT_WEBP);
    WolodriveApp *app = calloc(1, sizeof(*app));
    assert(app);
    app->catalog_mutex = SDL_CreateMutex();
    assert(app->catalog_mutex);
    app->window = SDL_CreateWindow("Language test", 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, 0);
    assert(app->window);
    /* WSL must select software even when acceleration can appear available. */
    setenv("WSL_DISTRO_NAME", "wolodrive-test", 1);
    unsetenv("SDL_RENDER_DRIVER");
    SDL_ResetHint(SDL_HINT_RENDER_DRIVER);
    assert(ui_create_renderer(app));
    SDL_RendererInfo info;
    assert(SDL_GetRendererInfo(app->renderer, &info) == 0);
    assert(strcmp(info.name, "software") == 0);
    setenv("LC_ALL", "en_US.UTF-8", 1);
    sys_init(app);
    assert(ui_init(app));
    assert(app->font_body && app->font_small && app->font_hero_title);
    assert(SDL_AtomicGet(&app->language) == LANG_EN);
    app->rows[0].selected_col = 2;
    GameItem *selected = app->rows[0].items[2];
    SDL_Event event = {0};
    SDL_zero(event);
    event.type = SDL_KEYDOWN;
    event.key.keysym.sym = SDLK_l;
    for (int lang = LANG_PL; lang <= LANG_COUNT; lang++) {
        ui_handle_input(app, &event);
        Language expected = (Language)(lang % LANG_COUNT);
        assert(SDL_AtomicGet(&app->language) == (int)expected);
        assert(app->rows[0].items[app->rows[0].selected_col] == selected);
        assert(app->catalog[1].state == STATE_DOWNLOADING);
        assert(app->catalog[1].progress == 42.5f);
        assert(strcmp(app->rows[0].title, TR(app, TXT_ROW_RECENT)) == 0);
        assert(i18n_load_language(app->language_path, LANG_COUNT) == expected);
        sys_trigger_action(app, selected);
        assert(strstr(app->toast_message, selected->title));
        sys_cancel_action(app, selected);
        app->sync_status_id = TXT_SYNC_SUBCATALOG;
        app->sync_current = 1;
        app->sync_total = 3;
        ui_render(app);
        app->sync_status_id = TXT_SYNC_COVER;
        snprintf(app->sync_title, sizeof(app->sync_title), "%s", selected->title);
        ui_render(app);
        app->sync_status_id = TXT_SYNC_DONE;
        app->sync_sources = 3;
        ui_render(app);
        if (getenv("WOLODRIVE_TEST_SCREENSHOTS")) {
            SDL_Surface *surface = SDL_CreateRGBSurfaceWithFormat(0, SCREEN_WIDTH, SCREEN_HEIGHT,
                                                                32, SDL_PIXELFORMAT_RGBA32);
            assert(surface);
            assert(SDL_RenderReadPixels(app->renderer, NULL, surface->format->format,
                                        surface->pixels, surface->pitch) == 0);
            char path[80];
            snprintf(path, sizeof(path), "build/language-%d.png", expected);
            assert(IMG_SavePNG(surface, path) == 0);
            SDL_FreeSurface(surface);
        }
    }
    event.key.repeat = 1;
    ui_handle_input(app, &event);
    assert(SDL_AtomicGet(&app->language) == LANG_EN);
    event.type = SDL_CONTROLLERBUTTONDOWN;
    event.cbutton.button = SDL_CONTROLLER_BUTTON_X;
    ui_handle_input(app, &event);
    assert(SDL_AtomicGet(&app->language) == LANG_PL);
    /* A new app instance restores the saved choice ahead of the environment. */
    WolodriveApp *restarted = calloc(1, sizeof(*restarted));
    assert(restarted);
    sys_init(restarted);
    assert(SDL_AtomicGet(&restarted->language) == LANG_PL);
    free(restarted);
    app->language_path[0] = '\0';
    sys_cycle_language(app);
    assert(SDL_AtomicGet(&app->language) == LANG_ES);
    assert(strstr(app->toast_message, "no se pudo guardar"));
    /* Publisher descriptions and generated fallbacks both render after switching. */
    selected->description_id = TEXT_COUNT;
    snprintf(selected->description, sizeof(selected->description), "Publisher text: acción, łódź");
    ui_render(app);
    assert(strcmp(selected->description, "Publisher text: acción, łódź") == 0);
    selected->description_id = TXT_DESCRIPTION_FALLBACK;
    ui_render(app);
    ui_cleanup(app);
    SDL_DestroyRenderer(app->renderer);
    SDL_DestroyWindow(app->window);
    SDL_DestroyMutex(app->catalog_mutex);
    free(app);
    IMG_Quit();
    TTF_Quit();
    SDL_Quit();
}

int main(void) {
    setbuf(stdout, NULL);
    test_locale_and_preferences();
    test_live_switching();
    puts("Locale detection, fallback, persistence, keyboard/controller switching and rendering passed.");
    return 0;
}
