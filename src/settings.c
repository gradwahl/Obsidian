#include "wolodrive.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

bool settings_source_enabled(const WolodriveApp *app, const char *source) {
    if (!app->filters.sources_limited) return true;
    for (int i = 0; i < app->filters.source_count; i++)
        if (SDL_strcasecmp(app->filters.sources[i], source) == 0) return true;
    return false;
}
bool settings_filters_active(const WolodriveApp *app) {
    return app->filters.sources_limited || app->filters.disabled_formats != 0;
}
bool settings_accepts_item(const WolodriveApp *app, const GameItem *item) {
    return settings_source_enabled(app, item->source_name) &&
        item->format >= FMT_EXFAT && item->format <= FMT_CATALOG &&
        !(app->filters.disabled_formats & (1u << item->format));
}
static bool read_integer(FILE *file, int *value) {
    char line[32], extra;
    return fgets(line, sizeof(line), file) && sscanf(line, "%d %c", value, &extra) == 1;
}
static bool read_source(FILE *file, char source[64]) {
    int length;
    if (!read_integer(file, &length) || length < 0 || length >= 64) return false;
    if (fread(source, 1, (size_t)length, file) != (size_t)length || memchr(source, '\0', (size_t)length)) return false;
    source[length] = '\0';
    return true;
}
bool settings_load(WolodriveApp *app) {
    memset(&app->filters, 0, sizeof(app->filters));
    FILE *file = fopen(app->settings_path, "rb");
    if (!file) return false;
    CatalogFilters *loaded = calloc(1, sizeof(*loaded));
    if (!loaded) { fclose(file); return false; }
    char header[32];
    int format = -1, count = -2;
    bool valid = fgets(header, sizeof(header), file) != NULL && read_integer(file, &format);
    if (valid && strcmp(header, "WOLODRIVE_FILTERS_1\n") == 0) {
        valid = format >= 0 && format <= 3 && read_source(file, loaded->sources[0]);
        if (valid) {
            loaded->disabled_formats = format ? 7u ^ (1u << (format-1)) : 0;
            loaded->sources_limited = loaded->sources[0][0] != '\0';
            loaded->source_count = loaded->sources_limited ? 1 : 0;
        }
    } else if (valid && strcmp(header, "WOLODRIVE_FILTERS_2\n") == 0) {
        valid = format >= 0 && format <= 15 && read_integer(file, &count) && count >= -1 && count <= MAX_CATALOG_ITEMS;
        loaded->disabled_formats = (unsigned)format;
        loaded->sources_limited = count >= 0;
        for (int i = 0; valid && i < count; i++) {
            valid = read_source(file, loaded->sources[i]) && loaded->sources[i][0];
            for (int j = 0; valid && j < i; j++)
                if (SDL_strcasecmp(loaded->sources[i], loaded->sources[j]) == 0) valid = false;
        }
        loaded->source_count = count > 0 ? count : 0;
    } else valid = false;
    fclose(file);
    if (valid) app->filters = *loaded;
    free(loaded);
    return valid;
}
bool settings_save(const WolodriveApp *app) {
    if (!app->settings_path[0]) return false;
    FILE *file = fopen(app->settings_path, "wb");
    if (!file) return false;
    int count = app->filters.sources_limited ? app->filters.source_count : -1;
    bool written = fprintf(file, "WOLODRIVE_FILTERS_2\n%u\n%d\n", app->filters.disabled_formats, count) >= 0;
    for (int i = 0; written && i < count; i++) {
        size_t length = strlen(app->filters.sources[i]);
        written = fprintf(file, "%zu\n", length) >= 0 && fwrite(app->filters.sources[i], 1, length, file) == length;
    }
    if (fclose(file) != 0) written = false;
    return written;
}

/* Theme preference is separate from filter serialization, preserving old files. */
static bool theme_path(const WolodriveApp *app, char *dst, size_t cap) {
    if (!app->settings_path[0]) return false;
    int n = snprintf(dst, cap, "%s.theme", app->settings_path);
    return n > 0 && (size_t)n < cap;
}
bool theme_load(WolodriveApp *app) {
    app->ui_theme = UI_THEME_NETFLIX;
    char path[sizeof(app->settings_path) + 16];
    if (!theme_path(app, path, sizeof(path))) return false;
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    char header[64];
    int choice = -1;
    bool valid = fgets(header, sizeof(header), f) &&
                 strcmp(header, "WOLODRIVE_THEME_1\n") == 0 &&
                 read_integer(f, &choice) && (choice == UI_THEME_NETFLIX || choice == UI_THEME_UBIQUITY);
    fclose(f);
    if (valid) app->ui_theme = choice;
    return valid;
}
bool theme_save(const WolodriveApp *app) {
    char path[sizeof(app->settings_path) + 16];
    if (!theme_path(app, path, sizeof(path))) return false;
    FILE *f = fopen(path, "wb");
    if (!f) return false;
    bool valid = fprintf(f, "WOLODRIVE_THEME_1\n%d\n", app->ui_theme) > 0;
    if (fclose(f) != 0) valid = false;
    return valid;
}
