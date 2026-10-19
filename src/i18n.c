#include "i18n.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *const codes[] = { "en", "pl", "es", "fr", "nl", "de", "it", "zh", "ru" };
static const char *const names[] = { "English", "Polski", "Español", "Français", "Nederlands", "Deutsch", "Italiano", "中文", "Русский" };
static const char *const translations[TEXT_COUNT][LANG_COUNT] = {
#define TEXT(id, en, pl, es, fr, nl, de, it, zh, ru) [id] = { en, pl, es, fr, nl, de, it, zh, ru },
#include "translations.def"
#undef TEXT
};

static bool valid_language(Language language) {
    return language >= LANG_EN && language < LANG_COUNT;
}

const char *i18n_text(Language language, TextId id) {
    if (id < 0 || id >= TEXT_COUNT) return "";
    if (!valid_language(language)) language = LANG_EN;
    const char *text = translations[id][language];
    return text && text[0] ? text : translations[id][LANG_EN];
}

const char *i18n_language_name(Language language) {
    return names[valid_language(language) ? language : LANG_EN];
}

static bool parse_language(const char *locale, Language *language) {
    if (!locale || !locale[0] || !locale[1]) return false;
    if (locale[2] && locale[2] != '_' && locale[2] != '-' &&
        locale[2] != '.' && locale[2] != '@' && locale[2] != ':') return false;
    for (int i = 0; i < LANG_COUNT; i++) {
        if (tolower((unsigned char)locale[0]) == codes[i][0] &&
            tolower((unsigned char)locale[1]) == codes[i][1]) {
            *language = (Language)i;
            return true;
        }
    }
    return false;
}

Language i18n_detect_language(void) {
    const char *variables[] = { "LC_ALL", "LC_MESSAGES", "LANG" };
    for (unsigned i = 0; i < sizeof(variables) / sizeof(variables[0]); i++) {
        const char *locale = getenv(variables[i]);
        if (locale && locale[0]) {
            Language language;
            return parse_language(locale, &language) ? language : LANG_EN;
        }
    }
    return LANG_EN;
}

Language i18n_load_language(const char *path, Language fallback) {
    FILE *file = path && path[0] ? fopen(path, "r") : NULL;
    if (!file) return valid_language(fallback) ? fallback : LANG_EN;
    char code[16];
    Language language;
    bool loaded = fgets(code, sizeof(code), file) != NULL;
    fclose(file);
    if (loaded) {
        code[strcspn(code, "\r\n")] = '\0';
        for (int i = 0; i < LANG_COUNT; i++) {
            if (strcmp(code, codes[i]) == 0) return (Language)i;
        }
    }
    language = valid_language(fallback) ? fallback : LANG_EN;
    return language;
}

bool i18n_save_language(const char *path, Language language) {
    if (!path || !path[0] || !valid_language(language)) return false;
    FILE *file = fopen(path, "w");
    if (!file) return false;
    bool saved = fprintf(file, "%s\n", codes[language]) > 0;
    return fclose(file) == 0 && saved;
}
