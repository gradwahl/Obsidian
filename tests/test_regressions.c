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
    assert(app->rows[3].item_count == 0);
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

int main(void) {
    assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
    test_description_text();
    test_truthful_catalog_state();
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
