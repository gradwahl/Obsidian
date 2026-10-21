/* Offline Linux visual comparison fixture; outputs four PNGs in build/. */
#include "../src/ui_netflix.c"
#include <assert.h>
int main(void) {
 assert(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER)==0); assert(TTF_Init()==0);
 WolodriveApp *app=calloc(1,sizeof(*app)); app->catalog_mutex=SDL_CreateMutex();
 app->window=SDL_CreateWindow("Glass capture",0,0,1920,1080,SDL_WINDOW_HIDDEN);
 app->renderer=SDL_CreateRenderer(app->window,-1,SDL_RENDERER_SOFTWARE);
 SDL_RenderSetLogicalSize(app->renderer,1920,1080);
 assert(ui_init(app)); SDL_AtomicSet(&app->language,LANG_EN);
 app->ui_theme=UI_THEME_UBIQUITY; app->active_row=VIEW_HOME;
 sys_import_native_snapshot(app); ui_rebuild_carousels(app);
 for(int size=0;size<2;size++) for(int expanded=0;expanded<2;expanded++) {
  int width=size?1920:1280,height=size?1080:720;
  SDL_SetWindowSize(app->window,width,height);
  app->sidebar_expand=expanded; app->sidebar_width=expanded?272:92;
  nav_highlight.initialized=false; category_highlight.initialized=false;
  SDL_SetRenderDrawColor(app->renderer,0,0,0,255);SDL_RenderClear(app->renderer);
  render_ubiquity_home(app);render_sidebar(app);render_language_control(app);
  SDL_Surface *image=SDL_CreateRGBSurfaceWithFormat(0,width,height,32,SDL_PIXELFORMAT_RGBA32);
  SDL_Rect capture={0,0,width,height};
  if(SDL_RenderReadPixels(app->renderer,&capture,image->format->format,image->pixels,image->pitch)!=0) {fprintf(stderr,"capture: %s\n",SDL_GetError());return 2;}
  char path[128];snprintf(path,sizeof(path),"build/glass-native-%dx%d-%s.png",width,height,expanded?"expanded":"collapsed");
  assert(IMG_SavePNG(image,path)==0); SDL_FreeSurface(image);
 }
 ui_cleanup(app); SDL_DestroyMutex(app->catalog_mutex);SDL_DestroyRenderer(app->renderer);SDL_DestroyWindow(app->window);free(app);TTF_Quit();SDL_Quit();return 0;
}
