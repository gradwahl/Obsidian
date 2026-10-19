#include "wolodrive.h"
#include "text.h"
#include <curl/curl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <sys/stat.h>

typedef struct {
    char *data;
    size_t size;
} MemoryBuffer;

static size_t write_mem_cb(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    MemoryBuffer *mem = (MemoryBuffer *)userp;
    char *ptr = realloc(mem->data, mem->size + realsize + 1);
    if (!ptr) return 0;
    mem->data = ptr;
    memcpy(&(mem->data[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->data[mem->size] = '\0';
    return realsize;
}

static char *http_get_string(const char *url) {
    CURL *curl = curl_easy_init();
    if (!curl) return NULL;

    MemoryBuffer chunk = { malloc(1), 0 };
    if (!chunk.data) {
        curl_easy_cleanup(curl);
        return NULL;
    }
    chunk.data[0] = '\0';

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_mem_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Wolodrive/2.0 (PS5-13.60; Pegasus-DL-Client)");
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 8L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

    CURLcode res = curl_easy_perform(curl);
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK || http_code >= 400 || chunk.size == 0) {
        free(chunk.data);
        return NULL;
    }
    return chunk.data;
}

/* ============================================================================
 * Lekki, odporny na formatowanie parser JSON dla struktur Pegasus-DL
 * ============================================================================ */

static const char *skip_ws(const char *p, const char *end) {
    while (p < end && (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')) p++;
    return p;
}

/* Znajduje koniec obiektu {...} lub tablicy [...] z uwzględnieniem cudzysłowów */
static const char *find_matching_bracket(const char *start, const char *end, char open_ch, char close_ch) {
    int depth = 0;
    bool in_str = false;
    for (const char *p = start; p < end; p++) {
        if (*p == '"' && (p == start || *(p - 1) != '\\')) {
            in_str = !in_str;
        } else if (!in_str) {
            if (*p == open_ch) depth++;
            else if (*p == close_ch) {
                depth--;
                if (depth == 0) return p;
            }
        }
    }
    return NULL;
}

/* Bounded substring search: absent keys must never scan later game records.
   The old unbounded strstr() was quadratic for full-source metadata indexes. */
static const char *find_limited(const char *start, const char *end, const char *needle) {
    size_t size = strlen(needle);
    if (!size) return start;
    while (start < end && (size_t)(end - start) >= size) {
        const char *hit = memchr(start, needle[0], (size_t)(end - start) - size + 1);
        if (!hit) break;
        if (memcmp(hit, needle, size) == 0) return hit;
        start = hit + 1;
    }
    return NULL;
}

/* Wyciąga wartość tekstową dla podanego klucza w obrębie [obj_start, obj_end] */
static bool json_extract_string(const char *obj_start, const char *obj_end, const char *key, char *out, size_t out_sz) {
    char pattern[128];
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    size_t pat_len = strlen(pattern);

    const char *p = obj_start;
    while (p + pat_len < obj_end) {
        const char *hit = find_limited(p, obj_end, pattern);
        if (!hit || hit >= obj_end) return false;

        const char *colon = skip_ws(hit + pat_len, obj_end);
        if (colon < obj_end && *colon == ':') {
            const char *val = skip_ws(colon + 1, obj_end);
            if (val < obj_end && *val == '"') {
                return text_decode_json_string(val + 1, obj_end, out, out_sz);
            }
        }
        p = hit + pat_len;
    }
    return false;
}

/* Locate a direct member, excluding URLs nested in media arrays or metadata. */
static const char *json_object_value(const char *start, const char *end, const char *key) {
    int objects = 0, arrays = 0;
    size_t length = strlen(key);
    for (const char *p = start; p < end; p++) {
        if (*p == '{') objects++;
        else if (*p == '}') objects--;
        else if (*p == '[') arrays++;
        else if (*p == ']') arrays--;
        else if (*p == '"') {
            const char *first = ++p;
            while (p < end && *p != '"') {
                if (*p == '\\' && p+1 < end) p++;
                p++;
            }
            if (p >= end) return NULL;
            if (objects == 1 && arrays == 0 && (size_t)(p-first) == length && strncmp(first, key, length) == 0) {
                const char *colon = skip_ws(p+1, end);
                if (colon < end && *colon == ':') return skip_ws(colon+1, end);
            }
        }
    }
    return NULL;
}

/* Wyciąga liczbę (np. sizeBytes lub size) dla podanego klucza */
static double json_extract_number(const char *obj_start, const char *obj_end, const char *key) {
    char pattern[128];
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    size_t pat_len = strlen(pattern);

    const char *p = obj_start;
    while (p + pat_len < obj_end) {
        const char *hit = find_limited(p, obj_end, pattern);
        if (!hit || hit >= obj_end) return 0.0;

        const char *colon = skip_ws(hit + pat_len, obj_end);
        if (colon < obj_end && *colon == ':') {
            const char *val = skip_ws(colon + 1, obj_end);
            if (val < obj_end && (*val == '"' || isdigit((unsigned char)*val))) {
                if (*val == '"') val++;
                return strtod(val, NULL);
            }
        }
        p = hit + pat_len;
    }
    return 0.0;
}

/* Wyciąga pierwszy link z tablicy downloadLinks: [ { "url": "..." } ] */
static bool json_extract_download_link(const char *obj_start, const char *obj_end, char *out, size_t out_sz) {
    const char *dl = find_limited(obj_start, obj_end, "\"downloadLinks\"");
    if (dl && dl < obj_end) {
        const char *arr_start = strchr(dl, '[');
        if (arr_start && arr_start < obj_end) {
            const char *arr_end = find_matching_bracket(arr_start, obj_end, '[', ']');
            if (arr_end) {
                if (json_extract_string(arr_start, arr_end, "url", out, out_sz)) return true;
                if (json_extract_string(arr_start, arr_end, "href", out, out_sz)) return true;
            }
        }
    }
    if (json_extract_string(obj_start, obj_end, "downloadUrl", out, out_sz)) return true;
    if (json_extract_string(obj_start, obj_end, "pkgUrl", out, out_sz)) return true;
    const char *url = json_object_value(obj_start, obj_end, "url");
    if (url && url < obj_end && *url == '"') return text_decode_json_string(url+1, obj_end, out, out_sz);
    return false;
}

static void generate_color_from_id(const char *id, uint8_t *r, uint8_t *g, uint8_t *b) {
    uint32_t hash = 5381;
    for (int i = 0; id[i] != '\0'; i++) {
        hash = ((hash << 5) + hash) + (uint8_t)id[i];
    }
    const uint8_t palette[][3] = {
        { 175, 24,  38  }, { 28,  92,  165 }, { 120, 42,  148 },
        { 22,  128, 115 }, { 184, 88,  24  }, { 45,  115, 72  },
        { 145, 35,  85  }, { 60,  78,  155 }
    };
    int idx = (int)(hash % 8);
    *r = palette[idx][0];
    *g = palette[idx][1];
    *b = palette[idx][2];
}

static PackageFormat detect_package_format(const char *id_code, const char *dl_url, const char *fmt_str) {
    if (fmt_str && fmt_str[0]) {
        if (strcasecmp(fmt_str, "catalog") == 0) return FMT_CATALOG;
        if (strcasestr(fmt_str, "exfat")) return FMT_EXFAT;
        if (strcasestr(fmt_str, "ffpkg")) return FMT_FFPKG;
        if (strcasestr(fmt_str, "pkg"))   return FMT_PKG;
    }
    if (dl_url && dl_url[0]) {
        if (strcasestr(dl_url, ".exfat")) return FMT_EXFAT;
        if (strcasestr(dl_url, ".ffpkg")) return FMT_FFPKG;
        if (strcasestr(dl_url, ".pkg"))   return FMT_PKG;
    }
    if (id_code && strncasecmp(id_code, "CUSA", 4) == 0) {
        return FMT_PKG;
    }
    return FMT_EXFAT;
}

/* The sources publish ISO 8601 UTC createdAt timestamps, not retail dates.
   Reject partial/invalid dates instead of inventing an ordering for them. */
static uint64_t parse_catalog_date(const char *text) {
    size_t length = strlen(text);
    if (length < 20 || text[4] != '-' || text[7] != '-' || text[10] != 'T' ||
        text[13] != ':' || text[16] != ':') return 0;
    const int positions[] = {0, 1, 2, 3, 5, 6, 8, 9, 11, 12, 14, 15, 17, 18};
    uint64_t value = 0;
    for (size_t i = 0; i < sizeof(positions) / sizeof(positions[0]); i++) {
        char c = text[positions[i]];
        if (c < '0' || c > '9') return 0;
        value = value * 10 + (unsigned)(c - '0');
    }
    size_t suffix = 19;
    if (text[suffix] == '.') {
        suffix++;
        size_t first_digit = suffix;
        while (suffix < length && isdigit((unsigned char)text[suffix])) suffix++;
        if (suffix == first_digit) return 0;
    }
    if (suffix + 1 != length || text[suffix] != 'Z') return 0;
    int year = (int)(value / 10000000000ULL);
    int month = (int)(value / 100000000ULL % 100);
    int day = (int)(value / 1000000ULL % 100);
    int hour = (int)(value / 10000 % 100);
    int minute = (int)(value / 100 % 100), second = (int)(value % 100);
    static const int days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (year < 1 || month < 1 || month > 12 || day < 1 || hour > 23 || minute > 59 || second > 59) return 0;
    int maximum = days[month - 1] + (month == 2 && year % 4 == 0 && (year % 100 != 0 || year % 400 == 0));
    return day <= maximum ? value : 0;
}

typedef struct {
    GameItem *item;
    int order;
} HomeCandidate;

typedef struct {
    const char *name;
    int next, end;
    uint64_t newest;
} HomeSource;

static int home_candidate_compare(const void *left, const void *right) {
    const HomeCandidate *a = left, *b = right;
    int source = SDL_strcasecmp(a->item->source_name, b->item->source_name);
    if (source) return source;
    if (a->item->catalog_added_at != b->item->catalog_added_at)
        return a->item->catalog_added_at > b->item->catalog_added_at ? -1 : 1;
    /* Undated catalog metadata stays in its original source listing order. */
    return (a->order > b->order) - (a->order < b->order);
}

static int home_source_compare(const void *left, const void *right) {
    const HomeSource *a = left, *b = right;
    if (a->newest != b->newest) return a->newest > b->newest ? -1 : 1;
    return SDL_strcasecmp(a->name, b->name);
}

static bool home_same_game(const GameItem *a, const GameItem *b) {
    if (SDL_strcasecmp(a->title, b->title) != 0) return false;
    if (a->platform[0] || b->platform[0])
        return SDL_strcasecmp(a->platform, b->platform) == 0;
    return SDL_strncasecmp(a->id_code, b->id_code, 4) == 0;
}

int catalog_latest_games_locked(WolodriveApp *app, GameItem **items, int capacity) {
    if (!app || !items || capacity <= 0) return 0;

    HomeCandidate *candidates = malloc(sizeof(*candidates) * (size_t)app->catalog_count);
    HomeSource *sources = malloc(sizeof(*sources) * (size_t)app->catalog_count);
    if (!candidates || !sources) {
        free(candidates);
        free(sources);
        return 0;
    }

    int eligible = 0;
    for (int i = 0; i < app->catalog_count; i++) {
        GameItem *item = &app->catalog[i];
        if (!item->is_game || item->is_dlc || !settings_accepts_item(app, item)) continue;
        candidates[eligible++] = (HomeCandidate){item, i};
    }
    if (eligible == 0) {
        free(candidates);
        free(sources);
        return 0;
    }

    qsort(candidates, (size_t)eligible, sizeof(*candidates), home_candidate_compare);
    int source_count = 0;
    for (int i = 0; i < eligible; ) {
        int end = i + 1;
        const char *name = candidates[i].item->source_name;
        while (end < eligible && SDL_strcasecmp(name, candidates[end].item->source_name) == 0)
            end++;
        sources[source_count++] = (HomeSource){name, i, end,
                                              candidates[i].item->catalog_added_at};
        i = end;
    }
    qsort(sources, (size_t)source_count, sizeof(*sources), home_source_compare);

    int count = 0;
    bool available = true;
    while (count < capacity && available) {
        available = false;
        /* Round-robin each enabled source, so even undated indexes such as
           Axekin appear next to dated Pegasus packages on Home. */
        for (int i = 0; i < source_count && count < capacity; i++) {
            HomeSource *source = &sources[i];
            if (source->next >= source->end) continue;
            available = true;
            GameItem *item = candidates[source->next++].item;
            bool duplicate = false;
            for (int j = 0; j < count; j++) {
                if (home_same_game(item, items[j])) {
                    duplicate = true;
                    break;
                }
            }
            if (!duplicate) items[count++] = item;
        }
    }

    free(candidates);
    free(sources);
    return count;
}

static void json_extract_screenshots(const char *start, const char *end, GameItem *item) {
    item->screenshot_count = 0;
    const char *array = json_object_value(start, end, "screenshots");
    if (!array) array = json_object_value(start, end, "screenshotUrls");
    if (!array) return;
    if (array >= end || *array != '[') return;
    const char *last = find_matching_bracket(array, end, '[', ']');
    if (!last) return;
    item->screenshot_count = 0;
    const char *p = array + 1;
    while (p < last && item->screenshot_count < MAX_GAME_SCREENSHOTS) {
        p = skip_ws(p, last);
        if (p >= last) break;
        if (*p == ',') { p++; continue; }
        char url[512] = {0};
        if (*p == '"') {
            if (!text_decode_json_string(p+1, last, url, sizeof(url))) break;
            p++;
            while (p < last && *p != '"') {
                if (*p == '\\' && p+1 < last) p++;
                p++;
            }
            if (p < last) p++;
        } else if (*p == '{') {
            const char *object_end = find_matching_bracket(p, last, '{', '}');
            if (!object_end) break;
            json_extract_string(p, object_end, "url", url, sizeof(url));
            p = object_end+1;
        } else break;
        if (strncmp(url, "https://", 8) != 0 && strncmp(url, "http://", 7) != 0) continue;
        bool duplicate = false;
        for (int i = 0; i < item->screenshot_count; i++) if (strcmp(url, item->screenshot_urls[i]) == 0) duplicate = true;
        if (!duplicate) SDL_strlcpy(item->screenshot_urls[item->screenshot_count++], url, 512);
    }
}

static void register_parsed_game(WolodriveApp *app, const char *catalog_name,
                                 const char *id_code, const char *title,
                                 const char *version, const char *desc,
                                 const char *dl_url, const char *cover_url,
                                 float size_gb, PackageFormat fmt,
                                 const char *category, uint64_t added_at,
                                 const char *object_start, const char *object_end) {
    if (!title || title[0] == '\0') return;

    SDL_LockMutex(app->catalog_mutex);

    /* Sprawdź czy gra o tym samym ID lub tytule już istnieje – jeśli tak, zaktualizuj dane */
    GameItem *target = NULL;
    /* Unique offline Axekin records can append without quadratic scanning. */
    bool bulk_axekin = fmt == FMT_CATALOG && catalog_name && strcmp(catalog_name, "Axekin") == 0;
    for (int i = 0; !bulk_axekin && i < app->catalog_count; i++) {
        if (strcasecmp(app->catalog[i].title, title) == 0 &&
            strcasecmp(app->catalog[i].source_name, catalog_name && catalog_name[0] ? catalog_name : "Pegasus-DL") == 0 &&
            strcasecmp(app->catalog[i].id_code, id_code && id_code[0] ? id_code : "PPSA00001") == 0 &&
            app->catalog[i].format == fmt) {
            target = &app->catalog[i];
            break;
        }
    }

    if (!target) {
        if (app->catalog_count >= MAX_CATALOG_ITEMS) {
            SDL_UnlockMutex(app->catalog_mutex);
            return;
        }
        target = &app->catalog[app->catalog_count++];
        memset(target, 0, sizeof(GameItem));
        target->state = STATE_AVAILABLE;
        target->progress = 0.0f;
    }

    snprintf(target->id_code, sizeof(target->id_code), "%s", (id_code && id_code[0]) ? id_code : "PPSA00001");
    target->platform[0] = '\0';
    json_extract_string(object_start, object_end, "platform", target->platform, sizeof(target->platform));
    snprintf(target->title, sizeof(target->title), "%s", title);
    snprintf(target->version, sizeof(target->version), "%s", (version && version[0]) ? version : "1.00");
    snprintf(target->source_name, sizeof(target->source_name), "%s", (catalog_name && catalog_name[0]) ? catalog_name : "Pegasus-DL");
    if (category && category[0]) {
        target->is_game = strcasecmp(category, "game") == 0;
        target->is_dlc = strcasecmp(category, "dlc") == 0;
    } else if (fmt == FMT_CATALOG) {
        /* Compact curated game indexes omit their redundant category key. */
        target->is_game = true;
        target->is_dlc = false;
    }
    char parent[32] = {0};
    if (json_extract_string(object_start, object_end, "baseTitleId", parent, sizeof(parent)) ||
        json_extract_string(object_start, object_end, "parentTitleId", parent, sizeof(parent)))
        SDL_strlcpy(target->parent_title_id, parent, sizeof(target->parent_title_id));
    json_extract_screenshots(object_start, object_end, target);
    const char *media_keys[] = {"launchboxUrl", "playstationUrl"};
    char *media_destinations[] = {target->launchbox_url, target->playstation_url};
    for (int media = 0; media < 2; media++) {
        media_destinations[media][0] = 0;
        const char *value = json_object_value(object_start, object_end, media_keys[media]);
        if (value && *value == '"') text_decode_json_string(value+1, object_end, media_destinations[media], 512);
    }

    const char *boxart = json_object_value(object_start, object_end, "boxArtUrl");
    if (!boxart) boxart = json_object_value(object_start, object_end, "boxartUrl");
    if (!boxart) boxart = json_object_value(object_start, object_end, "boxCoverUrl");
    target->boxart_url[0] = 0;
    if (boxart && *boxart == '"') text_decode_json_string(boxart+1, object_end, target->boxart_url, sizeof(target->boxart_url));
    const char *icon = json_object_value(object_start, object_end, "iconUrl");
    if (!icon) icon = json_object_value(object_start, object_end, "icon");
    target->icon_url[0] = 0;
    if (icon && *icon == '"') text_decode_json_string(icon+1, object_end, target->icon_url, sizeof(target->icon_url));
    const char *landscape_keys[] = {"landscapeUrl", "cardUrl", "bannerUrl"};
    const char *backdrop_keys[] = {"backdropUrl", "backgroundUrl", "heroUrl"};
    target->landscape_url[0] = target->backdrop_url[0] = 0;
    for (int i = 0; i < 3; i++) {
        const char *value = json_object_value(object_start, object_end, landscape_keys[i]);
        if (value && *value == '"' && text_decode_json_string(value+1, object_end, target->landscape_url, sizeof(target->landscape_url)) && target->landscape_url[0]) break;
    }
    for (int i = 0; i < 3; i++) {
        const char *value = json_object_value(object_start, object_end, backdrop_keys[i]);
        if (value && *value == '"' && text_decode_json_string(value+1, object_end, target->backdrop_url, sizeof(target->backdrop_url)) && target->backdrop_url[0]) break;
    }

    /* A newer source for the same game counts as its newest catalog addition. */
    if (added_at > target->catalog_added_at) target->catalog_added_at = added_at;

    text_plain_description(desc, target->description, sizeof(target->description));
    target->description_id = target->description[0] ? TEXT_COUNT : TXT_DESCRIPTION_FALLBACK;

    if (dl_url && dl_url[0]) {
        snprintf(target->download_url, sizeof(target->download_url), "%s", dl_url);
    }
    SDL_strlcpy(target->cover_url, cover_url ? cover_url : "", sizeof(target->cover_url));

    target->format  = fmt;
    target->size_gb = fmt == FMT_CATALOG ? (size_gb > 0 ? size_gb : 0) :
                      (size_gb > 0.05f ? size_gb : 42.5f);

    if (fmt == FMT_CATALOG) {
        target->download_url[0] = '\0';
        target->install_path[0] = '\0';
    } else if (fmt == FMT_PKG) {
        snprintf(target->install_path, sizeof(target->install_path), "/data/pkg/%s.pkg", target->id_code);
    } else if (fmt == FMT_FFPKG) {
        snprintf(target->install_path, sizeof(target->install_path), "/data/homebrew/%s", target->id_code);
    } else {
        snprintf(target->install_path, sizeof(target->install_path), "/data/homebrew/%s/%s.exfat", target->id_code, target->id_code);
    }

    generate_color_from_id(target->id_code, &target->cover_r, &target->cover_g, &target->cover_b);

    app->catalog_dirty = true;
    SDL_UnlockMutex(app->catalog_mutex);
}

/* Parsuje pojedynczy plik JSON katalogu gier (np. z tablicą "packages" lub "games") */
static int parse_catalog_json(WolodriveApp *app, const char *json, const char *fallback_source_name,
                              char subcatalog_urls[][512], int *subcatalog_count, int max_subcatalogs) {
    if (!json) return 0;
    const char *end = json + strlen(json);
    int games_added = 0;

    char catalog_name[64] = {0};
    if (!json_extract_string(json, end, "name", catalog_name, sizeof(catalog_name))) {
        snprintf(catalog_name, sizeof(catalog_name), "%s", fallback_source_name ? fallback_source_name : "Pegasus-DL");
    }

    /* Przeszukaj wszystkie obiekty JSON {...} wewnątrz pliku */
    const char *p = json;
    while (p < end) {
        const char *obj_start = strchr(p, '{');
        if (!obj_start || obj_start >= end) break;

        /* Znajdź kolejny zagnieżdżony lub bieżący obiekt */
        const char *obj_end = find_matching_bracket(obj_start, end, '{', '}');
        if (!obj_end) break;

        /* Sprawdź, czy obiekt jest elementem płaskim (nie zawiera kolejnej tablicy packages/sources/catalogs) */
        bool has_nested_packages = find_limited(obj_start + 1, obj_end, "\"packages\"") ||
                                   find_limited(obj_start + 1, obj_end, "\"sources\"") ||
                                   find_limited(obj_start + 1, obj_end, "\"catalogs\"") ||
                                   find_limited(obj_start + 1, obj_end, "\"games\"");

        if (has_nested_packages) {
            /* Wchodzimy głębiej do środka głównego obiektu JSON */
            p = obj_start + 1;
            continue;
        }

        char title[128]      = {0};
        char id_code[32]     = {0};
        char version[32]     = {0};
        char desc[4096]      = {0};
        char cover_url[512]  = {0};
        char dl_url[512]     = {0};
        char fmt_str[32]     = {0};
        char category[32]    = {0};
        char created_at[64]  = {0};

        /* 1. Sprawdź czy to wpis gry (posiada titleId / cusa / ppsa lub title + downloadLinks/posterUrl) */
        bool has_id = json_extract_string(obj_start, obj_end, "titleId", id_code, sizeof(id_code)) ||
                      json_extract_string(obj_start, obj_end, "title_id", id_code, sizeof(id_code)) ||
                      json_extract_string(obj_start, obj_end, "id", id_code, sizeof(id_code)) ||
                      json_extract_string(obj_start, obj_end, "code", id_code, sizeof(id_code));

        bool has_title = json_extract_string(obj_start, obj_end, "title", title, sizeof(title)) ||
                         json_extract_string(obj_start, obj_end, "name", title, sizeof(title));

        bool has_cover = json_extract_string(obj_start, obj_end, "posterUrl", cover_url, sizeof(cover_url)) ||
                         json_extract_string(obj_start, obj_end, "coverUrl", cover_url, sizeof(cover_url)) ||
                         json_extract_string(obj_start, obj_end, "iconUrl", cover_url, sizeof(cover_url)) ||
                         json_extract_string(obj_start, obj_end, "image", cover_url, sizeof(cover_url)) ||
                         json_extract_string(obj_start, obj_end, "cover", cover_url, sizeof(cover_url)) ||
                         json_extract_string(obj_start, obj_end, "icon", cover_url, sizeof(cover_url));

        bool has_dl = json_extract_download_link(obj_start, obj_end, dl_url, sizeof(dl_url));

        /* Jeśli obiekt wskazuje na zewnętrzny pod-katalog JSON (w default-sources.json) */
        if (subcatalog_urls && subcatalog_count) {
            char sub_url[512] = {0};
            if (!json_extract_string(obj_start, obj_end, "catalogUrl", sub_url, sizeof(sub_url))) {
                if (!json_extract_string(obj_start, obj_end, "sourceUrl", sub_url, sizeof(sub_url))) {
                    json_extract_string(obj_start, obj_end, "url", sub_url, sizeof(sub_url));
                }
            }
            /* evoX-CoreOS is broken; do not re-add it from the upstream manifest. */
            char source_id[64] = {0}, source_name[64] = {0};
            json_extract_string(obj_start, obj_end, "id", source_id, sizeof(source_id));
            json_extract_string(obj_start, obj_end, "name", source_name, sizeof(source_name));
            if (strcasecmp(source_id, "evox-coreos") == 0 ||
                strcasecmp(source_name, "evox-coreos") == 0 ||
                strcasestr(sub_url, "/evox-coreos/") != NULL) {
                p = obj_end + 1;
                continue;
            }
            if (strncmp(sub_url, "http", 4) == 0 &&
                (strcasestr(sub_url, ".json") != NULL || (!has_id && !has_cover))) {
                bool dup = false;
                for (int k = 0; k < *subcatalog_count; k++) {
                    if (strcmp(subcatalog_urls[k], sub_url) == 0) {
                        dup = true;
                        break;
                    }
                }
                if (!dup && *subcatalog_count < max_subcatalogs) {
                    snprintf(subcatalog_urls[*subcatalog_count], 512, "%s", sub_url);
                    (*subcatalog_count)++;
                }
                /* Manifest descriptors are source references, not packages. */
                p = obj_end + 1;
                continue;
            }
        }

        /* Jeżeli obiekt opisuje grę/pakiet w katalogu Pegasus-DL */
        if (has_title && (has_id || has_cover || (has_dl && strcasestr(dl_url, ".json") == NULL))) {
            json_extract_string(obj_start, obj_end, "version", version, sizeof(version));
            json_extract_string(obj_start, obj_end, "description", desc, sizeof(desc));
            json_extract_string(obj_start, obj_end, "format", fmt_str, sizeof(fmt_str));
            json_extract_string(obj_start, obj_end, "category", category, sizeof(category));
            json_extract_string(obj_start, obj_end, "createdAt", created_at, sizeof(created_at));

            double size_bytes = json_extract_number(obj_start, obj_end, "sizeBytes");
            float size_gb = 0.0f;
            if (size_bytes > 1024.0 * 1024.0) {
                size_gb = (float)(size_bytes / (1024.0 * 1024.0 * 1024.0));
            } else {
                double raw_size = json_extract_number(obj_start, obj_end, "size");
                if (raw_size > 1024.0 * 1024.0) {
                    size_gb = (float)(raw_size / (1024.0 * 1024.0 * 1024.0));
                } else if (raw_size > 0.0) {
                    size_gb = (float)raw_size;
                }
            }

            PackageFormat fmt = detect_package_format(id_code, dl_url, fmt_str);
            register_parsed_game(app, catalog_name, id_code, title, version, desc, dl_url, cover_url,
                                 size_gb, fmt, category, parse_catalog_date(created_at), obj_start, obj_end);
            games_added++;
        }

        p = obj_end + 1;
    }

    return games_added;
}

/* Load the optional, curated metadata snapshot. Never crawl external websites on
   the PS5: this offline snapshot carries public title/page information only. */
static int load_local_catalog_file(WolodriveApp *app, int *source_count, const char *path) {
    FILE *file = fopen(path, "rb");
    if (!file) return 0;
    if (fseek(file, 0, SEEK_END) != 0) { fclose(file); return 0; }
    long size = ftell(file);
    if (size < 0 || size > 64 * 1024 * 1024 || fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return 0;
    }
    char *json = malloc((size_t)size + 1);
    if (!json) { fclose(file); return 0; }
    size_t count = fread(json, 1, (size_t)size, file);
    fclose(file);
    if (count != (size_t)size) { free(json); return 0; }
    json[count] = '\0';

    const char *end = json + count;
    const char *sources = json_object_value(json, end, "sources");
    if (!sources || *sources != '[') { free(json); return 0; }
    const char *last = find_matching_bracket(sources, end, '[', ']');
    if (!last) { free(json); return 0; }
    int added = 0;
    for (const char *p = sources + 1; p < last; ) {
        p = skip_ws(p, last);
        if (p >= last) break;
        if (*p != '{') { p++; continue; }
        const char *last_object = find_matching_bracket(p, last, '{', '}');
        if (!last_object) break;
        char name[64] = {0};
        if (json_extract_string(p, last_object, "name", name, sizeof(name)) && name[0]) {
            bool new_source = false;
            SDL_LockMutex(app->catalog_mutex);
            if (app->catalog_source_count < 40) {
                bool duplicate = false;
                for (int i = 0; i < app->catalog_source_count; i++)
                    if (strcasecmp(name, app->catalog_source_names[i]) == 0) duplicate = true;
                if (!duplicate) {
                    SDL_strlcpy(app->catalog_source_names[app->catalog_source_count++], name, 64);
                    new_source = true;
                }
            }
            SDL_UnlockMutex(app->catalog_mutex);
            size_t length = (size_t)(last_object - p + 1);
            char *source_json = malloc(length + 1);
            if (source_json) {
                memcpy(source_json, p, length);
                source_json[length] = '\0';
                added += parse_catalog_json(app, source_json, name, NULL, NULL, 0);
                free(source_json);
            }
            if (new_source) (*source_count)++;
        }
        p = last_object + 1;
    }
    free(json);
    return added;
}

static int load_additional_catalog(WolodriveApp *app, int *source_count) {
#ifdef __PS5__
    const char *additional = "/data/homebrew/wolodrive/catalogs/additional.json";
    const char *axekin = "/data/homebrew/wolodrive/catalogs/axekin.json";
#else
    const char *additional = "assets/catalogs/additional.json";
    const char *axekin = "assets/catalogs/axekin.json";
#endif
    int added = 0, before = *source_count;
    if (!app->additional_catalog_loaded) {
        added += load_local_catalog_file(app, source_count, additional);
        app->additional_catalog_loaded = *source_count > before;
    }
    if (!app->axekin_catalog_loaded) {
        int count_before = app->catalog_count;
        added += load_local_catalog_file(app, source_count, axekin);
        app->axekin_catalog_loaded = app->catalog_count > count_before;
    }
    return added;
}

static int catalog_sync_worker(void *userdata) {
    WolodriveApp *app = (WolodriveApp *)userdata;

    SDL_LockMutex(app->catalog_mutex);
    app->sync_in_progress = true;
    app->sync_status_id = TXT_SYNC_MANIFEST;
    SDL_UnlockMutex(app->catalog_mutex);

    char subcatalogs[24][512];
    int subcatalog_count = 0;
    int snapshot_sources = 0;
    int remote_games_total = load_additional_catalog(app, &snapshot_sources);

    char *manifest_json = http_get_string(PEGASUS_DEFAULT_SOURCES_URL);
    if (manifest_json) {
        remote_games_total += parse_catalog_json(app, manifest_json, "Pegasus-DL Default",
                                                 subcatalogs, &subcatalog_count, 24);
        free(manifest_json);

        /* Pobierz i sparsuj wszystkie pod-katalogi znalezione w default-sources.json */
        for (int i = 0; i < subcatalog_count && app->running; i++) {
            SDL_LockMutex(app->catalog_mutex);
            app->sync_status_id = TXT_SYNC_SUBCATALOG;
            app->sync_current = i + 1;
            app->sync_total = subcatalog_count;
            SDL_UnlockMutex(app->catalog_mutex);

            char *sub_json = http_get_string(subcatalogs[i]);
            if (sub_json) {
                remote_games_total += parse_catalog_json(app, sub_json, "Pegasus-DL Store", NULL, NULL, 0);
                free(sub_json);
            }
        }
    }

    SDL_LockMutex(app->catalog_mutex);
    app->sync_in_progress = false;
    app->sync_status_id = TXT_SYNC_DONE;
    app->sync_current = 0;
    app->sync_total = 0;
    app->sync_sources = subcatalog_count + snapshot_sources;
    app->catalog_dirty = true;
    SDL_UnlockMutex(app->catalog_mutex);

    if (remote_games_total > 0) {
        sys_send_notification(app, TR(app, TXT_SYNC_LOADED), remote_games_total);
    } else {
        sys_send_notification(app, TR(app, TXT_SYNC_DONE),
                              app->catalog_count, subcatalog_count);
    }

    return 0;
}

void catalog_start_async_sync(WolodriveApp *app) {
    SDL_LockMutex(app->catalog_mutex);
    if (app->sync_in_progress) {
        SDL_UnlockMutex(app->catalog_mutex);
        sys_send_notification(app, TR(app, TXT_SYNC_BUSY));
        return;
    }
    app->sync_in_progress = true;
    SDL_UnlockMutex(app->catalog_mutex);

    if (app->catalog_thread) SDL_WaitThread(app->catalog_thread, NULL);
    app->catalog_thread = SDL_CreateThread(catalog_sync_worker, "catalog", app);
    if (!app->catalog_thread) {
        SDL_LockMutex(app->catalog_mutex);
        app->sync_in_progress = false;
        SDL_UnlockMutex(app->catalog_mutex);
    }
}

void catalog_stop_sync(WolodriveApp *app) {
    if (app->catalog_thread) SDL_WaitThread(app->catalog_thread, NULL);
    app->catalog_thread = NULL;
}
