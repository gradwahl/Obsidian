
#include "../src/ui_netflix.c"
#include <assert.h>
int main(void) {
 assert(SDL_Init(SDL_INIT_VIDEO)==0);
 SDL_Window *w=SDL_CreateWindow("probe",0,0,1280,720,0);
 SDL_Renderer *r=SDL_CreateRenderer(w,-1,SDL_RENDERER_SOFTWARE);
 SDL_RenderSetLogicalSize(r,1920,1080);
 SDL_SetRenderDrawColor(r,0,0,0,255); SDL_RenderClear(r);
 fill_vertical_gradient(r,0,0,1920,1080,255,255,255,255,255,255,255,255,64);
 SDL_Surface *s=SDL_CreateRGBSurfaceWithFormat(0,1280,720,32,SDL_PIXELFORMAT_RGBA32);
 assert(SDL_RenderReadPixels(r,NULL,s->format->format,s->pixels,s->pitch)==0);
 int gaps=0;
 for(int y=0;y<720;y++){
  Uint8 red,green,blue,alpha;
  SDL_GetRGBA(*(Uint32*)((Uint8*)s->pixels+y*s->pitch+640*4),s->format,&red,&green,&blue,&alpha);
  if(red!=255)gaps++;
 }
 printf("Unpainted rows at 1280x720: %d\n",gaps);
 IMG_SavePNG(s,"build/gradient-before.png");
 return gaps==0 ? 0 : 1;
}
