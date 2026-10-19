#ifndef WOLODRIVE_ARTWORK_H
#define WOLODRIVE_ARTWORK_H

#include "wolodrive.h"

/* Requests and textures belong to the UI thread; decoding/networking to a worker. */
SDL_Texture *artwork_get(WolodriveApp *app, const GameItem *item, bool backdrop,
                         int priority, int *width, int *height);
SDL_Texture *artwork_get_media(WolodriveApp *app, const char *uri, int priority, int *width, int *height, bool *failed);
void artwork_prefetch(WolodriveApp *app, const GameItem *item, int priority);
void artwork_destroy(WolodriveApp *app);
bool artwork_complete_box(WolodriveApp *app, SDL_Texture *texture);
SDL_Color artwork_background(WolodriveApp *app, SDL_Texture *texture);
SDL_Rect artwork_fit(int width, int height, SDL_Rect box, bool crop);

#endif
