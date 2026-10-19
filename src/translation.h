#ifndef WOLODRIVE_TRANSLATION_H
#define WOLODRIVE_TRANSLATION_H

#include "i18n.h"
#include <stddef.h>

#define MAX_TRANSLATED_DESCRIPTION_BYTES (32768 + 1)

typedef struct TranslationService TranslationService;
typedef enum {
    TRANSLATION_UNCONFIGURED,
    TRANSLATION_PENDING,
    TRANSLATION_READY,
    TRANSLATION_FAILED
} TranslationStatus;

/* Configuration is read once from WOLODRIVE_TRANSLATE_URL and
   WOLODRIVE_TRANSLATE_API_KEY. No endpoint is contacted unless configured. */
TranslationService *translation_create(void);
void translation_destroy(TranslationService *service);
TranslationStatus translation_get(TranslationService *service, const char *source,
                                  Language target, char *out, size_t capacity);

#endif
