/* Exercise worker/cache lifecycle with a local HTTP fixture, never a public CDN. */
#include "../src/artwork.c"
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
    assert(wait_art(app,&item,false,&w,&h) && w == 50 && h == 50);
    assert(remove("assets/artwork/PPSA12345.card.png") == 0);
    assert(IMG_SavePNG(local,"assets/artwork/PPSA12345.box.png") == 0);
    complete = wait_art(app,&item,false,&w,&h);
    assert(complete && artwork_complete_box(app,complete));
    assert(remove("assets/artwork/PPSA12345.box.png") == 0);
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
    artwork_destroy(app); assert(!app->artwork);
    SDL_DestroyRenderer(app->renderer); SDL_FreeSurface(screen); free(app);
    curl_global_cleanup(); IMG_Quit(); SDL_Quit();
    puts("Artwork: fit, URL invalidation, shared variants, soft/separate backgrounds, local overrides, corrupt-cache recovery, retry, eviction and joined shutdown passed.");
    return 0;
}
