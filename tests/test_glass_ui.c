/* Offline real-time interruption, cap rendering, and renderer-state checks. */
#include <SDL.h>
#include <assert.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>
#include "ui_glass.h"

int main(void) {
    assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER)==0);
    assert(glass_sidebar_ease(0)<.001f && glass_sidebar_ease(1)>.999f);
    HighlightAnim a={0};
    SDL_FRect first={8,164,256,44}, next={8,356,256,44};
    updateHighlight(&a,first,false);
    assert(a.current.y==first.y);
    updateHighlight(&a,next,false);
    assert(a.duration==328);
    a.started-=100;
    updateHighlight(&a,next,false);
    SDL_FRect in_flight=a.current;
    updateHighlight(&a,(SDL_FRect){8,900,256,44},false);
    assert(fabsf(a.from.y-in_flight.y)<2);
    updateHighlight(&a,first,true);
    assert(a.current.y==first.y);
    SDL_Surface *surface=SDL_CreateRGBSurfaceWithFormat(0,640,480,32,SDL_PIXELFORMAT_RGBA32);
    SDL_Renderer *r=SDL_CreateSoftwareRenderer(surface);
    assert(r);
    SDL_RenderSetLogicalSize(r,640,480);
    SDL_Rect clip={0,0,640,480}; SDL_RenderSetClipRect(r,&clip);
    drawHighlight(r,&a,first,true);
    drawGlassPill(r,(SDL_FRect){40,260,210,40},GLASS_NEUTRAL);
    drawGlassPill(r,(SDL_FRect){280,32,272,400},GLASS_PANEL);
    SDL_Rect actual; SDL_RenderGetClipRect(r,&actual);
    assert(!memcmp(&clip,&actual,sizeof(clip)));
    assert(glass_cache[GLASS_PANEL*2+3].texture);
    SDL_RenderPresent(r);
    unsigned int nonzero=0;
    for(int y=0;y<surface->h;y++) for(int x=0;x<surface->w;x++) {
        Uint8 red,green,blue,alpha;
        SDL_GetRGBA(((Uint32*)((Uint8*)surface->pixels+y*surface->pitch))[x],surface->format,&red,&green,&blue,&alpha);
        if(alpha) nonzero++;
    }
    assert(nonzero>50000);
    glass_cleanup(r); SDL_DestroyRenderer(r); SDL_FreeSurface(surface); SDL_Quit();
    SDL_Log("PASS: glass geometry, cached textures, interruption and resize snap");
    return 0;
}
