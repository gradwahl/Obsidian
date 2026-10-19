#include "translation.h"
#include "text.h"
#include <SDL2/SDL.h>
#include <curl/curl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TRANSLATION_CACHE_SIZE 64
#define RESPONSE_LIMIT (MAX_TRANSLATED_DESCRIPTION_BYTES - 1)
#define ENTRY_IDLE (-1)

typedef struct {
    char *source;
    char *result;
    Language target;
    int status;
    Uint32 failed_at;
    unsigned long long accessed;
} TranslationEntry;

struct TranslationService {
    SDL_mutex *mutex;
    SDL_cond *changed;
    SDL_Thread *worker;
    SDL_atomic_t stopping;
    char *url;
    char *key;
    TranslationEntry entries[TRANSLATION_CACHE_SIZE];
    int pending, working;
    unsigned long long clock;
};

typedef struct { char *data; size_t size; } Response;

static size_t receive_response(char *data, size_t size, size_t count, void *context) {
    Response *response = context;
    if (size && count > RESPONSE_LIMIT / size) return 0;
    size_t bytes = size * count;
    if (bytes > RESPONSE_LIMIT - response->size) return 0;
    char *buffer = realloc(response->data, response->size + bytes + 1);
    if (!buffer) return 0;
    response->data = buffer;
    memcpy(buffer + response->size, data, bytes);
    response->size += bytes;
    buffer[response->size] = '\0';
    return bytes;
}

static int request_progress(void *context, curl_off_t total, curl_off_t now,
                            curl_off_t upload_total, curl_off_t uploaded) {
    (void)total; (void)now; (void)upload_total; (void)uploaded;
    TranslationService *service = context;
    return SDL_AtomicGet(&service->stopping);
}

static const char *end_of_string(const char *text, const char *end) {
    while (text < end) {
        if (*text == '"') return text;
        if (*text++ == '\\' && text < end) text++;
    }
    return end;
}

/* Read only a root string field, never an error message or a nested result. */
static char *translated_text(const char *json, size_t size) {
    const char *p = json, *end = json + size;
    int depth = 0;
    while (p < end && SDL_isspace((unsigned char)*p)) p++;
    if (p == end || *p != '{') return NULL;
    while (p < end) {
        if (*p == '{' || *p == '[') depth++;
        else if (*p == '}' || *p == ']') depth--;
        else if (*p == '"') {
            const char *close = end_of_string(p + 1, end);
            if (close == end) return NULL;
            char key[64];
            bool key_valid = text_decode_json_string(p + 1, close + 1, key, sizeof(key));
            p = close + 1;
            while (p < end && SDL_isspace((unsigned char)*p)) p++;
            if (depth == 1 && key_valid && !strcmp(key, "translatedText") && p < end && *p == ':') {
                p++;
                while (p < end && SDL_isspace((unsigned char)*p)) p++;
                if (p == end || *p != '"') return NULL;
                char *result = malloc(size + 1);
                if (!result) return NULL;
                if (text_decode_json_string(p + 1, end, result, size + 1)) return result;
                free(result);
                return NULL;
            }
            continue;
        }
        p++;
    }
    return NULL;
}

static char *request_translation(TranslationService *service, TranslationEntry *entry) {
    CURL *curl = curl_easy_init();
    if (!curl) return NULL;
    char *query = curl_easy_escape(curl, entry->source, 0);
    char *key = curl_easy_escape(curl, service->key, 0);
    char *body = NULL;
    Response response = {0};
    char *result = NULL;
    if (!query || !key) goto cleanup;
    size_t capacity = strlen(query) + strlen(key) + 128;
    body = malloc(capacity);
    if (!body) goto cleanup;
    snprintf(body, capacity, "q=%s&source=auto&target=%s&format=text&api_key=%s",
             query, i18n_language_code(entry->target), key);
    curl_easy_setopt(curl, CURLOPT_URL, service->url);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, receive_response);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, 3000L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, 10000L);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, request_progress);
    curl_easy_setopt(curl, CURLOPT_XFERINFODATA, service);
    /* TLS verification stays enabled. Redirects are not followed with credentials. */
    CURLcode status = curl_easy_perform(curl);
    long http_status = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_status);
    if (status == CURLE_OK && http_status == 200 && response.data) {
        result = translated_text(response.data, response.size);
    }
cleanup:
    free(response.data);
    free(body);
    curl_free(query);
    curl_free(key);
    curl_easy_cleanup(curl);
    return result;
}

static int translation_worker(void *context) {
    TranslationService *service = context;
    SDL_LockMutex(service->mutex);
    while (!SDL_AtomicGet(&service->stopping)) {
        if (service->pending < 0) {
            SDL_CondWait(service->changed, service->mutex);
            continue;
        }
        int index = service->pending;
        service->pending = -1;
        service->working = index;
        TranslationEntry *entry = &service->entries[index];
        SDL_UnlockMutex(service->mutex);
        char *result = request_translation(service, entry);
        SDL_LockMutex(service->mutex);
        entry->result = result;
        entry->status = result ? TRANSLATION_READY : TRANSLATION_FAILED;
        entry->failed_at = SDL_GetTicks();
        service->working = -1;
    }
    SDL_UnlockMutex(service->mutex);
    return 0;
}

TranslationService *translation_create(void) {
    const char *url = SDL_getenv("WOLODRIVE_TRANSLATE_URL");
    if (!url || !*url || (strncmp(url, "https://", 8) && strncmp(url, "http://", 7))) return NULL;
    TranslationService *service = SDL_calloc(1, sizeof(*service));
    if (!service) return NULL;
    service->pending = service->working = -1;
    service->url = SDL_strdup(url);
    const char *key = SDL_getenv("WOLODRIVE_TRANSLATE_API_KEY");
    service->key = SDL_strdup(key ? key : "");
    service->mutex = SDL_CreateMutex();
    service->changed = SDL_CreateCond();
    if (!service->url || !service->key || !service->mutex || !service->changed) {
        translation_destroy(service);
        return NULL;
    }
    service->worker = SDL_CreateThread(translation_worker, "description-translation", service);
    if (!service->worker) { translation_destroy(service); return NULL; }
    return service;
}

void translation_destroy(TranslationService *service) {
    if (!service) return;
    SDL_AtomicSet(&service->stopping, 1);
    if (service->worker) {
        SDL_LockMutex(service->mutex);
        SDL_CondSignal(service->changed);
        SDL_UnlockMutex(service->mutex);
        SDL_WaitThread(service->worker, NULL);
    }
    for (int i = 0; i < TRANSLATION_CACHE_SIZE; i++) {
        SDL_free(service->entries[i].source);
        free(service->entries[i].result);
    }
    if (service->mutex) SDL_DestroyMutex(service->mutex);
    if (service->changed) SDL_DestroyCond(service->changed);
    SDL_free(service->url);
    SDL_free(service->key);
    SDL_free(service);
}

TranslationStatus translation_get(TranslationService *service, const char *source,
                                  Language target, char *out, size_t capacity) {
    if (out && capacity) out[0] = '\0';
    if (!service) return TRANSLATION_UNCONFIGURED;
    if (!source || !*source || target < LANG_EN || target >= LANG_COUNT || !out || !capacity) return TRANSLATION_FAILED;
    SDL_LockMutex(service->mutex);
    int index = -1, oldest = -1;
    for (int i = 0; i < TRANSLATION_CACHE_SIZE; i++) {
        TranslationEntry *entry = &service->entries[i];
        if (entry->source && entry->target == target && !strcmp(entry->source, source)) { index = i; break; }
        if (i != service->working && (oldest < 0 || entry->accessed < service->entries[oldest].accessed)) oldest = i;
    }
    if (index < 0) {
        index = oldest;
        TranslationEntry *entry = &service->entries[index];
        SDL_free(entry->source);
        free(entry->result);
        memset(entry, 0, sizeof(*entry));
        entry->source = SDL_strdup(source);
        entry->target = target;
        entry->status = ENTRY_IDLE;
    }
    if (service->pending >= 0 && service->pending != index) {
        service->entries[service->pending].status = ENTRY_IDLE;
        service->pending = -1;
    }
    TranslationEntry *entry = &service->entries[index];
    entry->accessed = ++service->clock;
    if (!entry->source) entry->status = TRANSLATION_FAILED;
    if (entry->status == TRANSLATION_FAILED && entry->source && SDL_GetTicks() - entry->failed_at >= 30000) entry->status = ENTRY_IDLE;
    if (entry->status == ENTRY_IDLE) {
        entry->status = TRANSLATION_PENDING;
        service->pending = index;
        SDL_CondSignal(service->changed);
    }
    TranslationStatus status = (TranslationStatus)entry->status;
    if (status == TRANSLATION_READY) {
        if (strlen(entry->result) >= capacity) status = TRANSLATION_FAILED;
        else SDL_strlcpy(out, entry->result, capacity);
    }
    SDL_UnlockMutex(service->mutex);
    return status;
}
