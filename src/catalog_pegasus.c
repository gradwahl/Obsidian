#include "wolodrive.h"
#include "text.h"
#include <curl/curl.h>
#include <pthread.h>
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

static size_t write_file_cb(void *ptr, size_t size, size_t nmemb, void *stream) {
    return fwrite(ptr, size, nmemb, (FILE *)stream);
}

static void ensure_directories(void) {
#ifdef __PS5__
    mkdir("/data/homebrew", 0777);
    mkdir(WOLODRIVE_DATA_PATH, 0777);
    mkdir(COVERS_CACHE_DIR, 0777);
#else
    mkdir("assets", 0777);
    mkdir(COVERS_CACHE_DIR, 0777);
#endif
}

static bool file_exists_and_valid(const char *path) {
    struct stat st;
    if (stat(path, &st) == 0 && st.st_size > 256) {
        return true;
    }
    return false;
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

static bool http_download_file(const char *url, const char *dest_path) {
    if (!url || strncmp(url, "http", 4) != 0) return false;
    if (file_exists_and_valid(dest_path)) return true;

    char tmp_path[300];
    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", dest_path);

    FILE *fp = fopen(tmp_path, "wb");
    if (!fp) return false;

    CURL *curl = curl_easy_init();
    if (!curl) {
        fclose(fp);
        remove(tmp_path);
        return false;
    }

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_file_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Wolodrive/2.0 (PS5-13.60; CoverCache)");
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 8L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 20L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

    CURLcode res = curl_easy_perform(curl);
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    curl_easy_cleanup(curl);
    fclose(fp);

    if (res == CURLE_OK && http_code >= 200 && http_code < 300 && file_exists_and_valid(tmp_path)) {
        rename(tmp_path, dest_path);
        return true;
    }
    remove(tmp_path);
    return false;
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

/* Wyciąga wartość tekstową dla podanego klucza w obrębie [obj_start, obj_end] */
static bool json_extract_string(const char *obj_start, const char *obj_end, const char *key, char *out, size_t out_sz) {
    char pattern[128];
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    size_t pat_len = strlen(pattern);

    const char *p = obj_start;
    while (p + pat_len < obj_end) {
        const char *hit = strstr(p, pattern);
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
        const char *hit = strstr(p, pattern);
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
    const char *dl = strstr(obj_start, "\"downloadLinks\"");
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

static void sanitize_filename(const char *input, char *output, size_t max_len) {
    size_t j = 0;
    for (size_t i = 0; input[i] != '\0' && j + 1 < max_len; i++) {
        char c = input[i];
        if (isalnum((unsigned char)c) || c == '_' || c == '-') {
            output[j++] = c;
        } else {
            output[j++] = '_';
        }
    }
    output[j] = '\0';
}

static PackageFormat detect_package_format(const char *id_code, const char *dl_url, const char *fmt_str) {
    if (fmt_str && fmt_str[0]) {
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

static bool newer_game(const GameItem *a, const GameItem *b) {
    if (a->catalog_added_at != b->catalog_added_at) return a->catalog_added_at > b->catalog_added_at;
    int title_order = strcmp(a->title, b->title);
    return title_order ? title_order < 0 : strcmp(a->id_code, b->id_code) < 0;
}

int catalog_latest_games_locked(WolodriveApp *app, GameItem **items, int capacity) {
    int count = 0;
    if (capacity <= 0) return 0;
    for (int i = 0; i < app->catalog_count; i++) {
        GameItem *item = &app->catalog[i];
        if (!item->is_game || !item->catalog_added_at || !settings_accepts_item(app, item)) continue;
        /* Retain source packages in storage, but show one latest dated copy of
           an identically named title per console family on Home. */
        bool duplicate = false;
        for (int j = 0; j < count; j++) {
            if (SDL_strcasecmp(item->title, items[j]->title) != 0 ||
                SDL_strncasecmp(item->id_code, items[j]->id_code, 4) != 0) continue;
            if (!newer_game(item, items[j])) { duplicate = true; break; }
            for (int k = j; k+1 < count; k++) items[k] = items[k+1];
            count--;
            break;
        }
        if (duplicate) continue;
        int position = 0;
        while (position < count && !newer_game(item, items[position])) position++;
        if (position >= capacity) continue;
        if (count < capacity) count++;
        for (int j = count - 1; j > position; j--) items[j] = items[j - 1];
        items[position] = item;
    }
    return count;
}

/* Prioritize Home, then the supported consoles' visible catalog rows. */
static int cover_prefetch_locked(WolodriveApp *app, GameItem **items) {
    int count = catalog_latest_games_locked(app, items, MAX_ITEMS_PER_ROW);
    for (int company = 0; company < library_company_count(); company++) {
        const LibraryCompany *brand = library_company_at(company);
        for (int c = 0; c < brand->console_count; c++) {
            const LibraryConsole *console = &brand->consoles[c];
            if (!console->title_id_prefix) continue;
            int matched = 0;
            for (int i = 0; i < app->catalog_count && matched < MAX_ITEMS_PER_ROW; i++) {
                GameItem *item = &app->catalog[i];
                if (!item->is_game || !library_console_matches_id(console, item->id_code)) continue;
                matched++;
                bool duplicate = false;
                for (int j = 0; j < count; j++) {
                    if (items[j] == item) { duplicate = true; break; }
                }
                if (!duplicate && count < MAX_COVER_PREFETCH) items[count++] = item;
            }
        }
    }
    return count;
}

static void json_extract_screenshots(const char *start, const char *end, GameItem *item) {
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
    for (int i = 0; i < app->catalog_count; i++) {
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
    snprintf(target->title, sizeof(target->title), "%s", title);
    snprintf(target->version, sizeof(target->version), "%s", (version && version[0]) ? version : "1.00");
    snprintf(target->source_name, sizeof(target->source_name), "%s", (catalog_name && catalog_name[0]) ? catalog_name : "Pegasus-DL");
    if (category && category[0]) target->is_game = strcasecmp(category, "game") == 0;
    if (category && category[0]) target->is_dlc = strcasecmp(category, "dlc") == 0;
    char parent[32] = {0};
    if (json_extract_string(object_start, object_end, "baseTitleId", parent, sizeof(parent)) ||
        json_extract_string(object_start, object_end, "parentTitleId", parent, sizeof(parent)))
        SDL_strlcpy(target->parent_title_id, parent, sizeof(target->parent_title_id));
    json_extract_screenshots(object_start, object_end, target);
    /* A newer source for the same game counts as its newest catalog addition. */
    if (added_at > target->catalog_added_at) target->catalog_added_at = added_at;

    text_plain_description(desc, target->description, sizeof(target->description));
    target->description_id = target->description[0] ? TEXT_COUNT : TXT_DESCRIPTION_FALLBACK;

    if (dl_url && dl_url[0]) {
        snprintf(target->download_url, sizeof(target->download_url), "%s", dl_url);
    }
    if (cover_url && cover_url[0]) {
        snprintf(target->cover_url, sizeof(target->cover_url), "%s", cover_url);
    }

    target->format  = fmt;
    target->size_gb = (size_gb > 0.05f) ? size_gb : 42.5f;

    if (fmt == FMT_PKG) {
        snprintf(target->install_path, sizeof(target->install_path), "/data/pkg/%s.pkg", target->id_code);
    } else if (fmt == FMT_FFPKG) {
        snprintf(target->install_path, sizeof(target->install_path), "/data/homebrew/%s", target->id_code);
    } else {
        snprintf(target->install_path, sizeof(target->install_path), "/data/homebrew/%s/%s.exfat", target->id_code, target->id_code);
    }

    generate_color_from_id(target->id_code, &target->cover_r, &target->cover_g, &target->cover_b);

    char clean_id[64];
    sanitize_filename(target->id_code, clean_id, sizeof(clean_id));
    snprintf(target->cover_local_path, sizeof(target->cover_local_path), "%s/%s.cover", COVERS_CACHE_DIR, clean_id);

    if (file_exists_and_valid(target->cover_local_path)) {
        target->cover_ready_on_disk = true;
    }

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
        bool has_nested_packages = (strstr(obj_start + 1, "\"packages\"") != NULL && strstr(obj_start + 1, "\"packages\"") < obj_end) ||
                                   (strstr(obj_start + 1, "\"sources\"") != NULL && strstr(obj_start + 1, "\"sources\"") < obj_end) ||
                                   (strstr(obj_start + 1, "\"catalogs\"") != NULL && strstr(obj_start + 1, "\"catalogs\"") < obj_end) ||
                                   (strstr(obj_start + 1, "\"games\"") != NULL && strstr(obj_start + 1, "\"games\"") < obj_end);

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
        if (subcatalog_urls && subcatalog_count && *subcatalog_count < max_subcatalogs) {
            char sub_url[512] = {0};
            if (!json_extract_string(obj_start, obj_end, "catalogUrl", sub_url, sizeof(sub_url))) {
                if (!json_extract_string(obj_start, obj_end, "sourceUrl", sub_url, sizeof(sub_url))) {
                    json_extract_string(obj_start, obj_end, "url", sub_url, sizeof(sub_url));
                }
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
                if (!dup) {
                    snprintf(subcatalog_urls[*subcatalog_count], 512, "%s", sub_url);
                    (*subcatalog_count)++;
                }
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

static void *catalog_sync_worker(void *userdata) {
    WolodriveApp *app = (WolodriveApp *)userdata;
    ensure_directories();

    SDL_LockMutex(app->catalog_mutex);
    app->sync_in_progress = true;
    app->sync_status_id = TXT_SYNC_MANIFEST;
    SDL_UnlockMutex(app->catalog_mutex);

    char subcatalogs[24][512];
    int subcatalog_count = 0;
    int remote_games_total = 0;

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

    /* Krok 2: Pobieranie i cache'owanie okładek do assets/covers/ dla wszystkich pozycji w katalogu */
    int cached_ok = 0;
    int total_items = 0;
    GameItem *cover_items[MAX_COVER_PREFETCH];

    SDL_LockMutex(app->catalog_mutex);
    total_items = cover_prefetch_locked(app, cover_items);
    SDL_UnlockMutex(app->catalog_mutex);

    for (int i = 0; i < total_items && app->running; i++) {
        char url[512] = {0};
        char local_path[256] = {0};
        char title[128] = {0};

        SDL_LockMutex(app->catalog_mutex);
        snprintf(url, sizeof(url), "%s", cover_items[i]->cover_url);
        snprintf(local_path, sizeof(local_path), "%s", cover_items[i]->cover_local_path);
        snprintf(title, sizeof(title), "%s", cover_items[i]->title);
        SDL_UnlockMutex(app->catalog_mutex);

        if (url[0] != '\0') {
            if (!file_exists_and_valid(local_path)) {
                SDL_LockMutex(app->catalog_mutex);
                app->sync_status_id = TXT_SYNC_COVER;
                app->sync_current = i + 1;
                app->sync_total = total_items;
                snprintf(app->sync_title, sizeof(app->sync_title), "%s", title);
                SDL_UnlockMutex(app->catalog_mutex);

                http_download_file(url, local_path);
            }

            if (file_exists_and_valid(local_path)) {
                SDL_LockMutex(app->catalog_mutex);
                cover_items[i]->cover_ready_on_disk = true;
                cached_ok++;
                app->covers_cached_count = cached_ok;
                SDL_UnlockMutex(app->catalog_mutex);
            }
        }
    }

    SDL_LockMutex(app->catalog_mutex);
    app->sync_in_progress = false;
    app->sync_status_id = TXT_SYNC_DONE;
    app->sync_current = cached_ok;
    app->sync_total = total_items;
    app->sync_sources = subcatalog_count;
    app->catalog_dirty = true;
    SDL_UnlockMutex(app->catalog_mutex);

    if (remote_games_total > 0) {
        sys_send_notification(app, TR(app, TXT_SYNC_LOADED), remote_games_total);
    } else {
        sys_send_notification(app, TR(app, TXT_SYNC_CACHED),
                              subcatalog_count, cached_ok);
    }

    return NULL;
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

    pthread_t tid;
    if (pthread_create(&tid, NULL, catalog_sync_worker, app) == 0) {
        pthread_detach(tid);
    } else {
        SDL_LockMutex(app->catalog_mutex);
        app->sync_in_progress = false;
        SDL_UnlockMutex(app->catalog_mutex);
    }
}

void catalog_free_textures(WolodriveApp *app) {
    for (int i = 0; i < app->catalog_count; i++) {
        if (app->catalog[i].cover_tex) {
            SDL_DestroyTexture(app->catalog[i].cover_tex);
            app->catalog[i].cover_tex = NULL;
        }
    }
}
