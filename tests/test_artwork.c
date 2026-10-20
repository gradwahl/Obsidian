/* Exercise worker/cache lifecycle with a local HTTP fixture, never a public CDN. */
#include <sys/stat.h>
static unsigned filesystem_probes;
static int counted_stat(const char *path, struct stat *info) {
    filesystem_probes++;
    return stat(path, info);
}
#define stat(path, info) counted_stat(path, info)
#include "../src/artwork.c"
#undef stat
#include <assert.h>
#include <unistd.h>

static SDL_Texture *wait_art(WolodriveApp *app, GameItem *item, bool backdrop, int *w, int *h) {
    Uint32 start = SDL_GetTicks();
    SDL_Texture *texture;
    do {
        texture = artwork_get(app, item, backdrop, 100, w, h);
        if (texture) return texture;
        SDL_Delay(10);
    } while (SDL_GetTicks()-start < 4000);
    return NULL;
}

static void expire_locations(WolodriveApp *app) {
    for (int i = 0; i < 128; i++) app->artwork->locations[i].checked -= 1000;
}

static void test_scroll_queue(SDL_Renderer *renderer) {
    WolodriveApp *app = calloc(1, sizeof(*app));
    assert(app);
    app->renderer = renderer;
    ART_MKDIR("assets"); ART_MKDIR("assets/artwork");
    SDL_Surface *fixture = SDL_CreateRGBSurfaceWithFormat(0, 37, 51, 32, SDL_PIXELFORMAT_RGBA32);
    assert(fixture);
    GameItem old_selection[3], visible = {0};
    memset(old_selection, 0, sizeof(old_selection));
    for (int i = 0; i < 3; i++) {
        char path[128];
        snprintf(path, sizeof(path), "assets/artwork/scroll-%d.png", i);
        assert(IMG_SavePNG(fixture, path) == 0);
        snprintf(old_selection[i].cover_url, sizeof(old_selection[i].cover_url), "local:%s", path);
        artwork_prefetch(app, &old_selection[i], 110);
    }
    assert(IMG_SavePNG(fixture, "assets/artwork/scroll-visible.png") == 0);
    SDL_FreeSurface(fixture);
    SDL_strlcpy(visible.cover_url, "local:assets/artwork/scroll-visible.png", sizeof(visible.cover_url));
    int pending = 0;
    for (int tries = 0; tries < 200 && pending < 3; tries++) {
        SDL_Delay(5); pending = 0;
        SDL_LockMutex(app->artwork->mutex);
        for (int i = 0; i < ART_SLOTS; i++) if (app->artwork->entries[i].surface) pending++;
        SDL_UnlockMutex(app->artwork->mutex);
    }
    assert(pending == 3);
    /* Three old hero images should not block a current lower-priority cover. */
    Uint32 start = SDL_GetTicks();
    SDL_Texture *texture = NULL;
    int w = 0, h = 0;
    do {
        texture = artwork_get(app, &visible, false, 80, &w, &h);
        if (!texture) SDL_Delay(5);
    } while (!texture && SDL_GetTicks()-start < 500);
    assert(texture && w == 37 && h == 51);
    printf("Visible cached cover replaced stale hero prefetch in %u ms.\n", SDL_GetTicks()-start);
    /* Discarding decoded work is not a load failure or a reason to delay reuse. */
    SDL_LockMutex(app->artwork->mutex);
    for (int i = 0; i < ART_SLOTS; i++) {
        ArtworkEntry *entry = &app->artwork->entries[i];
        assert(!entry->failed);
    }
    SDL_UnlockMutex(app->artwork->mutex);
    for (int old = 0; old < 3; old++) {
        start = SDL_GetTicks(); texture = NULL;
        do {
            texture = artwork_get(app, &old_selection[old], false, 100, &w, &h);
            if (!texture) SDL_Delay(5);
        } while (!texture && SDL_GetTicks()-start < 500);
        assert(texture && w == 37 && h == 51);
    }
    SDL_LockMutex(app->artwork->mutex);
    int reserved = 0;
    for (int i = 0; i < ART_SLOTS; i++) {
        ArtworkEntry *entry = &app->artwork->entries[i];
        reserved += (entry->surface != NULL) + entry->busy;
    }
    assert(reserved <= 3 && app->artwork->bytes <= ART_BUDGET);
    SDL_UnlockMutex(app->artwork->mutex);
    artwork_destroy(app);
    free(app);
}

int main(void) {
    assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
    IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG | IMG_INIT_WEBP);
    curl_global_init(CURL_GLOBAL_DEFAULT);
    const char *root = getenv("WOLODRIVE_ARTWORK_TEST_DIR");
    const char *server = getenv("WOLODRIVE_ARTWORK_TEST_URL");
    assert(root && server && chdir(root) == 0);
    WolodriveApp *app = calloc(1, sizeof(*app));
    SDL_Surface *screen = SDL_CreateRGBSurfaceWithFormat(0, 640, 360, 32, SDL_PIXELFORMAT_RGBA32);
    app->renderer = SDL_CreateSoftwareRenderer(screen);
    assert(app->renderer);
    test_scroll_queue(app->renderer);
    /* A source returning HTML must advance through failed candidates to an
       independently matched cover, rather than stopping at a placeholder. */
    GameItem protected_source = {0};
    snprintf(protected_source.cover_url, sizeof(protected_source.cover_url), "%s/challenge", server);
    snprintf(protected_source.icon_url, sizeof(protected_source.icon_url), "%s/bad-source", server);
    snprintf(protected_source.cover_fallback_url, sizeof(protected_source.cover_fallback_url), "%s/independent", server);
    int fallback_w = 0, fallback_h = 0;
    assert(wait_art(app, &protected_source, false, &fallback_w, &fallback_h));
    assert(fallback_w == 160 && fallback_h == 90);
    SDL_Rect fit = artwork_fit(200, 400, (SDL_Rect){10,20,320,180}, false);
    assert(fit.x == 125 && fit.y == 20 && fit.w == 90 && fit.h == 180);
    fit = artwork_fit(400,400,(SDL_Rect){0,0,320,180},false);
    assert(fit.x == 70 && fit.w == 180 && fit.h == 180);
    GameItem item = {0};
    SDL_strlcpy(item.id_code, "PPSA12345", sizeof(item.id_code));
    snprintf(item.cover_url, sizeof(item.cover_url), "%s/good", server);
    snprintf(item.icon_url,sizeof(item.icon_url),"%s/icon",server);
    ArtworkEntry location = {0};
    artwork_location(&item, false, &location);
    assert(strcmp(location.url,item.cover_url) == 0);
    ART_MKDIR("assets"); ART_MKDIR(COVERS_CACHE_DIR);
    FILE *bad = fopen(location.path, "wb"); assert(bad);
    fputs("corrupted cached image", bad); fclose(bad);
    int w = 0, h = 0;
    SDL_Texture *first = wait_art(app, &item, false, &w, &h);
    assert(first && w == 80 && h == 120);
    unsigned before = filesystem_probes;
    for (int i = 0; i < 100; i++) artwork_prefetch(app, &item, 10);
    assert(filesystem_probes == before);
    GameItem missing = {0};
    SDL_strlcpy(missing.id_code, "NO_ARTWORK", sizeof(missing.id_code));
    artwork_prefetch(app, &missing, 10);
    before = filesystem_probes;
    for (int i = 0; i < 100; i++) artwork_prefetch(app, &missing, 10);
    assert(filesystem_probes == before); /* Cache absent artwork too. */
    expire_locations(app);
    artwork_prefetch(app, &missing, 10);
    assert(filesystem_probes > before);
    bool media_failed = false;
    assert(artwork_get_media(app,item.cover_url,100,&w,&h,&media_failed) == first && !media_failed);
    GameItem variant = item;
    SDL_strlcpy(variant.id_code, "PPSA54321", sizeof(variant.id_code));
    assert(artwork_get(app,&variant,false,80,&w,&h) == first);
    SDL_Texture *soft = wait_art(app,&item,true,&w,&h);
    assert(soft && soft != first && w <= 48 && h <= 27);
    item.cover_url[0] = item.icon_url[0] = 0;
    snprintf(item.landscape_url,sizeof(item.landscape_url),"%s/changed",server);
    SDL_Texture *changed = wait_art(app,&item,false,&w,&h);
    assert(changed && changed != first && w == 160 && h == 90);
    snprintf(item.backdrop_url,sizeof(item.backdrop_url),"%s/hero",server);
    assert(wait_art(app,&item,true,&w,&h) && w == 1920 && h == 1080);
    snprintf(item.boxart_url,sizeof(item.boxart_url),"%s/boxart",server);
    assert(wait_art(app,&item,false,&w,&h) && w == 200 && h == 280);
    SDL_Texture *complete = artwork_get(app,&item,false,100,&w,&h);
    assert(artwork_complete_box(app,complete));
    item.boxart_url[0] = 0;
    assert(artwork_get(app,&item,false,100,&w,&h) == changed);
    SDL_Surface *local = SDL_CreateRGBSurfaceWithFormat(0,50,50,32,SDL_PIXELFORMAT_RGBA32);
    SDL_FillRect(local,NULL,SDL_MapRGBA(local->format,230,40,80,255));
    assert(IMG_SavePNG(local,"assets/artwork/PPSA12345.card.png") == 0);
    expire_locations(app);
    assert(wait_art(app,&item,false,&w,&h) && w == 50 && h == 50);
    Uint32 media_start = SDL_GetTicks();
    SDL_Texture *local_media = NULL;
    while (!local_media && SDL_GetTicks()-media_start < 2000) {
        local_media = artwork_get_media(app,"assets/artwork/PPSA12345.card.png",100,&w,&h,&media_failed);
        SDL_Delay(10);
    }
    assert(local_media && !media_failed && w == 50 && h == 50);
    assert(remove("assets/artwork/PPSA12345.card.png") == 0);
    assert(IMG_SavePNG(local,"assets/artwork/PPSA12345.box.png") == 0);
    expire_locations(app);
    complete = wait_art(app,&item,false,&w,&h);
    assert(complete && artwork_complete_box(app,complete));
    assert(remove("assets/artwork/PPSA12345.box.png") == 0);
    expire_locations(app);
    assert(artwork_get(app,&item,false,80,&w,&h) == changed);
    SDL_strlcpy(item.id_code,"../escape",sizeof(item.id_code));
    artwork_location(&item,false,&location); assert(!location.local);
    SDL_FreeSurface(local);
    /* More full-size images than the GPU budget: old textures must be evicted. */
    for (int i = 0; i < 10; i++) {
        snprintf(item.landscape_url,sizeof(item.landscape_url),"%s/large%d",server,i);
        assert(wait_art(app,&item,false,&w,&h));
        assert(app->artwork->bytes <= ART_BUDGET);
    }
    assert(app->artwork->bytes < 10u*1920u*1080u*4u);
    /* Three decoded prefetches must not starve a new higher-priority request. */
    for (int i = 0; i < 3; i++) {
        snprintf(item.landscape_url,sizeof(item.landscape_url),"%s/nearby%d",server,i);
        artwork_prefetch(app,&item,10);
    }
    int pending = 0;
    for (int tries = 0; tries < 200 && pending < 3; tries++) {
        SDL_Delay(10); pending = 0;
        SDL_LockMutex(app->artwork->mutex);
        for (int i = 0; i < ART_SLOTS; i++) if (app->artwork->entries[i].surface) pending++;
        SDL_UnlockMutex(app->artwork->mutex);
    }
    assert(pending == 3);
    snprintf(item.landscape_url,sizeof(item.landscape_url),"%s/visible",server);
    assert(wait_art(app,&item,false,&w,&h));
    snprintf(item.landscape_url,sizeof(item.landscape_url),"%s/retry",server);
    item.cover_url[0] = 0;
    ArtworkEntry *entry = artwork_request(app,&item,false,100);
    bool failed = false;
    for (int i = 0; i < 200 && !failed; i++) {
        SDL_Delay(10); SDL_LockMutex(app->artwork->mutex);
        failed = entry->failed; SDL_UnlockMutex(app->artwork->mutex);
    }
    assert(failed);
    SDL_LockMutex(app->artwork->mutex);
    entry->retry = SDL_GetTicks();
    SDL_UnlockMutex(app->artwork->mutex);
    assert(wait_art(app,&item,false,&w,&h));
    /* A file imported during network retry cooldown must load from disk. */
    GameItem imported = {0};
    snprintf(imported.cover_url, sizeof(imported.cover_url), "%s/bad-until-import", server);
    ArtworkEntry *waiting = artwork_request(app, &imported, false, 110);
    bool waiting_failed = false;
    for (int i = 0; i < 300 && !waiting_failed; i++) {
        SDL_Delay(5);
        SDL_LockMutex(app->artwork->mutex); waiting_failed = waiting->failed; SDL_UnlockMutex(app->artwork->mutex);
    }
    assert(waiting_failed);
    ArtworkEntry imported_location = {0};
    artwork_location(&imported, false, &imported_location);
    SDL_Surface *import_fixture = SDL_CreateRGBSurfaceWithFormat(0, 39, 53, 32, SDL_PIXELFORMAT_RGBA32);
    assert(import_fixture && IMG_SavePNG(import_fixture, imported_location.path) == 0);
    SDL_FreeSurface(import_fixture);
    Uint32 import_start = SDL_GetTicks();
    assert(wait_art(app, &imported, false, &w, &h) && w == 39 && h == 53);
    assert(SDL_GetTicks() - import_start < 1500);
    printf("Imported cover bypassed network retry cooldown in %u ms.\n", SDL_GetTicks() - import_start);
    /* A slow network image must not hold cached covers behind it. */
    GameItem slow = {0}, cached = {0};
    snprintf(slow.cover_url, sizeof(slow.cover_url), "%s/stall", server);
    ArtworkEntry *blocked = artwork_request(app, &slow, false, 110);
    bool busy = false;
    for (int i = 0; i < 200 && !busy; i++) {
        SDL_Delay(5);
        SDL_LockMutex(app->artwork->mutex); busy = blocked->busy && blocked->disk_tried; SDL_UnlockMutex(app->artwork->mutex);
    }
    assert(busy);
    SDL_Delay(50);
    snprintf(cached.cover_url, sizeof(cached.cover_url), "%s/cached-during-stall", server);
    ArtworkEntry cached_location = {0};
    artwork_location(&cached, false, &cached_location);
    SDL_Surface *fixture = SDL_CreateRGBSurfaceWithFormat(0, 37, 51, 32, SDL_PIXELFORMAT_RGBA32);
    assert(fixture && IMG_SavePNG(fixture, cached_location.path) == 0);
    SDL_FreeSurface(fixture);
    Uint32 cached_start = SDL_GetTicks();
    SDL_Texture *ready = NULL;
    do {
        ready = artwork_get(app, &cached, false, 100, &w, &h);
        if (!ready) SDL_Delay(5);
    } while (!ready && SDL_GetTicks() - cached_start < 500);
    assert(ready && w == 37 && h == 51);
    printf("Cached cover loaded in %u ms during a 2-second network stall.\n", SDL_GetTicks() - cached_start);
    artwork_destroy(app); assert(!app->artwork);
    SDL_DestroyRenderer(app->renderer); SDL_FreeSurface(screen); free(app);
    curl_global_cleanup(); IMG_Quit(); SDL_Quit();
    puts("Artwork: fit, URL invalidation, shared variants, soft/separate backgrounds, local overrides, corrupt-cache recovery, retry, eviction and joined shutdown passed.");
    return 0;
}
