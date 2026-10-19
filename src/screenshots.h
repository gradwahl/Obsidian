#ifndef WOLODRIVE_SCREENSHOTS_H
#define WOLODRIVE_SCREENSHOTS_H
#include "wolodrive.h"

typedef enum { SHOTS_PENDING, SHOTS_READY, SHOTS_EMPTY, SHOTS_FAILED } ScreenshotState;
typedef struct {
    ScreenshotState state;
    int count;
    char images[MAX_GAME_SCREENSHOTS][512];
    char source[40];
    char page[512];
} ScreenshotSet;

/* A transport seam keeps parser/worker tests offline. Returned HTML is malloc-owned. */
typedef char *(*ScreenshotFetch)(void *userdata, const char *url);
struct ScreenshotService *screenshots_create(ScreenshotFetch fetch, void *userdata);
void screenshots_get(WolodriveApp *app, const GameItem *item, ScreenshotSet *out);
void screenshots_destroy(struct ScreenshotService *service);
#endif
