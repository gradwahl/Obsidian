#include "wolodrive.h"
#include <stdio.h>
#include <string.h>

bool settings_accepts_item(const WolodriveApp *app, const GameItem *item) {
    return (!app->filter_source[0] || SDL_strcasecmp(app->filter_source, item->source_name) == 0) &&
           (!app->filter_format || app->filter_format == (int)item->format + 1);
}

/* A bounded length-prefixed source name also supports spaces and newlines. */
bool settings_load(WolodriveApp *app) {
    app->filter_source[0] = '\0';
    app->filter_format = 0;
    FILE *file = fopen(app->settings_path, "rb");
    if (!file) return false;
    char header[32], line[32], source[64] = {0};
    int format = -1, length = -1;
    bool valid = fgets(header, sizeof(header), file) && strcmp(header, "WOLODRIVE_FILTERS_1\n") == 0 &&
        fgets(line, sizeof(line), file) && sscanf(line, "%d", &format) == 1 &&
        fgets(line, sizeof(line), file) && sscanf(line, "%d", &length) == 1 &&
        format >= 0 && format <= 3 && length >= 0 && length < (int)sizeof(source);
    if (valid) valid = fread(source, 1, (size_t)length, file) == (size_t)length && !memchr(source, '\0', (size_t)length);
    fclose(file);
    if (!valid) return false;
    SDL_strlcpy(app->filter_source, source, sizeof(app->filter_source));
    app->filter_format = format;
    return true;
}

bool settings_save(const WolodriveApp *app) {
    if (!app->settings_path[0]) return false;
    FILE *file = fopen(app->settings_path, "wb");
    if (!file) return false;
    size_t length = strlen(app->filter_source);
    bool written = fprintf(file, "WOLODRIVE_FILTERS_1\n%d\n%zu\n", app->filter_format, length) >= 0 &&
        fwrite(app->filter_source, 1, length, file) == length;
    if (fclose(file) != 0) written = false;
    return written;
}
