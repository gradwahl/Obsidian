#include "artwork.h"
#include <curl/curl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <direct.h>
#define ART_MKDIR(path) _mkdir(path)
#else
#define ART_MKDIR(path) mkdir(path, 0777)
#endif

#define ART_SLOTS 64
#define ART_BUDGET (48u * 1024u * 1024u)
#ifdef __PS5__
#define ART_OVERRIDES WOLODRIVE_DATA_PATH "/artwork"
#else
#define ART_OVERRIDES "assets/artwork"
#endif

typedef struct {
    char key[768], url[512], path[256];
    SDL_Texture *texture;
    SDL_Surface *surface;
    SDL_Color color;
    int width, height, priority;
    Uint32 used, retry, priority_tick;
    bool busy, local, failed, soft, complete_box;
} ArtworkEntry;

struct ArtworkCache {
    SDL_mutex *mutex;
    SDL_cond *condition;
    SDL_Thread *thread;
    SDL_atomic_t stop;
    ArtworkEntry entries[ART_SLOTS];
    size_t bytes;
};

SDL_Rect artwork_fit(int width, int height, SDL_Rect box, bool crop) {
    if (width <= 0 || height <= 0) return (SDL_Rect){box.x, box.y, 0, 0};
    double sx = (double)box.w / width, sy = (double)box.h / height;
    double scale = crop ? SDL_max(sx, sy) : SDL_min(sx, sy);
    int w = SDL_max(1, (int)(width * scale)), h = SDL_max(1, (int)(height * scale));
    return (SDL_Rect){box.x + (box.w-w)/2, box.y + (box.h-h)/2, w, h};
}

static Uint64 artwork_hash(const char *url) {
    Uint64 hash = 14695981039346656037ULL;
    while (*url) { hash ^= (unsigned char)*url++; hash *= 1099511628211ULL; }
    return hash;
}

static void artwork_location(const GameItem *item, bool backdrop, ArtworkEntry *request) {
    const char *role = backdrop ? "backdrop" : "box";
    /* Only safe title IDs can select local overrides. */
    bool safe = item->id_code[0] != 0;
    for (const char *p = item->id_code; *p; p++)
        if (!((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') ||
              (*p >= '0' && *p <= '9') || *p == '-' || *p == '_')) safe = false;
    const char *extensions[] = {"png", "jpg", "webp"};
    struct stat st;
    if (safe) for (int fallback = 0; fallback < (backdrop ? 2 : 3); fallback++) {
        for (int i = 0; i < 3; i++) {
            if (!fallback) snprintf(request->path, sizeof(request->path), "%s/%s.%s.%s", ART_OVERRIDES, item->id_code, role, extensions[i]);
            else if (!backdrop && fallback == 1) snprintf(request->path, sizeof(request->path), "%s/%s.card.%s", ART_OVERRIDES, item->id_code, extensions[i]);
            else snprintf(request->path, sizeof(request->path), "%s/%s.%s", ART_OVERRIDES, item->id_code, extensions[i]);
            if (stat(request->path, &st) == 0 && st.st_size > 0) {
                request->local = true;
                request->complete_box = !backdrop && fallback == 0;
                snprintf(request->key, sizeof(request->key), "%s:%lld:%lld", request->path, (long long)st.st_mtime, (long long)st.st_size);
                if (request->complete_box) SDL_strlcat(request->key, ":box", sizeof(request->key));
                return;
            }
        }
    }
    const char *url;
    if (backdrop) url = item->backdrop_url[0] ? item->backdrop_url :
                        item->landscape_url[0] ? item->landscape_url :
                        item->cover_url[0] ? item->cover_url : item->icon_url;
    else url = item->boxart_url[0] ? item->boxart_url :
               item->cover_url[0] ? item->cover_url :
               item->icon_url[0] ? item->icon_url : item->landscape_url;
    request->complete_box = !backdrop && item->boxart_url[0];
    request->soft = backdrop && !item->backdrop_url[0] && !item->landscape_url[0];
    SDL_strlcpy(request->url, url, sizeof(request->url));
    if (!url[0]) { request->key[0] = 0; return; }
    SDL_strlcpy(request->key, url, sizeof(request->key));
    if (request->soft) SDL_strlcat(request->key, ":soft", sizeof(request->key));
    if (request->complete_box) SDL_strlcat(request->key, ":box", sizeof(request->key));
    snprintf(request->path, sizeof(request->path), "%s/%016llx.art", COVERS_CACHE_DIR, (unsigned long long)artwork_hash(url));
}

typedef struct { FILE *file; size_t bytes; } ArtworkDownload;
static size_t artwork_write(void *data, size_t size, size_t count, void *userdata) {
    ArtworkDownload *download = userdata;
    size_t bytes = size * count;
    if (bytes > 16u * 1024u * 1024u - download->bytes) return 0;
    size_t written = fwrite(data, 1, bytes, download->file);
    download->bytes += written;
    return written;
}
static int artwork_progress(void *userdata, curl_off_t a, curl_off_t b, curl_off_t c, curl_off_t d) {
    (void)a; (void)b; (void)c; (void)d;
    return SDL_AtomicGet(&((struct ArtworkCache *)userdata)->stop);
}

static SDL_Surface *artwork_load(struct ArtworkCache *cache, const ArtworkEntry *entry) {
    SDL_Surface *surface = IMG_Load(entry->path);
    if (surface || entry->local) return surface;
    if (strncmp(entry->url, "https://", 8) && strncmp(entry->url, "http://", 7)) return NULL;
    char temporary[280];
    snprintf(temporary, sizeof(temporary), "%s.tmp", entry->path);
    FILE *file = fopen(temporary, "wb");
    if (!file) return NULL;
    CURL *curl = curl_easy_init();
    ArtworkDownload download = {file, 0};
    CURLcode result = CURLE_FAILED_INIT;
    long status = 0;
    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, entry->url);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
#if LIBCURL_VERSION_NUM >= 0x075500
        curl_easy_setopt(curl, CURLOPT_PROTOCOLS_STR, "http,https");
        curl_easy_setopt(curl, CURLOPT_REDIR_PROTOCOLS_STR, "http,https");
#else
        curl_easy_setopt(curl, CURLOPT_PROTOCOLS, CURLPROTO_HTTP | CURLPROTO_HTTPS);
        curl_easy_setopt(curl, CURLOPT_REDIR_PROTOCOLS, CURLPROTO_HTTP | CURLPROTO_HTTPS);
#endif
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 5L);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 12L);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, artwork_write);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &download);
        curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, artwork_progress);
        curl_easy_setopt(curl, CURLOPT_XFERINFODATA, cache);
        curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
        result = curl_easy_perform(curl);
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
        curl_easy_cleanup(curl);
    }
    int closed = fclose(file);
    if (result == CURLE_OK && status >= 200 && status < 300 && closed == 0)
        surface = IMG_Load(temporary);
    if (surface) {
        /* Windows rename cannot replace an existing corrupt file. */
        remove(entry->path);
        if (rename(temporary, entry->path) != 0) remove(temporary);
    } else remove(temporary);
    return surface;
}

static SDL_Surface *artwork_resize(SDL_Surface *source, bool soft) {
    if (!source) return NULL;
    SDL_Rect fit = artwork_fit(source->w, source->h, soft ? (SDL_Rect){0,0,48,27} : (SDL_Rect){0,0,1920,1080}, false);
    if (source->w <= fit.w && source->h <= fit.h) {
        SDL_Surface *rgba = SDL_ConvertSurfaceFormat(source, SDL_PIXELFORMAT_RGBA32, 0);
        SDL_FreeSurface(source);
        return rgba;
    }
    SDL_Surface *small = SDL_CreateRGBSurfaceWithFormat(0, fit.w, fit.h, 32, SDL_PIXELFORMAT_RGBA32);
    if (small) {
        SDL_SetSurfaceBlendMode(source, SDL_BLENDMODE_NONE);
        if (SDL_BlitScaled(source, NULL, small, NULL) != 0) { SDL_FreeSurface(small); small = NULL; }
    }
    SDL_FreeSurface(source);
    return small;
}

static int artwork_worker(void *userdata) {
    struct ArtworkCache *cache = userdata;
    SDL_LockMutex(cache->mutex);
    while (!SDL_AtomicGet(&cache->stop)) {
        ArtworkEntry *next = NULL;
        Uint32 now = SDL_GetTicks();
        int pending = 0;
        for (int i = 0; i < ART_SLOTS; i++) {
            ArtworkEntry *entry = &cache->entries[i];
            if (entry->surface && now-entry->used > 2000) {
                SDL_FreeSurface(entry->surface); entry->surface = NULL;
            }
            if (entry->surface) pending++;
        }
        /* Bound decoded surfaces waiting for the UI to upload them as well. */
        for (int i = 0; i < ART_SLOTS; i++) {
            ArtworkEntry *entry = &cache->entries[i];
            if (!entry->key[0] || entry->texture || entry->surface || entry->busy ||
                (entry->failed && (Sint32)(now-entry->retry) < 0) || now-entry->used > 2000) continue;
            if (!next || entry->priority > next->priority ||
                (entry->priority == next->priority && entry->used > next->used)) next = entry;
        }
        if (!next) { SDL_CondWaitTimeout(cache->condition, cache->mutex, 100); continue; }
        if (pending >= 3) {
            ArtworkEntry *lowest = NULL;
            for (int i = 0; i < ART_SLOTS; i++) {
                ArtworkEntry *entry = &cache->entries[i];
                if (entry->surface && (!lowest || entry->priority < lowest->priority)) lowest = entry;
            }
            /* Nearby prefetch must never block a newly selected visible image. */
            if (!lowest || lowest->priority >= next->priority) {
                SDL_CondWaitTimeout(cache->condition, cache->mutex, 100); continue;
            }
            SDL_FreeSurface(lowest->surface); lowest->surface = NULL;
            lowest->failed = true; lowest->retry = now + 2000;
        }
        next->busy = true;
        ArtworkEntry request = *next;
        SDL_UnlockMutex(cache->mutex);
        SDL_Surface *surface = artwork_resize(artwork_load(cache, &request), request.soft);
        SDL_LockMutex(cache->mutex);
        next->surface = surface;
        next->busy = false;
        next->failed = !surface;
        next->retry = SDL_GetTicks() + 30000;
    }
    SDL_UnlockMutex(cache->mutex);
    return 0;
}

static void artwork_release(struct ArtworkCache *cache, ArtworkEntry *entry) {
    if (entry->texture) {
        cache->bytes -= (size_t)entry->width * entry->height * 4;
        SDL_DestroyTexture(entry->texture);
    }
    SDL_FreeSurface(entry->surface);
    memset(entry, 0, sizeof(*entry));
}

static ArtworkEntry *artwork_request(WolodriveApp *app, const GameItem *item, bool backdrop, int priority) {
    if (!item) return NULL;
    GameItem snapshot;
    if (app->catalog_mutex) SDL_LockMutex(app->catalog_mutex);
    snapshot = *item;
    if (app->catalog_mutex) SDL_UnlockMutex(app->catalog_mutex);
    ArtworkEntry request = {0};
    artwork_location(&snapshot, backdrop, &request);
    if (!request.key[0]) return NULL;
    if (!app->artwork) {
        struct ArtworkCache *cache = calloc(1, sizeof(*cache));
        if (!cache) return NULL;
        cache->mutex = SDL_CreateMutex();
        cache->condition = SDL_CreateCond();
        if (!cache->mutex || !cache->condition) {
            SDL_DestroyMutex(cache->mutex); SDL_DestroyCond(cache->condition); free(cache); return NULL;
        }
        ART_MKDIR("assets");
#ifdef __PS5__
        ART_MKDIR("/data/homebrew"); ART_MKDIR(WOLODRIVE_DATA_PATH);
#endif
        ART_MKDIR(COVERS_CACHE_DIR);
        ART_MKDIR(ART_OVERRIDES);
        app->artwork = cache;
        cache->thread = SDL_CreateThread(artwork_worker, "artwork", cache);
        if (!cache->thread) { artwork_destroy(app); return NULL; }
    }
    struct ArtworkCache *cache = app->artwork;
    SDL_LockMutex(cache->mutex);
    ArtworkEntry *entry = NULL, *oldest = NULL;
    Uint32 now = SDL_GetTicks();
    for (int i = 0; i < ART_SLOTS; i++) {
        ArtworkEntry *candidate = &cache->entries[i];
        if (strcmp(candidate->key, request.key) == 0) { entry = candidate; break; }
        if (!candidate->busy && (!oldest || !candidate->key[0] || now-candidate->used > now-oldest->used)) oldest = candidate;
    }
    if (!entry && oldest) {
        artwork_release(cache, oldest);
        *oldest = request;
        entry = oldest;
    }
    if (entry) {
        if (now-entry->priority_tick > 100) {
            entry->priority = priority; entry->priority_tick = now;
        } else entry->priority = SDL_max(entry->priority, priority);
        entry->used = now;
        SDL_CondSignal(cache->condition);
    }
    SDL_UnlockMutex(cache->mutex);
    return entry;
}

SDL_Texture *artwork_get(WolodriveApp *app, const GameItem *item, bool backdrop,
                         int priority, int *width, int *height) {
    ArtworkEntry *entry = artwork_request(app, item, backdrop, priority);
    if (!entry) return NULL;
    struct ArtworkCache *cache = app->artwork;
    SDL_LockMutex(cache->mutex);
    if (entry->failed && !entry->surface && !entry->texture) {
        bool local = entry->local;
        SDL_UnlockMutex(cache->mutex);
        GameItem fallback;
        if (app->catalog_mutex) SDL_LockMutex(app->catalog_mutex);
        fallback = *item;
        if (app->catalog_mutex) SDL_UnlockMutex(app->catalog_mutex);
        fallback.id_code[0] = 0;
        if (!local) {
            if (backdrop) fallback.backdrop_url[0] = fallback.landscape_url[0] = 0;
            else if (fallback.boxart_url[0]) fallback.boxart_url[0] = 0;
            else if (fallback.cover_url[0]) fallback.cover_url[0] = 0;
            else fallback.icon_url[0] = 0;
        }
        entry = artwork_request(app, &fallback, backdrop, priority);
        if (!entry) return NULL;
        SDL_LockMutex(cache->mutex);
    }
    if (entry->surface) {
        size_t bytes = (size_t)entry->surface->w * entry->surface->h * 4;
        while (cache->bytes + bytes > ART_BUDGET) {
            ArtworkEntry *oldest = NULL;
            for (int i = 0; i < ART_SLOTS; i++) {
                ArtworkEntry *candidate = &cache->entries[i];
                if (candidate != entry && candidate->texture && (!oldest || candidate->used < oldest->used)) oldest = candidate;
            }
            if (!oldest) break;
            artwork_release(cache, oldest);
        }
        /* Alpha-weighted sampled color gives letterboxed cards a matching matte. */
        SDL_Surface *rgba = entry->surface;
        if (rgba) {
            Uint64 red = 0, green = 0, blue = 0, alpha = 0;
            for (int y = 0; y < rgba->h; y += SDL_max(1, rgba->h/24)) {
                for (int x = 0; x < rgba->w; x += SDL_max(1, rgba->w/24)) {
                    Uint8 *pixel = (Uint8 *)rgba->pixels + y*rgba->pitch + x*4;
                    red += pixel[0]*pixel[3]; green += pixel[1]*pixel[3]; blue += pixel[2]*pixel[3]; alpha += pixel[3];
                }
            }
            if (alpha) entry->color = (SDL_Color){red/alpha, green/alpha, blue/alpha, 255};
        }
        entry->texture = SDL_CreateTextureFromSurface(app->renderer, entry->surface);
        entry->width = entry->surface->w; entry->height = entry->surface->h;
        if (entry->texture) cache->bytes += bytes;
        else { entry->failed = true; entry->retry = SDL_GetTicks() + 30000; }
        SDL_FreeSurface(entry->surface); entry->surface = NULL;
    }
    *width = entry->width; *height = entry->height;
    SDL_Texture *texture = entry->texture;
    SDL_UnlockMutex(cache->mutex);
    return texture;
}

SDL_Color artwork_background(WolodriveApp *app, SDL_Texture *texture) {
    if (app->artwork) for (int i = 0; i < ART_SLOTS; i++) {
        ArtworkEntry *entry = &app->artwork->entries[i];
        if (entry->texture == texture) return entry->color;
    }
    return (SDL_Color){40,40,45,255};
}

bool artwork_complete_box(WolodriveApp *app, SDL_Texture *texture) {
    if (app->artwork) for (int i = 0; i < ART_SLOTS; i++) {
        ArtworkEntry *entry = &app->artwork->entries[i];
        if (entry->texture == texture) return entry->complete_box;
    }
    return false;
}

void artwork_prefetch(WolodriveApp *app, const GameItem *item, int priority) {
    artwork_request(app, item, false, priority);
}

void artwork_destroy(WolodriveApp *app) {
    struct ArtworkCache *cache = app->artwork;
    if (!cache) return;
    SDL_AtomicSet(&cache->stop, 1);
    SDL_CondSignal(cache->condition);
    if (cache->thread) SDL_WaitThread(cache->thread, NULL);
    for (int i = 0; i < ART_SLOTS; i++) artwork_release(cache, &cache->entries[i]);
    SDL_DestroyCond(cache->condition); SDL_DestroyMutex(cache->mutex);
    free(cache); app->artwork = NULL;
}
