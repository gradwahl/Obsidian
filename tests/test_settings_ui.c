/* Settings navigation must stop at Reset after removing Appearance. */
#include <assert.h>
#include "../src/ui_netflix.c"

int main(void) {
    assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
    assert(TTF_Init() == 0);
    WolodriveApp *app = calloc(1, sizeof(*app));
    assert(app);
    app->catalog_mutex = SDL_CreateMutex();
    assert(app->catalog_mutex);
    app->window = SDL_CreateWindow("Settings test", 0, 0, 1920, 1080, SDL_WINDOW_HIDDEN);
    assert(app->window);
    app->renderer = SDL_CreateRenderer(app->window, -1, SDL_RENDERER_SOFTWARE);
    assert(app->renderer && ui_init(app));
    SDL_AtomicSet(&app->language, LANG_EN);
    app->ui_theme = UI_THEME_UBIQUITY;
    app->active_row = VIEW_SETTINGS;
    app->settings_column = 1;
    app->settings_selection = 4;
    SDL_Event event = {0};
    event.type = SDL_KEYDOWN;
    event.key.keysym.sym = SDLK_DOWN;
    assert(handle_settings_input(app, &event));
    assert(app->settings_selection == 5);
    event.type = SDL_CONTROLLERBUTTONDOWN;
    event.cbutton.button = SDL_CONTROLLER_BUTTON_DPAD_DOWN;
    assert(handle_settings_input(app, &event));
    assert(app->settings_selection == 5);
    app->filters.disabled_formats = 15;
    app->filters.sources_limited = true;
    event.cbutton.button = SDL_CONTROLLER_BUTTON_A;
    assert(handle_settings_input(app, &event));
    assert(!app->filters.disabled_formats && !app->filters.sources_limited);
    app->filters.disabled_formats = 15;
    SDL_Rect reset = settings_target(1, 5);
    event.type = SDL_MOUSEBUTTONDOWN;
    event.button.button = SDL_BUTTON_LEFT;
    event.button.x = reset.x + 20;
    event.button.y = reset.y + 20;
    assert(handle_settings_input(app, &event));
    assert(!app->filters.disabled_formats);
    assert(app->ui_theme == UI_THEME_UBIQUITY);
    render_settings(app);
    SDL_Surface *image = SDL_CreateRGBSurfaceWithFormat(0, 1920, 1080, 32, SDL_PIXELFORMAT_RGBA32);
    assert(image);
    assert(SDL_RenderReadPixels(app->renderer, NULL, image->format->format, image->pixels, image->pitch) == 0);
    assert(IMG_SavePNG(image, "build/settings-native.png") == 0);
    SDL_FreeSurface(image);
    ui_cleanup(app);
    SDL_DestroyMutex(app->catalog_mutex);
    SDL_DestroyRenderer(app->renderer);
    SDL_DestroyWindow(app->window);
    free(app);
    TTF_Quit();
    SDL_Quit();
    puts("PASS: Settings keyboard/controller bounds, pointer reset and sole Ubiquity theme");
    return 0;
}
