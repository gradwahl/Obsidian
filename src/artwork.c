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
#define ART_PRIORITY_WINDOW_MS 100u
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
    Uint32 used, retry, priority_tick, disk_checked;
    bool busy, local, failed, soft, complete_box, disk_tried, needs_download;
} ArtworkEntry;

typedef struct {
    char input[768];
    ArtworkEntry request;
    Uint32 checked;
    Uint64 used;
} ArtworkLocation;

struct ArtworkCache {
    SDL_mutex *mutex;
    SDL_cond *condition;
    SDL_Thread *thread;
    SDL_Thread *download_thread;
    SDL_atomic_t stop;
    ArtworkEntry entries[ART_SLOTS];
    size_t bytes;
    /* UI thread only: cache positive and negative filesystem probes. */
    ArtworkLocation locations[128];
    Uint64 location_clock;
};

SDL_Rect artwork_fit(int width, int height, SDL_Rect box, bool crop) {
    if (width <= 0 || height <= 0) return (SDL_Rect){box.x, box.y, 0, 0};
    double sx = (double)box.w / width, sy = (double)box.h / height;
    double scale = crop ? SDL_max(sx, sy) : SDL_min(sx, sy);
    int w = SDL_max(1, (int)(width * scale)), h = SDL_max(1, (int)(height * scale));
    return (SDL_Rect){box.x + (box.w-w)/2, box.y + (box.h-h)/2, w, h};
}

static const char *artwork_url(const GameItem *item, bool backdrop) {
    if (backdrop) return item->backdrop_url[0] ? item->backdrop_url :
        item->landscape_url[0] ? item->landscape_url : item->cover_url[0] ? item->cover_url :
        item->icon_url[0] ? item->icon_url : item->cover_fallback_url;
    return item->boxart_url[0] ? item->boxart_url : item->cover_url[0] ? item->cover_url :
        item->icon_url[0] ? item->icon_url : item->landscape_url[0] ? item->landscape_url : item->cover_fallback_url;
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
    const char *url = artwork_url(item, backdrop);
    if (strncmp(url,"local:",6) == 0) {
        request->local = true;
        SDL_strlcpy(request->path,url+6,sizeof(request->path));
        if (stat(request->path,&st) == 0 && st.st_size > 0)
            snprintf(request->key,sizeof(request->key),"%s:%lld:%lld",request->path,(long long)st.st_mtime,(long long)st.st_size);
        return;
    }
    request->complete_box = !backdrop && (item->boxart_url[0] ||
        (item->cover_fallback_url[0] && !strcmp(url, item->cover_fallback_url)));
    request->soft = backdrop && !item->backdrop_url[0] && !item->landscape_url[0];
    SDL_strlcpy(request->url, url, sizeof(request->url));
    if (!url[0]) { request->key[0] = 0; return; }
    SDL_strlcpy(request->key, url, sizeof(request->key));
    if (request->soft) SDL_strlcat(request->key, ":soft", sizeof(request->key));
    if (request->complete_box) SDL_strlcat(request->key, ":box", sizeof(request->key));
    /* Stream remote artwork into bounded memory; no persistent .art cache. */
    request->needs_download = true;
}

/* Remote covers are memory-only: do not persist network images on PS5.
   The SDL texture LRU is already capped at 48 MiB, and in-flight downloads
   have a separate 16 MiB cap. Manual local overrides are still supported. */
#define ART_TRANSFER_LIMIT (16u * 1024u * 1024u)
typedef struct { unsigned char *data; size_t bytes, capacity; } ArtworkDownload;
static size_t artwork_write(void *data, size_t size, size_t count, void *userdata) {
    ArtworkDownload *download = userdata;
    if (size && count > SIZE_MAX / size) return 0;
    size_t bytes = size * count;
    if (download->bytes > ART_TRANSFER_LIMIT ||
        bytes > ART_TRANSFER_LIMIT - download->bytes) return 0;
    size_t requested = download->bytes + bytes;
    if (requested > download->capacity) {
        size_t capacity = download->capacity ? download->capacity : 64 * 1024;
        while (capacity < requested) capacity *= 2;
        unsigned char *expanded = realloc(download->data, capacity);
        if (!expanded) return 0;
        download->data = expanded;
        download->capacity = capacity;
    }
    if (bytes) memcpy(download->data + download->bytes, data, bytes);
    download->bytes += bytes;
    return bytes;
}
static int artwork_progress(void *userdata, curl_off_t a, curl_off_t b, curl_off_t c, curl_off_t d) {
    (void)a; (void)b; (void)c; (void)d;
    return SDL_AtomicGet(&((struct ArtworkCache *)userdata)->stop);
}
static SDL_Surface *artwork_load(struct ArtworkCache *cache, const ArtworkEntry *entry) {
    if (entry->local) return IMG_Load(entry->path);
    if (strncmp(entry->url, "https://", 8) && strncmp(entry->url, "http://", 7)) return NULL;
    CURL *curl = curl_easy_init();
    if (!curl) return NULL;
    ArtworkDownload download = {0};
    CURLcode result;
    long status = 0;
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
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Obsidian/2.0 (streamed artwork)");
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, artwork_write);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &download);
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, artwork_progress);
    curl_easy_setopt(curl, CURLOPT_XFERINFODATA, cache);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    result = curl_easy_perform(curl);
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
    curl_easy_cleanup(curl);
    SDL_Surface *surface = NULL;
    if (result == CURLE_OK && status >= 200 && status < 300 &&
        download.data && download.bytes && download.bytes <= ART_TRANSFER_LIMIT) {
        SDL_RWops *stream = SDL_RWFromConstMem(download.data, (int)download.bytes);
        if (stream) surface = IMG_Load_RW(stream, 1);
    }
    free(download.data);
    /* Transparent console marks should remain readable against dark cards,
       matching the former local-image library renderer. */
    if (surface && (strstr(entry->url, "/assets/library/logos/") ||
                    strstr(entry->url, "/assets/library/companies/sony.png"))) {
        SDL_Surface *rgba = SDL_ConvertSurfaceFormat(surface, SDL_PIXELFORMAT_RGBA32, 0);
        if (rgba) {
            SDL_FreeSurface(surface);
            surface = rgba;
            if (SDL_LockSurface(surface) == 0) {
                for (int y = 0; y < surface->h; y++) {
                    for (int x = 0; x < surface->w; x++) {
                        Uint8 *pixel = (Uint8 *)surface->pixels + y * surface->pitch + x * 4;
                        pixel[0] = pixel[1] = pixel[2] = 245;
                    }
                }
                SDL_UnlockSurface(surface);
            }
        }
    }
    return surface;
}

static SDL_Surface *artwork_resize(SDL_Surface *source, bool soft) {
    (void)soft;
    if (!source) return NULL;
    SDL_Rect fit = artwork_fit(source->w, source->h, (SDL_Rect){0,0,1920,1080}, false);
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

static int artwork_priority(const ArtworkEntry *entry, Uint32 now) {
    /* Requests outside the current browsing window must not retain hero priority
       until the two-second stale-surface cleanup. */
    return now - entry->used <= ART_PRIORITY_WINDOW_MS ? entry->priority : 0;
}

static int artwork_run_worker(struct ArtworkCache *cache, bool downloading) {
    SDL_LockMutex(cache->mutex);
    while (!SDL_AtomicGet(&cache->stop)) {
        ArtworkEntry *next = NULL;
        Uint32 now = SDL_GetTicks();
        int pending = 0, busy = 0;
        for (int i = 0; i < ART_SLOTS; i++) {
            ArtworkEntry *entry = &cache->entries[i];
            if (entry->surface && now-entry->used > 2000) {
                SDL_FreeSurface(entry->surface); entry->surface = NULL;
            }
            if (entry->surface) pending++;
            if (entry->busy) busy++;
        }
        /* Bound decoded surfaces waiting for the UI to upload them as well. */
        for (int i = 0; i < ART_SLOTS; i++) {
            ArtworkEntry *entry = &cache->entries[i];
            if (!entry->key[0] || entry->texture || entry->surface || entry->busy || now-entry->used > 2000) continue;
            if (downloading) {
                if (!entry->needs_download || (entry->failed && (Sint32)(now-entry->retry) < 0)) continue;
            } else {
                /* Local overrides load on this worker; remote requests stream on the
                   download worker only, never touching the console filesystem. */
                if (entry->needs_download || !entry->local) continue;
                /* Recheck disk while a failed/missing download waits for retry:
                   a bulk import must become usable without waiting 30 seconds. */
                if (entry->disk_tried && now-entry->disk_checked < 1000) continue;
                if (entry->failed && !entry->needs_download && (Sint32)(now-entry->retry) < 0) continue;
            }
            if (!next || artwork_priority(entry, now) > artwork_priority(next, now) ||
                (artwork_priority(entry, now) == artwork_priority(next, now) && now-entry->used < now-next->used)) next = entry;
        }
        if (!next) { SDL_CondWaitTimeout(cache->condition, cache->mutex, 100); continue; }
        if (pending + busy >= 3) {
            ArtworkEntry *lowest = NULL;
            for (int i = 0; i < ART_SLOTS; i++) {
                ArtworkEntry *entry = &cache->entries[i];
                if (entry->surface && (!lowest || artwork_priority(entry, now) < artwork_priority(lowest, now))) lowest = entry;
            }
            /* Nearby prefetch must never block a newly selected visible image. */
            if (!lowest || artwork_priority(lowest, now) >= artwork_priority(next, now)) {
                SDL_CondWaitTimeout(cache->condition, cache->mutex, 100); continue;
            }
            SDL_FreeSurface(lowest->surface); lowest->surface = NULL;
            /* Requeue discarded decoded work without inventing a load failure
               and its retry cooldown. Priority/capacity rules prevent thrashing. */
            lowest->failed = false; lowest->retry = 0;
            lowest->needs_download = false;
            lowest->disk_tried = false;
        }
        next->busy = true;
        ArtworkEntry request = *next;
        SDL_UnlockMutex(cache->mutex);
        SDL_Surface *surface = artwork_resize(downloading ? artwork_load(cache, &request) : IMG_Load(request.path), request.soft);
        SDL_LockMutex(cache->mutex);
        next->surface = surface;
        next->busy = false;
        if (!downloading) {
            next->disk_tried = true;
            next->disk_checked = SDL_GetTicks();
        }
        next->needs_download = !surface && !request.local;
        if (surface || downloading || request.local) {
            next->failed = !surface;
            next->retry = SDL_GetTicks() + (downloading ? 30000 : 1000);
        }
        SDL_CondBroadcast(cache->condition);
    }
    SDL_UnlockMutex(cache->mutex);
    return 0;
}

static int artwork_worker(void *userdata) {
    return artwork_run_worker(userdata, false);
}

static int artwork_download_worker(void *userdata) {
    return artwork_run_worker(userdata, true);
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
        /* COVERS_CACHE_DIR is intentionally not created for streamed artwork.
           Existing disk caches are left alone and can be cleared manually. */
        ART_MKDIR(ART_OVERRIDES);
        app->artwork = cache;
        cache->thread = SDL_CreateThread(artwork_worker, "artwork", cache);
        if (!cache->thread) { artwork_destroy(app); return NULL; }
        cache->download_thread = SDL_CreateThread(artwork_download_worker, "artwork-download", cache);
        if (!cache->download_thread) { artwork_destroy(app); return NULL; }
    }
    struct ArtworkCache *cache = app->artwork;
    const char *url = artwork_url(&snapshot, backdrop);
    char input[768];
    snprintf(input, sizeof(input), "%s:%d:%d:%d:%s", snapshot.id_code, backdrop,
             !backdrop && (snapshot.boxart_url[0] ||
                 (snapshot.cover_fallback_url[0] && !strcmp(url, snapshot.cover_fallback_url))),
             backdrop && !snapshot.backdrop_url[0] && !snapshot.landscape_url[0], url);
    Uint32 now = SDL_GetTicks();
    ArtworkLocation *location = NULL, *oldest_location = &cache->locations[0];
    for (int i = 0; i < 128; i++) {
        ArtworkLocation *candidate = &cache->locations[i];
        if (candidate->used && !strcmp(candidate->input, input)) { location = candidate; break; }
        if (candidate->used < oldest_location->used) oldest_location = candidate;
    }
    if (!location || now - location->checked >= 1000) {
        if (!location) location = oldest_location;
        memset(&location->request, 0, sizeof(location->request));
        artwork_location(&snapshot, backdrop, &location->request);
        SDL_strlcpy(location->input, input, sizeof(location->input));
        location->checked = now;
    }
    location->used = ++cache->location_clock;
    ArtworkEntry request = location->request;
    if (!request.key[0]) return NULL;
    SDL_LockMutex(cache->mutex);
    ArtworkEntry *entry = NULL, *oldest = NULL;
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
        if (now-entry->priority_tick > ART_PRIORITY_WINDOW_MS) {
            entry->priority = priority; entry->priority_tick = now;
        } else entry->priority = SDL_max(entry->priority, priority);
        entry->used = now;
        SDL_CondBroadcast(cache->condition);
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
    GameItem fallback;
    bool copied = false;
    int attempts = 0;
    while (entry->failed && !entry->surface && !entry->texture) {
        if (++attempts > 8) { SDL_UnlockMutex(cache->mutex); return NULL; }
        bool local = entry->local;
        char failed_url[512];
        SDL_strlcpy(failed_url, entry->url, sizeof(failed_url));
        SDL_UnlockMutex(cache->mutex);
        if (!copied) {
            if (app->catalog_mutex) SDL_LockMutex(app->catalog_mutex);
            fallback = *item;
            if (app->catalog_mutex) SDL_UnlockMutex(app->catalog_mutex);
            copied = true;
        }
        fallback.id_code[0] = 0;
        if (!local) {
            char *urls[] = {fallback.boxart_url, fallback.cover_url, fallback.icon_url,
                           fallback.landscape_url, fallback.backdrop_url, fallback.cover_fallback_url};
            for (size_t i = 0; i < sizeof(urls)/sizeof(urls[0]); i++)
                if (!strcmp(urls[i], failed_url)) urls[i][0] = 0;
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

SDL_Texture *artwork_get_media(WolodriveApp *app, const char *uri, int priority, int *width, int *height, bool *failed) {
    GameItem media = {0};
    if (strncmp(uri,"https://",8) == 0 || strncmp(uri,"http://",7) == 0)
        SDL_strlcpy(media.cover_url,uri,sizeof(media.cover_url));
    else snprintf(media.cover_url,sizeof(media.cover_url),"local:%s",uri);
    SDL_Texture *texture = artwork_get(app,&media,false,priority,width,height);
    ArtworkEntry *entry = artwork_request(app,&media,false,priority);
    *failed = entry == NULL;
    if (entry && app->artwork) {
        SDL_LockMutex(app->artwork->mutex); *failed = entry->failed; SDL_UnlockMutex(app->artwork->mutex);
    }
    return texture;
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
    SDL_CondBroadcast(cache->condition);
    if (cache->thread) SDL_WaitThread(cache->thread, NULL);
    if (cache->download_thread) SDL_WaitThread(cache->download_thread, NULL);
    for (int i = 0; i < ART_SLOTS; i++) artwork_release(cache, &cache->entries[i]);
    SDL_DestroyCond(cache->condition); SDL_DestroyMutex(cache->mutex);
    free(cache); app->artwork = NULL;
}
