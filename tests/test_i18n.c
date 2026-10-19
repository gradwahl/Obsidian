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
    assert(i18n_detect_language() == LANG_DE);
    setenv("LC_ALL", "ja_JP.UTF-8", 1);
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
    const char *locales[] = { "en_GB", "pl_PL", "es_ES", "fr_FR", "nl_NL", "de_DE", "it_IT", "zh_CN", "ru_RU" };
    for (int lang = 0; lang < LANG_COUNT; lang++) {
        setenv("LC_ALL", locales[lang], 1);
        assert(i18n_detect_language() == (Language)lang);
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
    /* Validate actual glyph coverage for every translated label and language name. */
    for (int lang = 0; lang < LANG_COUNT; lang++) {
        for (int id = 0; id <= TEXT_COUNT; id++) {
            const unsigned char *text = (const unsigned char *)(id == TEXT_COUNT
                ? i18n_language_name((Language)lang) : i18n_text((Language)lang, (TextId)id));
            while (*text) {
                unsigned code = *text++;
                if (code >= 0xc0) {
                    int remaining = code < 0xe0 ? 1 : 2;
                    code &= remaining == 1 ? 0x1f : 0x0f;
                    while (remaining--) code = (code << 6) | (*text++ & 0x3f);
                }
                assert(TTF_GlyphIsProvided(app->font_body, (Uint16)code));
                assert(TTF_GlyphIsProvided(app->font_small, (Uint16)code));
            }
        }
    }
    assert(SDL_AtomicGet(&app->language) == LANG_EN);
    app->rows[0].selected_col = 2;
    GameItem *selected = app->rows[0].items[2];
    SDL_Event event = {0};
    SDL_zero(event);
    const Language order[] = { LANG_PL, LANG_EN, LANG_FR, LANG_NL, LANG_DE, LANG_ES, LANG_IT, LANG_ZH, LANG_RU };
    for (int lang = 0; lang < LANG_COUNT; lang++) {
        Language expected = order[lang];
        SDL_zero(event);
        event.type = SDL_MOUSEBUTTONDOWN;
        event.button.button = SDL_BUTTON_LEFT;
        event.button.x = 100;
        event.button.y = 68;
        ui_handle_input(app, &event);
        assert(app->language_menu_open);
        event.button.y = 122 + 8 + lang * 44 + 22;
        ui_handle_input(app, &event);
        assert(!app->language_menu_open);
        assert(SDL_AtomicGet(&app->language) == (int)expected);
        assert(strcmp(SDL_GetWindowTitle(app->window), TR(app, TXT_WINDOW_TITLE)) == 0);
        assert(strcmp(sys_format_to_string(app, FMT_FFPKG), TR(app, TXT_FORMAT_FOLDER)) == 0);
        assert(app->rows[0].items[app->rows[0].selected_col] == selected);
        assert(app->catalog[1].state == STATE_AVAILABLE);
        assert(app->catalog[1].progress == 0.0f);
        assert(strcmp(app->rows[VIEW_HOME].title, TR(app, TXT_NAV_HOME)) == 0);
        assert(strcmp(app->rows[VIEW_GAMES].title, TR(app, TXT_NAV_GAMES)) == 0);
        assert(strcmp(app->rows[VIEW_EMULATORS].title, TR(app, TXT_NAV_EMULATORS)) == 0);
        assert(strcmp(app->rows[VIEW_FILES].title, TR(app, TXT_NAV_FILES)) == 0);
        assert(strcmp(app->rows[VIEW_PAYLOADS].title, TR(app, TXT_NAV_PAYLOADS)) == 0);
        assert(strcmp(app->rows[VIEW_DOWNLOADS].title, TR(app, TXT_NAV_DOWNLOADS)) == 0);
        assert(i18n_load_language(app->language_path, LANG_COUNT) == expected);
        sys_trigger_action(app, selected);
        assert(strcmp(app->toast_message, TR(app, TXT_ACTION_UNAVAILABLE)) == 0);
        assert(selected->state == STATE_AVAILABLE && selected->progress == 0.0f);
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
            app->language_menu_open = true;
            app->language_menu_index = lang;
            ui_render(app);
            app->language_menu_open = false;
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
    /* Keyboard opens the menu, repeat is ignored, Escape cancels without quitting. */
    SDL_zero(event);
    event.type = SDL_KEYDOWN;
    event.key.keysym.sym = SDLK_l;
    app->running = true;
    ui_handle_input(app, &event);
    assert(app->language_menu_open);
    event.key.repeat = 1;
    ui_handle_input(app, &event);
    assert(app->language_menu_open);
    event.key.keysym.sym = SDLK_ESCAPE;
    ui_handle_input(app, &event);
    assert(!app->language_menu_open && app->running);
    assert(SDL_AtomicGet(&app->language) == LANG_RU);
    /* Controller wraps through all options, then selects Polish. */
    SDL_zero(event);
    event.type = SDL_CONTROLLERBUTTONDOWN;
    event.cbutton.button = SDL_CONTROLLER_BUTTON_X;
    ui_handle_input(app, &event);
    assert(app->language_menu_open);
    event.cbutton.button = SDL_CONTROLLER_BUTTON_DPAD_DOWN;
    ui_handle_input(app, &event);
    assert(app->language_menu_index == 0);
    event.cbutton.button = SDL_CONTROLLER_BUTTON_A;
    ui_handle_input(app, &event);
    assert(!app->language_menu_open);
    assert(SDL_AtomicGet(&app->language) == LANG_PL);
    /* Outside clicks dismiss; navigation inside the menu never changes the catalog. */
    event.cbutton.button = SDL_CONTROLLER_BUTTON_X;
    ui_handle_input(app, &event);
    event.cbutton.button = SDL_CONTROLLER_BUTTON_DPAD_UP;
    ui_handle_input(app, &event);
    assert(app->language_menu_index == LANG_COUNT - 1);
    assert(app->rows[0].selected_col == 2);
    SDL_zero(event);
    event.type = SDL_MOUSEBUTTONDOWN;
    event.button.button = SDL_BUTTON_LEFT;
    event.button.x = 1000;
    event.button.y = 100;
    ui_handle_input(app, &event);
    assert(!app->language_menu_open && SDL_AtomicGet(&app->language) == LANG_PL);
    event.type = SDL_CONTROLLERBUTTONDOWN;
    event.cbutton.button = SDL_CONTROLLER_BUTTON_X;
    ui_handle_input(app, &event);
    event.cbutton.button = SDL_CONTROLLER_BUTTON_B;
    ui_handle_input(app, &event);
    assert(!app->language_menu_open && app->rows[0].selected_col == 2);
    /* Keyboard navigation applies an explicit choice. */
    SDL_zero(event);
    event.type = SDL_KEYDOWN;
    event.key.keysym.sym = SDLK_l;
    ui_handle_input(app, &event);
    event.key.keysym.sym = SDLK_DOWN;
    ui_handle_input(app, &event);
    event.key.keysym.sym = SDLK_RETURN;
    ui_handle_input(app, &event);
    assert(SDL_AtomicGet(&app->language) == LANG_EN);
    sys_set_language(app, LANG_PL);
    /* A new app instance restores the saved choice ahead of the environment. */
    WolodriveApp *restarted = calloc(1, sizeof(*restarted));
    assert(restarted);
    sys_init(restarted);
    assert(SDL_AtomicGet(&restarted->language) == LANG_PL);
    free(restarted);
    app->language_path[0] = '\0';
    sys_set_language(app, LANG_ES);
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
