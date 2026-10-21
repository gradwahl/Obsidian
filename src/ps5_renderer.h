#ifndef OBSIDIAN_PS5_RENDERER_H
#define OBSIDIAN_PS5_RENDERER_H

#include <SDL.h>
#include <stdio.h>
#include <string.h>

/* SDL's automatic (-1) selection overwrites backend failures with
 * "Couldn't find matching render driver". Use an explicit software index so
 * PS5 VideoOut/direct-memory errors survive into the startup log. */
static SDL_Renderer *obsidian_create_ps5_renderer(SDL_Window *window) {
    SDL_version version;
    SDL_GetVersion(&version);
    const char *video = SDL_GetCurrentVideoDriver();
    fprintf(stderr, "[OBSIDIAN DISPLAY] SDL %u.%u.%u; video: %s\n",
            version.major, version.minor, version.patch, video ? video : "none");
    /* The PS5 port presents a CPU surface through VideoOut. Do not attempt
     * a nested GPU renderer to emulate that surface. */
    SDL_SetHintWithPriority(SDL_HINT_FRAMEBUFFER_ACCELERATION, "0", SDL_HINT_OVERRIDE);
    int count = SDL_GetNumRenderDrivers();
    for (int i = 0; i < count; ++i) {
        SDL_RendererInfo info;
        if (SDL_GetRenderDriverInfo(i, &info) != 0) continue;
        fprintf(stderr, "[OBSIDIAN DISPLAY] Driver %d: %s; flags: 0x%x\n",
                i, info.name, (unsigned)info.flags);
        if (strcmp(info.name, "software") != 0 || !(info.flags & SDL_RENDERER_SOFTWARE)) continue;
        SDL_Renderer *renderer = SDL_CreateRenderer(window, i, SDL_RENDERER_SOFTWARE);
        if (!renderer)
            fprintf(stderr, "[OBSIDIAN ERROR] PS5 software renderer backend: %s\n", SDL_GetError());
        return renderer;
    }
    SDL_SetError("PS5 SDL build has no software render driver (reported %d drivers)", count);
    return NULL;
}

#endif
