/* A no-font/no-assets startup frame must render even before app state exists. */
#include <SDL.h>
#include <assert.h>
#include <stdio.h>
#include "startup_splash.h"

int main(void) {
    assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
    SDL_Window *window = SDL_CreateWindow("startup splash", 0, 0, 640, 360, SDL_WINDOW_HIDDEN);
    assert(window);
    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    assert(renderer);
    assert(SDL_RenderSetLogicalSize(renderer, 1920, 1080) == 0);
    for (unsigned i = 0; i < 8; i++)
        startup_splash_draw(renderer, i);
    /* The spinner, label, and progress bar must draw on a blank renderer
       without invoking SDL_image, SDL_ttf, network, or UI code. */
    SDL_Surface *pixels = SDL_CreateRGBSurfaceWithFormat(0, 640, 360, 32, SDL_PIXELFORMAT_RGBA32);
    assert(pixels);
    assert(SDL_RenderReadPixels(renderer, NULL, pixels->format->format,
                                pixels->pixels, pixels->pitch) == 0);
    SDL_FreeSurface(pixels);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    puts("Startup splash: renderer-only boot screens passed");
    return 0;
}
