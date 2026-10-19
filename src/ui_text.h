/* UI-thread-only, renderer-owned text cache. Keys include all raster inputs. */
#define UI_TEXT_SLOTS 128
#define UI_TEXT_BUDGET (16u * 1024u * 1024u)
typedef struct {
    TTF_Font *font;
    char *text;
    SDL_Color color;
    int wrap, w, h;
    Uint64 used;
    SDL_Texture *texture;
} UiTextEntry;

struct UiTextCache {
    UiTextEntry entries[UI_TEXT_SLOTS];
    size_t bytes;
    Uint64 clock;
};

static void text_release(struct UiTextCache *cache, UiTextEntry *entry) {
    if (entry->texture) cache->bytes -= (size_t)entry->w * entry->h * 4;
    SDL_DestroyTexture(entry->texture);
    SDL_free(entry->text);
    memset(entry, 0, sizeof(*entry));
}

static void text_cache_destroy(WolodriveApp *app) {
    if (!app->text_cache) return;
    for (int i = 0; i < UI_TEXT_SLOTS; i++) text_release(app->text_cache, &app->text_cache->entries[i]);
    free(app->text_cache);
    app->text_cache = NULL;
}

static UiTextEntry *cached_text(WolodriveApp *app, TTF_Font *font, const char *text,
                                SDL_Color color, int wrap) {
    if (!font || !text || !text[0]) return NULL;
    if (!app->text_cache) app->text_cache = calloc(1, sizeof(*app->text_cache));
    struct UiTextCache *cache = app->text_cache;
    if (!cache) return NULL;
    UiTextEntry *oldest = &cache->entries[0];
    for (int i = 0; i < UI_TEXT_SLOTS; i++) {
        UiTextEntry *entry = &cache->entries[i];
        if (entry->texture && entry->font == font && entry->wrap == wrap &&
            entry->color.r == color.r && entry->color.g == color.g &&
            entry->color.b == color.b && entry->color.a == color.a && !strcmp(entry->text, text)) {
            entry->used = ++cache->clock;
            return entry;
        }
        if (entry->used < oldest->used) oldest = entry;
    }
    SDL_Surface *surface = wrap > 0 ? TTF_RenderUTF8_Blended_Wrapped(font, text, color, (Uint32)wrap)
                                  : TTF_RenderUTF8_Blended(font, text, color);
    if (!surface) return NULL;
    size_t bytes = (size_t)surface->w * surface->h * 4;
    if (bytes > UI_TEXT_BUDGET) { SDL_FreeSurface(surface); return NULL; }
    text_release(cache, oldest);
    while (cache->bytes && cache->bytes + bytes > UI_TEXT_BUDGET) {
        UiTextEntry *victim = NULL;
        for (int i = 0; i < UI_TEXT_SLOTS; i++) {
            UiTextEntry *entry = &cache->entries[i];
            if (entry->texture && (!victim || entry->used < victim->used)) victim = entry;
        }
        if (!victim) break;
        text_release(cache, victim);
    }
    oldest->texture = SDL_CreateTextureFromSurface(app->renderer, surface);
    oldest->text = SDL_strdup(text);
    oldest->w = surface->w;
    oldest->h = surface->h;
    SDL_FreeSurface(surface);
    if (oldest->texture) cache->bytes += bytes;
    if (!oldest->texture || !oldest->text) { text_release(cache, oldest); return NULL; }
    oldest->font = font;
    oldest->color = color;
    oldest->wrap = wrap;
    oldest->used = ++cache->clock;
    return oldest;
}
