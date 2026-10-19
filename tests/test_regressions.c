/* Exercise private rendering/parser seams without adding a production test API. */
#include "wolodrive.h"
#include "text.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if SDL_VERSION_ATLEAST(2, 0, 18)
static bool force_texture_fallback;
static int geometry_or_fallback(SDL_Renderer *renderer, SDL_Texture *texture,
                                const SDL_Vertex *vertices, int vertex_count,
                                const int *indices, int index_count) {
    if (force_texture_fallback) return -1;
    return SDL_RenderGeometry(renderer, texture, vertices, vertex_count, indices, index_count);
}
#define SDL_RenderGeometry geometry_or_fallback
#endif
#include "../src/ui_netflix.c"
#undef SDL_RenderGeometry
#include "../src/catalog_pegasus.c"

static void test_description_text(void) {
    char out[384];
    text_plain_description("<p>Notes: <b>Debug Menu</b>.</p><p>Touchpad &amp; Options<br/>Next line.</p>", out, sizeof(out));
    assert(strcmp(out, "Notes: Debug Menu. Touchpad & Options Next line.") == 0);
    text_plain_description("&lt;p&gt;Escaped &quot;markup&quot; &#39;works&#39;&lt;/p&gt;", out, sizeof(out));
    assert(strcmp(out, "Escaped \"markup\" 'works'") == 0);
    text_plain_description("  中文&nbsp;Русский &#x4E2D;&#25991;\n  A < 3 > 1  ", out, sizeof(out));
    assert(strcmp(out, "中文 Русский 中文 A < 3 > 1") == 0);
    text_plain_description("A<!-- comment -->B<style>bad css</style><SCRIPT>bad script</SCRIPT><p>C</p>", out, sizeof(out));
    assert(strcmp(out, "AB C") == 0);
    text_plain_description("<p title='a > b'>Text</p>&unknown; &#xZZ;", out, sizeof(out));
    assert(strcmp(out, "Text &unknown; &#xZZ;") == 0);
    text_plain_description("<p> </p>", out, sizeof(out));
    assert(out[0] == '\0');
    struct { char text[5]; char canary; } small = {{0}, 'X'};
    text_plain_description("中文 English", small.text, sizeof(small.text));
    assert(strcmp(small.text, "中") == 0 && small.canary == 'X');
    text_plain_description(NULL, out, sizeof(out));
    assert(!out[0]);

    const char *json = "a\\\"b\\n\\u4e2d\\u6587 \\ud83d\\ude80\"";
    assert(text_decode_json_string(json, json + strlen(json), out, sizeof(out)));
    assert(strcmp(out, "a\"b\n中文 🚀") == 0);
    json = "中文more\"";
    assert(text_decode_json_string(json, json + strlen(json), small.text, sizeof(small.text)));
    assert(strcmp(small.text, "中") == 0 && small.canary == 'X');
    json = "missing closing quote";
    assert(!text_decode_json_string(json, json + strlen(json), out, sizeof(out)));
    json = "\\uD800\"";
    assert(!text_decode_json_string(json, json + strlen(json), out, sizeof(out)));
    puts("HTML tags/entities, JSON Unicode and UTF-8 truncation passed.");
}

static void test_truthful_catalog_state(void) {
    WolodriveApp *app = calloc(1, sizeof(*app));
    assert(app);
    app->catalog_mutex = SDL_CreateMutex();
    assert(app->catalog_mutex);
    sys_init(app);
    assert(app->catalog_count > 0);
    for (int i = 0; i < app->catalog_count; i++) {
        assert(app->catalog[i].state == STATE_AVAILABLE && app->catalog[i].progress == 0.0f);
    }
    const char *json = "{\"games\":["
        "{\"titleId\":\"PPSA08338\",\"title\":\"Marvel's Spider-Man 2\","
        "\"description\":\"<p>Notes: &amp; \\u4e2d\\u6587</p>\",\"installed\":true},"
        "{\"titleId\":\"TEST00001\",\"title\":\"Remote test\",\"description\":\"<p> </p>\",\"state\":\"installed\"}]}";
    assert(parse_catalog_json(app, json, "Test", NULL, NULL, 0) == 2);
    assert(strcmp(app->catalog[0].description, "Notes: & 中文") == 0);
    assert(app->catalog[app->catalog_count - 1].description_id == TXT_DESCRIPTION_FALLBACK);
    for (int i = 0; i < app->catalog_count; i++) {
        assert(app->catalog[i].state == STATE_AVAILABLE && app->catalog[i].progress == 0.0f);
        sys_trigger_action(app, &app->catalog[i]);
        assert(app->catalog[i].state == STATE_AVAILABLE && app->catalog[i].progress == 0.0f);
        assert(strcmp(app->toast_message, TR(app, TXT_ACTION_UNAVAILABLE)) == 0);
    }
    ui_rebuild_carousels(app);
    assert(app->rows[VIEW_DOWNLOADS].item_count == 0);
    SDL_DestroyMutex(app->catalog_mutex);
    free(app);
    puts("No seeded installations, fake progress, or installation claims from remote metadata.");
}

static void test_scaled_gradient(int width, int height, int alpha) {
    SDL_Window *window = SDL_CreateWindow("Gradient regression", 0, 0, width, height, 0);
    assert(window);
    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    assert(renderer);
    assert(SDL_RenderSetLogicalSize(renderer, SCREEN_WIDTH, SCREEN_HEIGHT) == 0);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    fill_vertical_gradient(renderer, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT,
                           255, 255, 255, alpha, 255, 255, 255, alpha, 64);
    /* Capture the full physical target, including letterboxing, without a logical viewport. */
    assert(SDL_RenderSetLogicalSize(renderer, 0, 0) == 0);
    SDL_Surface *image = SDL_CreateRGBSurfaceWithFormat(0, width, height, 32, SDL_PIXELFORMAT_RGBA32);
    assert(image);
    assert(SDL_RenderReadPixels(renderer, NULL, image->format->format, image->pixels, image->pitch) == 0);
    int content_height = width * SCREEN_HEIGHT / SCREEN_WIDTH;
    int top = height > content_height ? (height - content_height) / 2 : 0;
    int bottom = height > content_height ? top + content_height : height;
    for (int y = top + 2; y < bottom - 2; y++) {
        for (int x = width / 4; x < 3 * width / 4; x++) {
            Uint8 red, green, blue, pixel_alpha;
            Uint32 pixel = *(Uint32 *)((Uint8 *)image->pixels + y * image->pitch + x * 4);
            SDL_GetRGBA(pixel, image->format, &red, &green, &blue, &pixel_alpha);
            if (abs((int)red - alpha) > 1) {
                fprintf(stderr, "Gradient %dx%d alpha=%d pixel=(%d,%d) actual=%d\n", width, height, alpha, x, y, red);
            }
            assert(abs((int)red - alpha) <= 1);
            assert(abs((int)green - alpha) <= 1 && abs((int)blue - alpha) <= 1);
        }
    }
    SDL_FreeSurface(image);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
}

static void test_tv_catalog_navigation(void) {
    assert(TTF_Init() == 0);
    IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG | IMG_INIT_WEBP);
    WolodriveApp *app = calloc(1, sizeof(*app));
    assert(app);
    app->running = true;
    app->catalog_mutex = SDL_CreateMutex();
    app->window = SDL_CreateWindow("TV catalog regression", 0, 0, 1280, 720, 0);
    assert(app->catalog_mutex && app->window);
    assert(ui_create_renderer(app));
    sys_init(app);
    for (int i = 0; i < app->catalog_count; i++) {
        app->catalog[i].is_game = true;
        app->catalog[i].catalog_added_at = 20261018120000ULL - (uint64_t)i * 10000;
    }
    assert(ui_init(app));
    SDL_Event event = {0};
    event.type = SDL_KEYDOWN;
    event.key.keysym.sym = SDLK_RIGHT;
    ui_handle_input(app, &event);
    assert(app->rows[0].selected_col == 1);
    ui_update_animations(app, 1.0f);
    assert(app->rows[0].scroll_x == CARD_WIDTH + CARD_GAP);
    assert(card_target(app, 1).x == CONTENT_X);

    /* Mouse selection and activation use the visible, scrolled card geometry. */
    event.type = SDL_MOUSEBUTTONDOWN;
    event.button.button = SDL_BUTTON_LEFT;
    event.button.x = CONTENT_X + CARD_WIDTH + CARD_GAP + 10;
    event.button.y = CARDS_Y + 20;
    app->toast_message[0] = '\0';
    ui_handle_input(app, &event);
    assert(app->rows[0].selected_col == 2 && !app->toast_message[0]);
    ui_update_animations(app, 1.0f);
    event.button.x = CONTENT_X + 20;
    event.button.y = CARDS_Y + CARD_HEIGHT + 20;
    ui_handle_input(app, &event);
    assert(strcmp(app->toast_message, TR(app, TXT_ACTION_UNAVAILABLE)) == 0);
    assert(get_focused_item(app)->state == STATE_AVAILABLE);

    /* All six views remain reachable, including the unimplemented sections. */
    event.type = SDL_CONTROLLERBUTTONDOWN;
    event.cbutton.button = SDL_CONTROLLER_BUTTON_DPAD_DOWN;
    for (int r = 1; r < TOTAL_ROWS; r++) {
        ui_handle_input(app, &event);
        assert(app->active_row == r);
        if (r >= VIEW_EMULATORS && r <= VIEW_PAYLOADS) {
            assert(!get_focused_item(app));
            assert(strcmp(empty_view_message(app), TR(app, TXT_SECTION_UNAVAILABLE)) == 0);
        }
    }
    assert(!get_focused_item(app));
    assert(strcmp(empty_view_message(app), TR(app, TXT_NO_DOWNLOADS)) == 0);
    app->toast_expire_tick = 0;
    ui_render(app); /* Empty category must clear the previous game's artwork. */
    SDL_Surface *pixel = SDL_CreateRGBSurfaceWithFormat(0, 1, 1, 32, SDL_PIXELFORMAT_RGBA32);
    SDL_Rect sample = {900, 200, 1, 1};
    assert(pixel);
    assert(SDL_RenderReadPixels(app->renderer, &sample, pixel->format->format,
                                pixel->pixels, pixel->pitch) == 0);
    Uint8 red, green, blue, alpha;
    SDL_GetRGBA(*(Uint32 *)pixel->pixels, pixel->format, &red, &green, &blue, &alpha);
    assert(red == 0 && green == 0 && blue == 0);
    SDL_FreeSurface(pixel);

    event.type = SDL_MOUSEBUTTONDOWN;
    event.button.button = SDL_BUTTON_LEFT;
    event.button.x = 50;
    event.button.y = sidebar_target(0).y + 40;
    ui_handle_input(app, &event);
    assert(app->active_row == 0 && app->rows[0].selected_col == 2);
    for (int r = 0; r < VIEW_COUNT; r++) {
        event.button.y = sidebar_target(r).y + 40;
        ui_handle_input(app, &event);
        assert(app->active_row == r);
    }
    event.button.y = sidebar_target(VIEW_HOME).y + 40;
    ui_handle_input(app, &event);
    assert(app->rows[VIEW_GAMES].item_count == app->rows[VIEW_HOME].item_count);
    bool ps4_present = false, ps5_present = false;
    for (int i = 0; i < app->rows[VIEW_GAMES].item_count; i++) {
        GameItem *item = app->rows[VIEW_GAMES].items[i];
        if (item->format == FMT_PKG) ps4_present = true;
        else ps5_present = true;
    }
    assert(ps4_present && ps5_present);
    ui_render(app);
    /* Description's moved Original button still responds to mouse input. */
    get_focused_item(app)->description_id = TEXT_COUNT;
    event.button.x = DESCRIPTION_BUTTON.x + 20;
    event.button.y = DESCRIPTION_BUTTON.y + 20;
    ui_handle_input(app, &event);
    assert(app->show_original_description);

    /* Games/Roms is a company -> console -> filtered games browser. */
    assert(library_company_count() == 6);
    assert(!library_company_at(-1) && !library_company_at(6));
    assert(!library_console_at(0, -1) && !library_console_at(0, 99));
    assert(library_console_matches_id(library_console_at(0, 3), "cusa12345"));
    assert(!library_console_matches_id(library_console_at(0, 3), "PPSA12345"));
    event.button.x = 50;
    event.button.y = sidebar_target(VIEW_GAMES).y + 40;
    ui_handle_input(app, &event);
    assert(app->library_depth == LIBRARY_COMPANIES && app->library_sidebar_focus);
    assert(!get_focused_item(app));
    ui_render(app);
    SDL_zero(event);
    event.type = SDL_KEYDOWN;
    event.key.keysym.sym = SDLK_RIGHT;
    ui_handle_input(app, &event);
    assert(!app->library_sidebar_focus);
    event.key.keysym.sym = SDLK_RETURN;
    ui_handle_input(app, &event);
    assert(app->library_depth == LIBRARY_CONSOLES && app->library_company == 0);
    event.key.keysym.sym = SDLK_RIGHT;
    for (int i = 0; i < 3; i++) ui_handle_input(app, &event);
    assert(app->library_console_selection == 3);
    event.key.keysym.sym = SDLK_RETURN;
    ui_handle_input(app, &event);
    assert(app->library_depth == LIBRARY_GAMES && app->rows[VIEW_GAMES].item_count > 0);
    for (int i = 0; i < app->rows[VIEW_GAMES].item_count; i++) {
        assert(strncmp(app->rows[VIEW_GAMES].items[i]->id_code, "CUSA", 4) == 0);
    }
    ui_render(app);
    event.key.keysym.sym = SDLK_ESCAPE;
    ui_handle_input(app, &event);
    assert(app->library_depth == LIBRARY_CONSOLES && app->library_console_selection == 3 && app->running);
    event.key.keysym.sym = SDLK_LEFT;
    for (int i = 0; i < 3; i++) ui_handle_input(app, &event);
    event.key.keysym.sym = SDLK_DOWN;
    ui_handle_input(app, &event);
    assert(app->library_console_selection == 4);
    event.key.keysym.sym = SDLK_RETURN;
    ui_handle_input(app, &event);
    assert(app->rows[VIEW_GAMES].item_count > 0);
    for (int i = 0; i < app->rows[VIEW_GAMES].item_count; i++) {
        assert(strncmp(app->rows[VIEW_GAMES].items[i]->id_code, "PPSA", 4) == 0);
    }
    SDL_zero(event);
    event.type = SDL_CONTROLLERBUTTONDOWN;
    event.cbutton.button = SDL_CONTROLLER_BUTTON_B;
    ui_handle_input(app, &event);
    assert(app->library_depth == LIBRARY_CONSOLES);
    ui_handle_input(app, &event);
    assert(app->library_depth == LIBRARY_COMPANIES);
    ui_handle_input(app, &event);
    assert(app->library_sidebar_focus);
    event.cbutton.button = SDL_CONTROLLER_BUTTON_DPAD_DOWN;
    ui_handle_input(app, &event);
    assert(app->active_row == VIEW_EMULATORS);
    event.cbutton.button = SDL_CONTROLLER_BUTTON_DPAD_UP;
    ui_handle_input(app, &event);
    assert(app->active_row == VIEW_GAMES && app->library_depth == LIBRARY_COMPANIES);

    SDL_zero(event);
    event.type = SDL_MOUSEBUTTONDOWN;
    event.button.button = SDL_BUTTON_LEFT;
    /* Every company tile opens its own console list. */
    for (int c = 0; c < library_company_count(); c++) {
        SDL_Rect tile = library_tile_target(c);
        assert(tile.w == tile.h && tile.x + tile.w <= SCREEN_WIDTH && tile.y + tile.h < 944);
        event.button.x = tile.x + 40;
        event.button.y = tile.y + 40;
        ui_handle_input(app, &event);
        assert(app->library_depth == LIBRARY_CONSOLES && app->library_company == c);
        assert(library_company_at(c)->console_count > 0);
        ui_render(app);
        event.button.x = LIBRARY_BACK_BUTTON.x + 20;
        event.button.y = LIBRARY_BACK_BUTTON.y + 20;
        ui_handle_input(app, &event);
        assert(app->library_depth == LIBRARY_COMPANIES);
    }
    SDL_Rect nintendo_tile = library_tile_target(3);
    event.button.x = nintendo_tile.x + 40;
    event.button.y = nintendo_tile.y + 40;
    ui_handle_input(app, &event);
    SDL_zero(event);
    event.type = SDL_KEYDOWN;
    event.key.keysym.sym = SDLK_DOWN;
    ui_handle_input(app, &event);
    ui_handle_input(app, &event);
    assert(app->library_console_selection == 8); /* Second page is reachable. */
    ui_render(app);
    SDL_Rect console_tile = library_tile_target(12);
    SDL_zero(event);
    event.type = SDL_MOUSEBUTTONDOWN;
    event.button.button = SDL_BUTTON_LEFT;
    event.button.x = console_tile.x + 40;
    event.button.y = console_tile.y + 40;
    ui_handle_input(app, &event);
    assert(app->library_depth == LIBRARY_GAMES && app->library_console_selection == 12);
    assert(!app->rows[VIEW_GAMES].item_count && !get_focused_item(app));
    assert(strcmp(empty_view_message(app), TR(app, TXT_CONSOLE_NOT_CONNECTED)) == 0);
    ui_render(app);
    event.button.x = LIBRARY_BACK_BUTTON.x + 20;
    event.button.y = LIBRARY_BACK_BUTTON.y + 20;
    ui_handle_input(app, &event);
    assert(app->library_depth == LIBRARY_CONSOLES && app->library_console_selection == 12);
    puts("Company grids, console pages, PS4/PS5 filtering, empty platforms and keyboard/controller/mouse back navigation passed.");
    ui_cleanup(app);
    SDL_DestroyRenderer(app->renderer);
    SDL_DestroyWindow(app->window);
    SDL_DestroyMutex(app->catalog_mutex);
    free(app);
    IMG_Quit();
    TTF_Quit();
    puts("Six labeled sidebar views, keyboard/controller/mouse navigation, mixed game formats, action footer and empty states passed.");
}

static void test_latest_catalog_games(void) {
    assert(parse_catalog_date("2026-10-18T21:14:29.526Z") == 20261018211429ULL);
    assert(parse_catalog_date("2024-02-29T23:59:59Z") == 20240229235959ULL);
    const char *invalid[] = {"", "2026-02-29T12:00:00Z", "2026-04-31T12:00:00Z",
        "2026-13-01T00:00:00Z", "2026-10-18T24:00:00Z", "2026-10-18T00:60:00Z",
        "2026-10-18T00:00:60Z", "2026-10-18", "2026-10-18T00:00:00.Z",
        "2026-10-18T00:00:00Zjunk", "2026-10-18T00:00:00+01:00"};
    for (size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); i++) assert(!parse_catalog_date(invalid[i]));

    WolodriveApp *app = calloc(1, sizeof(*app));
    assert(app);
    app->catalog_mutex = SDL_CreateMutex();
    assert(app->catalog_mutex);
    const char *json = "{\"packages\":["
        "{\"titleId\":\"OLD\",\"title\":\"Older game\",\"category\":\"game\",\"createdAt\":\"2026-01-01T00:00:00Z\"},"
        "{\"titleId\":\"NEW\",\"title\":\"New game\",\"category\":\"game\",\"createdAt\":\"2026-10-18T21:14:29.526Z\"},"
        "{\"titleId\":\"DLC\",\"title\":\"New DLC\",\"category\":\"dlc\",\"createdAt\":\"2026-10-19T00:00:00Z\"},"
        "{\"titleId\":\"APP\",\"title\":\"Utility\",\"category\":\"PKG\",\"createdAt\":\"2026-10-19T00:00:00Z\"},"
        "{\"titleId\":\"UNKNOWN\",\"title\":\"Unknown date\",\"category\":\"game\",\"updatedAt\":\"2026-10-19T00:00:00Z\"},"
        "{\"titleId\":\"INVALID\",\"title\":\"Invalid date\",\"category\":\"game\",\"createdAt\":\"2026-02-30T00:00:00Z\"}]}";
    assert(parse_catalog_json(app, json, "First source", NULL, NULL, 0) == 6);
    ui_rebuild_carousels(app);
    CarouselRow *home = &app->rows[VIEW_HOME];
    assert(home->item_count == 2 && app->rows[VIEW_GAMES].item_count == 6);
    assert(strcmp(home->items[0]->title, "New game") == 0);
    assert(strcmp(home->items[1]->title, "Older game") == 0);
    home->selected_col = 1;
    GameItem *selected = home->items[1];
    /* A later source's older timestamp must not move a duplicate game backwards. */
    const char *later = "{\"packages\":["
        "{\"titleId\":\"NEW\",\"title\":\"New game\",\"category\":\"game\",\"createdAt\":\"2025-01-01T00:00:00Z\"},"
        "{\"titleId\":\"NEWEST\",\"title\":\"Newest game\",\"category\":\"game\",\"createdAt\":\"2026-10-19T00:00:00Z\"}]}";
    assert(parse_catalog_json(app, later, "Later source", NULL, NULL, 0) == 2);
    ui_rebuild_carousels(app);
    assert(home->item_count == 3 && strcmp(home->items[0]->title, "Newest game") == 0);
    assert(home->items[home->selected_col] == selected);
    assert(home->items[1]->catalog_added_at == 20261018211429ULL);

    /* A newest game past the former 256-entry cutoff must still lead Home. */
    for (int i = app->catalog_count; i < 300; i++) {
        GameItem *item = &app->catalog[i];
        snprintf(item->title, sizeof(item->title), "Catalog game %03d", i);
        item->is_game = true;
        item->catalog_added_at = 20200101000000ULL;
    }
    app->catalog_count = 300;
    assert(parse_catalog_json(app, "{\"packages\":[{\"titleId\":\"LATE\",\"title\":\"Late-source newest\","
        "\"category\":\"game\",\"createdAt\":\"2026-10-19T12:00:00Z\"}]}", "Last source", NULL, NULL, 0) == 1);
    home->selected_col = 0;
    ui_rebuild_carousels(app);
    assert(home->item_count == MAX_ITEMS_PER_ROW);
    assert(home->items[0] == &app->catalog[300]);
    for (int i = 1; i < home->item_count; i++) {
        assert(home->items[i - 1]->catalog_added_at >= home->items[i]->catalog_added_at);
    }
    GameItem *covers[MAX_COVER_PREFETCH];
    int count = cover_prefetch_locked(app, covers);
    assert(count <= MAX_COVER_PREFETCH && covers[0] == home->items[0]);
    for (int i = 0; i < home->item_count; i++) assert(covers[i] == home->items[i]);
    for (int i = 0; i < count; i++) {
        for (int j = i + 1; j < count; j++) assert(covers[i] != covers[j]);
    }
    /* Missing dates never turn seed order into a supposed latest-release feed. */
    for (int i = 0; i < app->catalog_count; i++) app->catalog[i].catalog_added_at = 0;
    ui_rebuild_carousels(app);
    assert(home->item_count == 0 && home->selected_col == 0);
    SDL_DestroyMutex(app->catalog_mutex);
    free(app);
    puts("Latest games: UTC dates, game filtering, source merging, ordering beyond 256 entries, selection and bounded cover priority passed.");
}

int main(void) {
    assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
    test_description_text();
    test_truthful_catalog_state();
    test_latest_catalog_games();
    test_tv_catalog_navigation();
    const int sizes[][2] = {{1280, 720}, {1365, 767}, {1920, 1080}, {800, 600}};
    for (int fallback = 0; fallback < 2; fallback++) {
#if SDL_VERSION_ATLEAST(2, 0, 18)
        force_texture_fallback = fallback != 0;
#endif
        for (size_t i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i++) {
            test_scaled_gradient(sizes[i][0], sizes[i][1], 255);
            test_scaled_gradient(sizes[i][0], sizes[i][1], 128);
        }
    }
    SDL_Quit();
    puts("Opaque and blended gradients have no scaling gaps/overlaps at four window sizes, including fallback rendering.");
    return 0;
}
