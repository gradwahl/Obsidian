#include "wolodrive.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include "../src/ui_netflix.c"

static TranslationStatus wait_for_result(TranslationService *service, const char *source,
                                         Language target, char *out, size_t capacity) {
    Uint32 started = SDL_GetTicks();
    TranslationStatus status;
    do {
        status = translation_get(service, source, target, out, capacity);
        if (status != TRANSLATION_PENDING) return status;
        SDL_Delay(5);
    } while (SDL_GetTicks() - started < 4000);
    assert(!"Translation worker did not finish");
    return TRANSLATION_FAILED;
}

int main(void) {
    assert(SDL_Init(SDL_INIT_TIMER) == 0);
    assert(curl_global_init(CURL_GLOBAL_ALL) == CURLE_OK);
    char *url = SDL_strdup(SDL_getenv("WOLODRIVE_TRANSLATE_URL"));
    assert(url);
    SDL_setenv("WOLODRIVE_TRANSLATE_URL", "", 1);
    assert(translation_create() == NULL);
    char output[MAX_TRANSLATED_DESCRIPTION_BYTES];
    assert(translation_get(NULL, "original", LANG_PL, output, sizeof(output)) == TRANSLATION_UNCONFIGURED);
    SDL_setenv("WOLODRIVE_TRANSLATE_URL", url, 1);
    TranslationService *service = translation_create();
    assert(service);
    const char *source = "Quotes: \"test\" & newline\n中文 Русский";
    for (int language = 0; language < LANG_COUNT; language++) {
        assert(wait_for_result(service, source, (Language)language, output, sizeof(output)) == TRANSLATION_READY);
        char prefix[8];
        snprintf(prefix, sizeof(prefix), "[%s]", i18n_language_code((Language)language));
        assert(strncmp(output, prefix, strlen(prefix)) == 0);
        assert(strstr(output, source));
    }
    assert(translation_get(service, source, LANG_PL, output, sizeof(output)) == TRANSLATION_READY);
    char small[2];
    assert(translation_get(service, source, LANG_PL, small, sizeof(small)) == TRANSLATION_FAILED);
    assert(small[0] == '\0');
    assert(wait_for_result(service, "updated text", LANG_PL, output, sizeof(output)) == TRANSLATION_READY);
    char long_source[MAX_DESCRIPTION_BYTES];
    memset(long_source, 'a', sizeof(long_source) - 1);
    long_source[sizeof(long_source) - 1] = '\0';
    assert(wait_for_result(service, long_source, LANG_PL, output, sizeof(output)) == TRANSLATION_READY);
    assert(strstr(output, long_source));
    const char *failures[] = {"error", "malformed", "nested", "oversized"};
    for (size_t i = 0; i < sizeof(failures) / sizeof(failures[0]); i++) {
        assert(wait_for_result(service, failures[i], LANG_PL, output, sizeof(output)) == TRANSLATION_FAILED);
        assert(translation_get(service, failures[i], LANG_PL, output, sizeof(output)) == TRANSLATION_FAILED);
    }
    translation_get(service, "slow request", LANG_PL, output, sizeof(output));
    SDL_Delay(50);
    translation_get(service, "queued old", LANG_FR, output, sizeof(output));
    translation_get(service, "queued latest", LANG_DE, output, sizeof(output));
    assert(wait_for_result(service, "queued latest", LANG_DE, output, sizeof(output)) == TRANSLATION_READY);
    assert(strncmp(output, "[de]", 4) == 0);
    /* Source changes and current locale determine the UI text, never a previous result. */
    WolodriveApp *app = calloc(1, sizeof(*app));
    assert(app);
    app->catalog_mutex = SDL_CreateMutex();
    app->catalog_count = 1;
    app->translations = service;
    GameItem *item = &app->catalog[0];
    item->description_id = TEXT_COUNT;
    SDL_strlcpy(item->description, source, sizeof(item->description));
    SDL_AtomicSet(&app->language, LANG_PL);
    description_text(app, item, output, sizeof(output));
    assert(strncmp(output, "[pl]", 4) == 0);
    app->show_original_description = true;
    description_text(app, item, output, sizeof(output));
    assert(strcmp(output, source) == 0);
    sys_set_language(app, LANG_FR);
    assert(!app->show_original_description);
    description_text(app, item, output, sizeof(output));
    assert(strncmp(output, "[fr]", 4) == 0);
    app->translations = NULL;
    SDL_AtomicSet(&app->language, LANG_PL);
    description_text(app, item, output, sizeof(output));
    assert(strcmp(output, TR(app, TXT_TRANSLATION_UNCONFIGURED)) == 0);
    SDL_DestroyMutex(app->catalog_mutex);
    free(app);
    /* Exercise bounded-cache eviction and reuse, including owned UTF-8 strings. */
    for (int i = 0; i < 80; i++) {
        char text[32];
        snprintf(text, sizeof(text), "eviction-%d", i);
        assert(wait_for_result(service, text, LANG_PL, output, sizeof(output)) == TRANSLATION_READY);
    }
    assert(translation_get(service, "eviction-0", LANG_PL, output, sizeof(output)) == TRANSLATION_PENDING);
    assert(wait_for_result(service, "eviction-0", LANG_PL, output, sizeof(output)) == TRANSLATION_READY);
    translation_get(service, "slow shutdown", LANG_PL, output, sizeof(output));
    SDL_Delay(50);
    Uint32 started = SDL_GetTicks();
    translation_destroy(service);
    assert(SDL_GetTicks() - started < 3000);
    SDL_free(url);
    curl_global_cleanup();
    SDL_Quit();
    puts("Online translation contract, all nine targets, cache, source invalidation, errors, latest-request queue and shutdown passed.");
    return 0;
}
