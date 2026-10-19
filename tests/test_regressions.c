/* Exercise private rendering/parser seams without adding a production test API. */
#include "wolodrive.h"
#include "text.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

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
    int imported_start = app->catalog_count;
    assert(parse_catalog_json(app, json, "Test", NULL, NULL, 0) == 2);
    assert(strcmp(app->catalog[imported_start].description, "Notes: & 中文") == 0);
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

static void test_search(WolodriveApp *app) {
    select_sidebar_view(app, VIEW_HOME);
    SDL_Event event = {0};
    event.type = SDL_KEYDOWN;
    event.key.keysym.sym = SDLK_UP;
    ui_handle_input(app, &event);
    assert(app->active_row == VIEW_SEARCH && app->sidebar_focus);
    assert(sidebar_target(VIEW_SEARCH).y + sidebar_target(VIEW_SEARCH).h == sidebar_target(VIEW_HOME).y);
    event.key.keysym.sym = SDLK_RIGHT;
    ui_handle_input(app, &event);
    assert(!app->sidebar_focus);
    /* Search beyond the carousel limit, with title IDs and ASCII case folding. */
    for (int i = app->catalog_count; i < 300; i++) {
        snprintf(app->catalog[i].title, sizeof(app->catalog[i].title), "Search fixture %d", i);
        snprintf(app->catalog[i].id_code, sizeof(app->catalog[i].id_code), "TEST%05d", i);
    }
    app->catalog_count = 300;
    ui_rebuild_carousels(app);
    assert(app->search_count == 300);
    search_edit(app, "test00299", false);
    assert(app->search_count == 1 && app->search_results[0] == &app->catalog[299]);
    app->search_query[0] = '\0';
    app->search_count = 0;
    event.type = SDL_TEXTINPUT;
    SDL_strlcpy(event.text.text, "SEARCH FIXTURE", sizeof(event.text.text));
    ui_handle_input(app, &event);
    assert(app->search_count > MAX_ITEMS_PER_ROW);
    ui_render(app);
    app->search_results_focus = true;
    event.type = SDL_CONTROLLERBUTTONDOWN;
    event.cbutton.button = SDL_CONTROLLER_BUTTON_DPAD_DOWN;
    for (int i = 0; i < 3; i++) ui_handle_input(app, &event);
    assert(app->search_selected == 9 && app->active_row == VIEW_SEARCH);
    GameItem *selected = get_focused_item(app);
    ui_rebuild_carousels(app);
    assert(get_focused_item(app) == selected);
    event.cbutton.button = SDL_CONTROLLER_BUTTON_A;
    ui_handle_input(app, &event);
    assert(app->detail_open);
    ui_render(app);
    event.cbutton.button = SDL_CONTROLLER_BUTTON_B;
    ui_handle_input(app, &event);
    assert(!app->detail_open && get_focused_item(app) == selected);
    /* UTF-8 deletion removes a whole character, and L remains usable as text. */
    app->search_query[0] = '\0';
    search_edit(app, "é中", false);
    search_edit(app, "", true);
    assert(strcmp(app->search_query, "é") == 0);
    search_edit(app, "", true);
    assert(!app->search_query[0]);
    event.type = SDL_KEYDOWN;
    event.key.keysym.sym = SDLK_l;
    ui_handle_input(app, &event);
    assert(!app->language_menu_open);
    event.type = SDL_TEXTINPUT;
    SDL_strlcpy(event.text.text, "no-such-entry-xyz", sizeof(event.text.text));
    ui_handle_input(app, &event);
    assert(!app->search_count && !get_focused_item(app));
    ui_render(app);
    event.type = SDL_MOUSEBUTTONDOWN;
    event.button.button = SDL_BUTTON_LEFT;
    event.button.x = search_key_target(37).x + 10;
    event.button.y = search_key_target(37).y + 10;
    ui_handle_input(app, &event);
    assert(strcmp(app->search_query, "no-such-entry-xy") == 0);
    event.type = SDL_CONTROLLERBUTTONDOWN;
    event.cbutton.button = SDL_CONTROLLER_BUTTON_Y;
    ui_handle_input(app, &event);
    assert(strcmp(app->search_query, "no-such-entry-x") == 0);
    search_edit(app, "", false);
    app->search_query[0] = '\0';
    search_edit(app, "", false);
    app->search_key = 0;
    event.cbutton.button = SDL_CONTROLLER_BUTTON_A;
    ui_handle_input(app, &event);
    assert(strcmp(app->search_query, "a") == 0);
    event.cbutton.button = SDL_CONTROLLER_BUTTON_DPAD_LEFT;
    ui_handle_input(app, &event);
    assert(app->sidebar_focus);
    event.cbutton.button = SDL_CONTROLLER_BUTTON_DPAD_DOWN;
    ui_handle_input(app, &event);
    assert(app->active_row == VIEW_HOME);
    /* Reproduce source labels from the duplicate Search report. */
    const char *variants[] = {
        "Assassins Creed Valhalla", "[exFAT] Assassin's Creed® Valhalla",
        "Assassins Creed Valhalla (DLC)", "Assassins Creed Valhalla Complete Edition",
        "Assassin's Creed Valhalla — Complete Edition", "Assassins Creed Valhalla",
        "Assassin's Creed Mirage", "Assassin’s Creed® Mirage",
        "Assassins Creed Black Flag Resynced", "[FFPKG] Assassin's Creed Black Flag Resynced",
        "Assassins Creed Black Flag Resynced Deluxe Edition"
    };
    for (int i = 0; i < 11; i++) {
        GameItem *item = &app->catalog[300 + i];
        SDL_strlcpy(item->title, variants[i], sizeof(item->title));
        SDL_strlcpy(item->id_code, i == 5 ? "CUSA12345" : "PPSA12345", sizeof(item->id_code));
        item->is_game = i != 2;
        item->format = i == 1 ? FMT_EXFAT : FMT_FFPKG;
    }
    app->catalog_count = 311;
    select_sidebar_view(app, VIEW_SEARCH);
    app->search_query[0] = '\0';
    search_edit(app, "assass", false);
    /* Base, DLC, Complete, PS4 base, Mirage, Black Flag and Deluxe. */
    assert(app->search_count == 7);
    assert(app->catalog_count == 311); /* Package variants remain intact. */
    app->search_selected = 0;
    app->sidebar_focus = false;
    ui_open_game(app, &app->catalog[300]);
    app->detail_content_focus = true;
    event.type = SDL_KEYDOWN;
    event.key.keysym.sym = SDLK_RIGHT;
    ui_handle_input(app, &event);
    assert(app->detail_selection == 1);
    ui_rebuild_carousels(app);
    assert(app->search_count == 7 && app->detail_selection == 1);
    event.key.keysym.sym = SDLK_LEFT;
    ui_handle_input(app, &event);
    assert(app->detail_selection == 0);
    ui_render(app);
    app->detail_open = false;
    puts("Search groups format/typography duplicates, preserves DLC/editions/platforms and cycles every retained package.");
    puts("Search: full catalog, title/ID matching, UTF-8 editing, paging, refresh, details and keyboard/controller/mouse input passed.");
}

static void test_game_details(WolodriveApp *app) {
    const char *json = "{\"games\":["
        "{\"titleId\":\"PPSA90000\",\"title\":\"Detail Test\",\"category\":\"game\","
        "\"description\":\"<p>Game overview &amp; information.</p>\","
        "\"screenshots\":[\"https://example.com/one.png\",{\"url\":\"https://example.com/two.png\"},"
        "\"javascript:bad\",\"https://example.com/one.png\"]},"
        "{\"titleId\":\"PPSA90000\",\"title\":\"[exFAT] Detail Test\",\"category\":\"game\"},"
        "{\"titleId\":\"DLC90001\",\"title\":\"Expansion\",\"category\":\"dlc\",\"baseTitleId\":\"PPSA90000\"},"
        "{\"titleId\":\"DLC90002\",\"title\":\"Unrelated expansion\",\"category\":\"dlc\",\"parentTitleId\":\"PPSA99999\"},"
        "{\"titleId\":\"PPSA90000\",\"title\":\"Bonus DLC\",\"category\":\"dlc\"}]}";
    int base_index = app->catalog_count;
    assert(parse_catalog_json(app, json, "Panel source", NULL, NULL, 0) == 5);
    GameItem *base = &app->catalog[base_index];
    assert(base->screenshot_count == 2 && !base->download_url[0]);
    assert(app->catalog[base_index+2].is_dlc);
    assert(strcmp(app->catalog[base_index+2].parent_title_id, base->id_code) == 0);
    app->active_row = VIEW_HOME;
    app->sidebar_focus = false;
    app->rows[VIEW_HOME].items[0] = base;
    app->rows[VIEW_HOME].item_count = 1;
    app->rows[VIEW_HOME].selected_col = 0;
    SDL_Event event = {0};
    SDL_zero(event);
    event.type = SDL_KEYDOWN;
    event.key.keysym.sym = SDLK_RETURN;
    app->toast_message[0] = '\0';
    ui_handle_input(app, &event);
    assert(app->detail_open && app->detail_item == base && !app->toast_message[0]);
    GameItem *entries[MAX_CATALOG_ITEMS];
    assert(game_entries(app, entries) == 2);
    event.key.keysym.sym = SDLK_DOWN;
    ui_handle_input(app, &event);
    assert(app->detail_content_focus && !app->language_menu_open);
    event.key.keysym.sym = SDLK_RETURN;
    ui_handle_input(app, &event);
    assert(base->state == STATE_AVAILABLE);
    assert(strcmp(app->toast_message, TR(app, TXT_ACTION_UNAVAILABLE)) == 0);
    app->toast_expire_tick = 0;
    for (int tab = 0; tab < GAME_TAB_COUNT; tab++) {
        event.type = SDL_MOUSEBUTTONDOWN;
        event.button.button = SDL_BUTTON_LEFT;
        event.button.x = game_tab_target(tab).x + 20;
        event.button.y = game_tab_target(tab).y + 20;
        ui_handle_input(app, &event);
        assert(app->detail_tab == tab && app->active_row == VIEW_HOME);
        ui_render(app);
        if (getenv("WOLODRIVE_TEST_SCREENSHOTS")) {
            int w, h;
            SDL_GetRendererOutputSize(app->renderer, &w, &h);
            SDL_Surface *surface = SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_RGBA32);
            assert(surface && SDL_RenderReadPixels(app->renderer, NULL, surface->format->format, surface->pixels, surface->pitch) == 0);
            char path[80];
            snprintf(path, sizeof(path), "build/game-panel-%d.bmp", tab);
            assert(SDL_SaveBMP(surface, path) == 0);
            SDL_FreeSurface(surface);
        }
    }
    game_select_tab(app, GAME_DLC);
    assert(game_entries(app, entries) == 2);
    event.type = SDL_CONTROLLERBUTTONDOWN;
    event.cbutton.button = SDL_CONTROLLER_BUTTON_DPAD_DOWN;
    ui_handle_input(app, &event);
    event.cbutton.button = SDL_CONTROLLER_BUTTON_A;
    ui_handle_input(app, &event);
    assert(app->detail_parent == base && app->detail_item == &app->catalog[base_index+2]);
    event.cbutton.button = SDL_CONTROLLER_BUTTON_B;
    ui_handle_input(app, &event);
    assert(app->detail_item == base && app->detail_tab == GAME_DLC);
    event.cbutton.button = SDL_CONTROLLER_BUTTON_DPAD_RIGHT;
    ui_handle_input(app, &event);
    assert(app->detail_tab == GAME_SCREENSHOTS);
    event.cbutton.button = SDL_CONTROLLER_BUTTON_B;
    ui_handle_input(app, &event);
    assert(!app->detail_open && app->rows[VIEW_HOME].selected_col == 0 && app->running);
    app->active_row = VIEW_GAMES;
    app->library_depth = LIBRARY_GAMES;
    app->rows[VIEW_GAMES].items[0] = base;
    app->rows[VIEW_GAMES].selected_col = 0;
    app->rows[VIEW_GAMES].item_count = 1;
    event.cbutton.button = SDL_CONTROLLER_BUTTON_A;
    ui_handle_input(app, &event);
    assert(app->detail_open && app->detail_item == base);
    app->detail_item = &app->catalog[base_index+1];
    game_select_tab(app, GAME_DLC);
    ui_render(app);
    game_select_tab(app, GAME_SCREENSHOTS);
    ui_render(app);
    event.type = SDL_MOUSEBUTTONDOWN;
    event.button.button = SDL_BUTTON_LEFT;
    event.button.x = GAME_CLOSE.x+10;
    event.button.y = GAME_CLOSE.y+10;
    ui_handle_input(app, &event);
    assert(!app->detail_open);
    puts("Shared game panel: all entry points, four tabs, install truthfulness, related DLC, screenshot metadata and modal return passed.");
}

static void test_settings(WolodriveApp *app) {
    char original_path[512];
    SDL_strlcpy(original_path, app->settings_path, sizeof(original_path));
    SDL_strlcpy(app->settings_path, "build/filter-test.preferences", sizeof(app->settings_path));
    const char *exfat = "{\"games\":[{\"titleId\":\"PPSA88888\",\"title\":\"SettingsGame\",\"category\":\"game\",\"format\":\"exfat\",\"createdAt\":\"2026-10-19T00:00:00Z\"}]}";
    const char *folder = "{\"games\":[{\"titleId\":\"PPSA88888\",\"title\":\"SettingsGame\",\"category\":\"game\",\"format\":\"ffpkg\",\"createdAt\":\"2026-10-19T00:00:00Z\"}]}";
    const char *pkg = "{\"games\":[{\"titleId\":\"PPSA88888\",\"title\":\"SettingsGame\",\"category\":\"game\",\"format\":\"pkg\",\"createdAt\":\"2026-10-19T00:00:00Z\"}]}";
    int first = app->catalog_count;
    assert(parse_catalog_json(app, exfat, "Source Alpha", NULL, NULL, 0) == 1);
    assert(parse_catalog_json(app, folder, "Source Beta", NULL, NULL, 0) == 1);
    assert(parse_catalog_json(app, pkg, "Source Alpha", NULL, NULL, 0) == 1);
    assert(app->catalog_count == first+3);
    assert(parse_catalog_json(app, exfat, "Source Alpha", NULL, NULL, 0) == 1);
    assert(app->catalog_count == first+3); /* Re-sync updates only its own package. */
    GameItem *alpha = &app->catalog[first], *beta = &app->catalog[first+1];
    app->detail_open = false;
    app->active_row = VIEW_SETTINGS;
    app->sidebar_focus = false;
    app->search_query[0] = '\0';
    search_edit(app, "SettingsGame", false);
    const char *names[MAX_CATALOG_ITEMS*2];
    int count = settings_sources(app, names), alpha_index = -1, beta_index = -1;
    for (int i = 0; i < count; i++) {
        if (strcmp(names[i], "Source Alpha") == 0) alpha_index = i+1;
        if (strcmp(names[i], "Source Beta") == 0) beta_index = i+1;
    }
    assert(alpha_index > 0 && beta_index > 0);
    app->settings_column = 0; app->settings_selection = 0;
    settings_toggle(app); /* All sources -> none. */
    assert(app->filters.sources_limited && !app->filters.source_count && !app->search_count);
    app->settings_selection = beta_index;
    settings_toggle(app);
    assert(app->search_count == 1 && app->search_results[0] == beta);
    assert(app->rows[VIEW_HOME].item_count == 1 && app->rows[VIEW_HOME].items[0] == beta);
    app->settings_selection = alpha_index;
    settings_toggle(app);
    assert(settings_source_enabled(app, "Source Alpha") && settings_source_enabled(app, "Source Beta"));
    assert(app->filters.source_count == 2 && app->search_count == 1);
    ui_open_game(app, alpha);
    GameItem *entries[MAX_CATALOG_ITEMS];
    assert(game_entries(app, entries) == 3);
    app->detail_open = false;
    app->active_row = VIEW_SETTINGS;
    /* Exclude folder; exfat and pkg stay selected together. */
    app->settings_column = 1; app->settings_selection = 2;
    settings_toggle(app);
    assert(settings_accepts_item(app, alpha) && !settings_accepts_item(app, beta));
    assert(settings_accepts_item(app, &app->catalog[first+2]));
    assert(app->search_results[0] != beta);
    app->settings_selection = 0;
    settings_toggle(app); /* Partial -> all. */
    assert(!app->filters.disabled_formats);
    settings_toggle(app); /* All -> none, never silently reset to All. */
    assert(app->filters.disabled_formats == 7 && !app->search_count);
    SDL_Event event;
    SDL_zero(event);
    event.type = SDL_KEYDOWN;
    event.key.keysym.sym = SDLK_DOWN;
    ui_handle_input(app, &event);
    assert(app->settings_selection == 1);
    event.key.keysym.sym = SDLK_SPACE;
    ui_handle_input(app, &event);
    assert(app->filters.disabled_formats == 6 && settings_accepts_item(app, alpha));
    event.type = SDL_CONTROLLERBUTTONDOWN;
    event.cbutton.button = SDL_CONTROLLER_BUTTON_DPAD_DOWN;
    ui_handle_input(app, &event);
    event.cbutton.button = SDL_CONTROLLER_BUTTON_A;
    ui_handle_input(app, &event);
    assert(app->filters.disabled_formats == 4 && settings_accepts_item(app, beta));
    /* Multiple source names and mixed formats round-trip, including Unicode. */
    SDL_strlcpy(app->filters.sources[1], "A source\n中文", 64);
    assert(settings_save(app));
    memset(&app->filters, 0, sizeof(app->filters));
    assert(settings_load(app) && app->filters.disabled_formats == 4 && app->filters.source_count == 2);
    assert(strcmp(app->filters.sources[1], "A source\n中文") == 0);
    /* Existing single-select preferences migrate without changing their meaning. */
    FILE *legacy = fopen(app->settings_path, "wb");
    assert(legacy); fputs("WOLODRIVE_FILTERS_1\n2\n11\nSource Beta", legacy); fclose(legacy);
    assert(settings_load(app) && app->filters.disabled_formats == 5 && app->filters.source_count == 1);
    assert(settings_source_enabled(app, "Source Beta") && !settings_source_enabled(app, "Source Alpha"));
    FILE *bad = fopen(app->settings_path, "wb");
    assert(bad); fputs("WOLODRIVE_FILTERS_2\n99\n4000\n", bad); fclose(bad);
    assert(!settings_load(app) && !settings_filters_active(app));
    /* Missing saved sources remain individually visible and can be unchecked. */
    app->filters.sources_limited = true; app->filters.source_count = 1;
    SDL_strlcpy(app->filters.sources[0], "Unavailable source", 64);
    count = settings_sources(app, names);
    assert(strcmp(names[count-1], "Unavailable source") == 0);
    app->settings_column = 0; app->settings_selection = count;
    ui_rebuild_carousels(app);
    assert(!app->search_count);
    settings_toggle(app);
    assert(!app->filters.source_count && app->filters.sources_limited);
    /* Source list paging and direct mouse checkboxes use the displayed rows. */
    for (int i = 0; i < 12; i++) {
        GameItem *item = &app->catalog[app->catalog_count++];
        snprintf(item->title, sizeof(item->title), "Paging fixture %d", i);
        snprintf(item->source_name, sizeof(item->source_name), "Extra source %d", i);
    }
    count = settings_sources(app, names);
    app->settings_selection = 9;
    ui_render(app);
    event.type = SDL_MOUSEBUTTONDOWN;
    event.button.button = SDL_BUTTON_LEFT;
    event.button.x = settings_target(0, 2).x+25;
    event.button.y = settings_target(0, 2).y+25;
    char clicked_source[64]; SDL_strlcpy(clicked_source, names[8], 64);
    ui_handle_input(app, &event);
    assert(settings_source_enabled(app, clicked_source));
    event.button.x = settings_target(1, 4).x+25;
    event.button.y = settings_target(1, 4).y+25;
    ui_handle_input(app, &event);
    assert(!settings_filters_active(app) && app->search_count == 1);
    ui_render(app);
    if (getenv("WOLODRIVE_TEST_SCREENSHOTS")) {
        int w, h;
        SDL_GetRendererOutputSize(app->renderer, &w, &h);
        SDL_Surface *surface = SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_RGBA32);
        assert(surface && SDL_RenderReadPixels(app->renderer, NULL, surface->format->format, surface->pixels, surface->pitch) == 0);
        assert(SDL_SaveBMP(surface, "build/settings-preview.bmp") == 0);
        SDL_FreeSurface(surface);
    }
    assert(remove(app->settings_path) == 0);
    event.type = SDL_CONTROLLERBUTTONDOWN;
    event.cbutton.button = SDL_CONTROLLER_BUTTON_DPAD_LEFT;
    ui_handle_input(app, &event);
    assert(app->settings_column == 0 && !app->sidebar_focus);
    ui_handle_input(app, &event);
    assert(app->sidebar_focus);
    event.cbutton.button = SDL_CONTROLLER_BUTTON_DPAD_UP;
    ui_handle_input(app, &event);
    assert(app->active_row == VIEW_DOWNLOADS);
    SDL_strlcpy(app->settings_path, original_path, sizeof(app->settings_path));
    puts("Settings checkboxes: multiple/none/all sources and formats, grouping, input, paging, persistence, legacy migration and reset passed.");
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
    assert(app->rows[0].selected_col == 2 && app->detail_open && !app->toast_message[0]);
    app->detail_open = false;
    ui_update_animations(app, 1.0f);
    event.button.x = CONTENT_X + 20;
    event.button.y = CARDS_Y + CARD_HEIGHT + 20;
    ui_handle_input(app, &event);
    assert(app->detail_open && !app->toast_message[0]);
    assert(get_focused_item(app)->state == STATE_AVAILABLE);
    app->detail_open = false;

    /* Content arrows never switch sections; only Left at the edge enters the rail. */
    event.type = SDL_KEYDOWN;
    event.key.keysym.sym = SDLK_DOWN;
    ui_handle_input(app, &event);
    assert(app->active_row == VIEW_HOME && !app->sidebar_focus);
    event.key.keysym.sym = SDLK_LEFT;
    ui_handle_input(app, &event);
    ui_handle_input(app, &event);
    assert(!app->sidebar_focus && app->rows[VIEW_HOME].selected_col == 0);
    ui_handle_input(app, &event);
    assert(app->sidebar_focus);
    event.key.keysym.sym = SDLK_RIGHT;
    ui_handle_input(app, &event);
    assert(!app->sidebar_focus && app->rows[VIEW_HOME].selected_col == 0);
    event.type = SDL_CONTROLLERBUTTONDOWN;
    event.cbutton.button = SDL_CONTROLLER_BUTTON_DPAD_DOWN;
    ui_handle_input(app, &event);
    assert(app->active_row == VIEW_HOME && !app->sidebar_focus);
    event.cbutton.button = SDL_CONTROLLER_BUTTON_DPAD_LEFT;
    ui_handle_input(app, &event);
    assert(app->sidebar_focus);
    app->rows[VIEW_HOME].selected_col = 2; /* Selection persists while using the rail. */
    /* All six views remain reachable, including the unimplemented sections. */
    event.type = SDL_CONTROLLERBUTTONDOWN;
    event.cbutton.button = SDL_CONTROLLER_BUTTON_DPAD_DOWN;
    for (int r = 1; r <= VIEW_DOWNLOADS; r++) {
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
    assert(library_company_count() == 10);
    assert(!library_company_at(-1) && !library_company_at(10));
    const int system_counts[] = {6, 2, 8, 14, 5, 4, 4, 2, 8, 5};
    int system_total = 0;
    for (int i = 0; i < library_company_count(); i++) {
        assert(library_company_at(i)->console_count == system_counts[i]);
        system_total += library_company_at(i)->console_count;
    }
    assert(system_total == 58);
    assert(strstr(library_console_at(0, 2)->name, "experimental"));
    assert(strstr(library_console_at(1, 1)->name, "experimental"));
    assert(!library_console_at(0, -1) && !library_console_at(0, 99));
    assert(library_console_matches_id(library_console_at(0, 3), "cusa12345"));
    assert(!library_console_matches_id(library_console_at(0, 3), "PPSA12345"));
    event.button.x = 50;
    event.button.y = sidebar_target(VIEW_GAMES).y + 40;
    ui_handle_input(app, &event);
    assert(app->library_depth == LIBRARY_COMPANIES && app->sidebar_focus);
    assert(!get_focused_item(app));
    ui_render(app);
    SDL_zero(event);
    event.type = SDL_KEYDOWN;
    event.key.keysym.sym = SDLK_RIGHT;
    ui_handle_input(app, &event);
    assert(!app->sidebar_focus);
    event.key.keysym.sym = SDLK_UP;
    ui_handle_input(app, &event);
    assert(!app->sidebar_focus && app->active_row == VIEW_GAMES);
    event.key.keysym.sym = SDLK_DOWN;
    ui_handle_input(app, &event);
    assert(!app->sidebar_focus && app->active_row == VIEW_GAMES);
    event.key.keysym.sym = SDLK_UP;
    ui_handle_input(app, &event);
    assert(app->library_company_selection == 0);
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
    assert(app->sidebar_focus);
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
        app->library_company_selection = c; /* Select that page before clicking. */
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
    /* New categories are reachable on the second page using keyboard input. */
    app->library_company_selection = 4;
    SDL_zero(event);
    event.type = SDL_KEYDOWN;
    event.key.keysym.sym = SDLK_DOWN;
    ui_handle_input(app, &event);
    assert(app->library_company_selection == 8);
    ui_render(app);
    event.key.keysym.sym = SDLK_RETURN;
    ui_handle_input(app, &event);
    assert(app->library_depth == LIBRARY_CONSOLES && app->library_company == 8);
    assert(library_company_at(8)->console_count == 8);
    event.key.keysym.sym = SDLK_ESCAPE;
    ui_handle_input(app, &event);
    app->library_company_selection = 3;
    SDL_Rect nintendo_tile = library_tile_target(3);
    SDL_zero(event);
    event.type = SDL_MOUSEBUTTONDOWN;
    event.button.button = SDL_BUTTON_LEFT;
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
    /* Emulators uses the same groups, pages and tiles, without exposing games. */
    select_sidebar_view(app, VIEW_EMULATORS);
    assert(app->library_depth == LIBRARY_COMPANIES && app->sidebar_focus);
    assert(strcmp(library_section_title(app), TR(app, TXT_NAV_EMULATORS)) == 0);
    ui_render(app);
    for (int c = 0; c < library_company_count(); c++) {
        app->library_company_selection = c;
        SDL_Rect tile = library_tile_target(c);
        event.button.x = tile.x + 40;
        event.button.y = tile.y + 40;
        ui_handle_input(app, &event);
        assert(app->library_depth == LIBRARY_CONSOLES && app->library_company == c);
        ui_render(app);
        for (int i = 0; i < library_company_at(c)->console_count; i++) {
            app->library_console_selection = i;
            tile = library_tile_target(i);
            event.button.x = tile.x + 40;
            event.button.y = tile.y + 40;
            ui_handle_input(app, &event);
            assert(app->library_depth == LIBRARY_GAMES);
            assert(app->active_row == VIEW_EMULATORS && !get_focused_item(app));
            assert(!app->rows[VIEW_EMULATORS].item_count);
            assert(strcmp(view_content_title(app), library_console_at(c, i)->name) == 0);
            assert(strcmp(empty_view_message(app), TR(app, TXT_EMULATOR_NOT_CONNECTED)) == 0);
            if (i == 0) ui_render(app);
            event.button.x = LIBRARY_BACK_BUTTON.x + 20;
            event.button.y = LIBRARY_BACK_BUTTON.y + 20;
            ui_handle_input(app, &event);
            assert(app->library_depth == LIBRARY_CONSOLES && app->library_console_selection == i);
        }
        ui_handle_input(app, &event); /* Back to the group page. */
        assert(app->library_depth == LIBRARY_COMPANIES);
    }
    /* Keyboard/controller paging also works in Emulators. */
    app->library_company_selection = 4;
    SDL_zero(event);
    event.type = SDL_KEYDOWN;
    event.key.keysym.sym = SDLK_DOWN;
    ui_handle_input(app, &event);
    assert(app->library_company_selection == 8);
    event.key.keysym.sym = SDLK_RETURN;
    ui_handle_input(app, &event);
    assert(app->library_depth == LIBRARY_CONSOLES && app->library_company == 8);
    SDL_zero(event);
    event.type = SDL_CONTROLLERBUTTONDOWN;
    event.cbutton.button = SDL_CONTROLLER_BUTTON_B;
    ui_handle_input(app, &event);
    assert(app->library_depth == LIBRARY_COMPANIES);
    ui_handle_input(app, &event);
    assert(app->sidebar_focus);
    event.cbutton.button = SDL_CONTROLLER_BUTTON_DPAD_UP;
    ui_handle_input(app, &event);
    assert(app->active_row == VIEW_GAMES && app->library_depth == LIBRARY_COMPANIES);
    assert(strcmp(library_section_title(app), TR(app, TXT_NAV_GAMES)) == 0);
    puts("Emulators browses all 58 systems with shared cards, paging, back navigation and isolated empty sources.");
    /* Every tile has a decodable local asset; renderer-owned textures are reused. */
    for (int c = 0; c < library_company_count(); c++) {
        const LibraryCompany *brand = library_company_at(c);
        LibraryArtwork *logo = load_library_artwork(app, brand->artwork_path);
        assert(!brand->artwork_path || (logo && logo->texture && logo->width > 0 && logo->height > 0));
        for (int i = 0; i < brand->console_count; i++) {
            LibraryArtwork *art = load_library_artwork(app, brand->consoles[i].artwork_path);
            assert(!brand->consoles[i].artwork_path || (art && art->texture && art->width > 0 && art->height > 0));
            LibraryArtwork *mark = load_library_artwork(app, brand->consoles[i].logo_path);
            assert(!brand->consoles[i].logo_path || (mark && mark->texture && mark->source_bounds.w > 0 && mark->source_bounds.h > 0));
            if (!brand->consoles[i].artwork_path) continue; /* Arcade uses logos. */
            SDL_Surface *original = IMG_Load(brand->consoles[i].artwork_path);
            assert(original);
            SDL_Surface *rgba = SDL_ConvertSurfaceFormat(original, SDL_PIXELFORMAT_RGBA32, 0);
            assert(rgba);
            bool transparent = false, opaque = false;
            for (int y = 0; y < rgba->h; y++) {
                for (int x = 0; x < rgba->w; x++) {
                    Uint8 alpha = *((Uint8 *)rgba->pixels + y * rgba->pitch + x * 4 + 3);
                    transparent |= alpha == 0;
                    opaque |= alpha == 255;
                }
            }
            assert(transparent && opaque); /* Native cutout, no runtime white key. */
            SDL_FreeSurface(rgba);
            SDL_FreeSurface(original);
        }
    }
    assert(app->library_artwork_count == 118);
    const char *logo_path = library_company_at(0)->artwork_path;
    LibraryArtwork *logo = load_library_artwork(app, logo_path);
    assert(load_library_artwork(app, logo_path) == logo && app->library_artwork_count == 118);
    assert(!render_library_artwork(app, "assets/library/missing-test-image.png", (SDL_Rect){0, 0, 20, 20}));
    assert(app->library_artwork_count == 119);
    assert(!render_library_artwork(app, "assets/library/missing-test-image.png", (SDL_Rect){0, 0, 20, 20}));
    assert(app->library_artwork_count == 119);
    /* Resolve bundled assets relative to the executable if cwd is not the repo. */
    char working_directory[4096];
    assert(getcwd(working_directory, sizeof(working_directory)));
    assert(chdir("build") == 0);
    WolodriveApp *relocated = calloc(1, sizeof(*relocated));
    assert(relocated);
    relocated->renderer = app->renderer;
    LibraryArtwork *relocated_logo = load_library_artwork(relocated, logo_path);
    assert(relocated_logo && relocated_logo->texture);
    ui_cleanup(relocated);
    free(relocated);
    assert(chdir(working_directory) == 0);
    puts("All 118 artwork files for 58 systems decode, cache and resolve outside the repo.");
    puts("Company grids, console pages, PS4/PS5 filtering, empty platforms and keyboard/controller/mouse back navigation passed.");
    test_search(app);
    test_game_details(app);
    test_settings(app);
    ui_cleanup(app);
    SDL_DestroyRenderer(app->renderer);
    SDL_DestroyWindow(app->window);
    SDL_DestroyMutex(app->catalog_mutex);
    free(app);
    IMG_Quit();
    TTF_Quit();
    puts("Six labeled sidebar views, keyboard/controller/mouse navigation, mixed game formats, action footer and empty states passed.");
}

static void test_native_artwork_bounds(void) {
    SDL_Surface *image = SDL_CreateRGBSurfaceWithFormat(0, 5, 5, 32, SDL_PIXELFORMAT_RGBA32);
    assert(image);
    SDL_FillRect(image, NULL, SDL_MapRGBA(image->format, 0, 0, 0, 0));
    SDL_Rect body = {1, 1, 3, 3}, panel_rect = {2, 2, 1, 1};
    SDL_FillRect(image, &body, SDL_MapRGBA(image->format, 0, 0, 0, 255));
    SDL_FillRect(image, &panel_rect, SDL_MapRGBA(image->format, 255, 255, 255, 255));
    SDL_Rect bounds = library_alpha_bounds(image);
    assert(bounds.x == 1 && bounds.y == 1 && bounds.w == 3 && bounds.h == 3);
    Uint8 *white_panel = (Uint8 *)image->pixels + 2 * image->pitch + 2 * 4;
    Uint8 *black_body = (Uint8 *)image->pixels + image->pitch + 4;
    assert(white_panel[0] == 255 && white_panel[3] == 255);
    assert(black_body[0] == 0 && black_body[3] == 255);
    prepare_library_logo(image, "assets/library/logos/test.png");
    assert(black_body[0] == 245 && black_body[3] == 255);
    assert(((Uint8 *)image->pixels)[3] == 0);
    /* Company colors and opaque internal spaces must survive preparation. */
    const char *companies[] = {"microsoft", "sega", "nintendo", "atari", "snk"};
    for (size_t i = 0; i < sizeof(companies) / sizeof(companies[0]); i++) {
        char path[128];
        snprintf(path, sizeof(path), "assets/library/companies/%s.png", companies[i]);
        SDL_Surface *source = IMG_Load(path);
        assert(source);
        SDL_Surface *rgba = SDL_ConvertSurfaceFormat(source, SDL_PIXELFORMAT_RGBA32, 0);
        assert(rgba);
        size_t bytes = (size_t)rgba->pitch * rgba->h;
        void *original = malloc(bytes);
        assert(original);
        memcpy(original, rgba->pixels, bytes);
        prepare_library_logo(rgba, path);
        assert(memcmp(original, rgba->pixels, bytes) == 0);
        free(original);
        SDL_FreeSurface(rgba);
        SDL_FreeSurface(source);
    }
    SDL_FillRect(image, NULL, SDL_MapRGBA(image->format, 0, 0, 0, 255));
    prepare_library_logo(image, "assets/library/companies/sony.png");
    assert(((Uint8 *)image->pixels)[0] == 245);
    SDL_FreeSurface(image);
    puts("Native-alpha hardware, original company colors and readable monochrome marks passed.");
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
    test_native_artwork_bounds();
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
