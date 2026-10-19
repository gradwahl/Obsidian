#ifndef WOLODRIVE_I18N_H
#define WOLODRIVE_I18N_H

#include <stdbool.h>

typedef enum { LANG_EN, LANG_PL, LANG_ES, LANG_FR, LANG_NL, LANG_DE, LANG_IT, LANG_ZH, LANG_RU, LANG_COUNT } Language;
typedef enum {
#define TEXT(id, en, pl, es, fr, nl, de, it, zh, ru) id,
#include "translations.def"
#undef TEXT
    TEXT_COUNT
} TextId;

const char *i18n_text(Language language, TextId id);
const char *i18n_language_name(Language language);
const char *i18n_language_code(Language language);
Language i18n_detect_language(void);
Language i18n_load_language(const char *path, Language fallback);
bool i18n_save_language(const char *path, Language language);

#endif
