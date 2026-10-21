/* Real SDL dummy surface plus injected backend failure: no PS5 required. */
#include <SDL.h>
#include <assert.h>
#include <string.h>
static int fail_backend, calls;
static SDL_Renderer *create_checked(SDL_Window *window, int index, Uint32 flags) {
    SDL_RendererInfo info;
    assert(index >= 0); /* -1 would mask the backend error. */
    assert(SDL_GetRenderDriverInfo(index, &info) == 0);
    assert(strcmp(info.name, "software") == 0);
    assert(flags == SDL_RENDERER_SOFTWARE);
    assert(strcmp(SDL_GetHint(SDL_HINT_FRAMEBUFFER_ACCELERATION), "0") == 0);
    ++calls;
    if (fail_backend) { SDL_SetError("sceVideoOutOpen: injected failure"); return NULL; }
    return SDL_CreateRenderer(window, index, flags);
}
#define SDL_CreateRenderer create_checked
#include "ps5_renderer.h"
#undef SDL_CreateRenderer

int main(void) {
    assert(SDL_Init(SDL_INIT_VIDEO) == 0);
    SDL_Window *window = SDL_CreateWindow("renderer test", 0, 0, 320, 180, 0);
    assert(window);
    SDL_Renderer *renderer = poldrive_create_ps5_renderer(window);
    assert(renderer && calls == 1);
    assert(SDL_SetRenderDrawColor(renderer, 20, 40, 60, 255) == 0);
    assert(SDL_RenderClear(renderer) == 0);
    Uint32 pixel;
    SDL_Rect rect = {0, 0, 1, 1};
    assert(SDL_RenderReadPixels(renderer, &rect, SDL_PIXELFORMAT_ARGB8888, &pixel, 4) == 0);
    assert(pixel == 0xff14283c);
    SDL_RenderPresent(renderer);
    SDL_DestroyRenderer(renderer);
    fail_backend = 1;
    assert(!poldrive_create_ps5_renderer(window));
    assert(calls == 2);
    assert(strcmp(SDL_GetError(), "sceVideoOutOpen: injected failure") == 0);
    SDL_DestroyWindow(window);
    SDL_Quit();
    puts("PS5 renderer selection, drawing and backend error preservation passed");
    return 0;
}
