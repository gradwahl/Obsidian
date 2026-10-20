#include "../src/artwork.c"
#include "live_artwork.h"
#include <assert.h>

int main(void) {
    assert(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER)==0 && TTF_Init()==0);
    IMG_Init(IMG_INIT_PNG|IMG_INIT_JPG|IMG_INIT_WEBP);
    WolodriveApp *app=calloc(1,sizeof(*app));
    app->catalog_mutex=SDL_CreateMutex();
    SDL_Surface *screen=SDL_CreateRGBSurfaceWithFormat(0,1280,720,32,SDL_PIXELFORMAT_RGBA32);
    app->renderer=SDL_CreateSoftwareRenderer(screen);
    SDL_RenderSetLogicalSize(app->renderer,1920,1080);
    assert(ui_init(app));
    int count=sizeof(fixture_items)/sizeof(fixture_items[0]);
    app->catalog_count=count;
    for(int i=0;i<count;i++) {
        app->catalog[i]=fixture_items[i];
        app->catalog[i].is_game=true;
        app->catalog[i].catalog_added_at=20261019000000ULL+count-i;
        app->catalog[i].description_id=TXT_DESCRIPTION_FALLBACK;
    }
    ui_rebuild_carousels(app);
    for(int group=0;group<8;group++) {
        CarouselRow *row=&app->rows[VIEW_HOME];
        for(int step=0;step<16;step++) {
            row->selected_col=(group*16+step)%count;
            ui_update_animations(app,0.016f);
            ui_render(app);
        }
        Uint32 start=SDL_GetTicks();
        if(group==0) {
            SDL_LockMutex(app->artwork->mutex);
            for(int i=0;i<64;i++) {
                ArtworkEntry *e=&app->artwork->entries[i];
                if(e->key[0]&&!e->texture)printf("BEFORE %s p%d surf%d busy%d failed%d retry_in%d age%u\n",e->key,e->priority,e->surface!=NULL,e->busy,e->failed,(Sint32)(e->retry-start),start-e->used);
            }
            SDL_UnlockMutex(app->artwork->mutex);
        }
        int missing=7;
        while(SDL_GetTicks()-start<3000) {
            ui_update_animations(app,0.016f);
            ui_render(app);
            missing=0;
            for(int i=row->selected_col;i<SDL_min(count,row->selected_col+7);i++) {
                int w,h;
                if(!artwork_get(app,row->items[i],false,80,&w,&h)) missing++;
            }
            if(!missing)break;
            SDL_Delay(5);
        }
        printf("Stop at %d: %u ms, %d missing\n",row->selected_col,SDL_GetTicks()-start,missing);
        if(missing) {
            SDL_LockMutex(app->artwork->mutex);
            for(int i=0;i<64;i++){
                ArtworkEntry *e=&app->artwork->entries[i];
                if(e->key[0]&&!e->texture)printf("  %s p%d surf%d busy%d failed%d disk%d retry%u used%u\n",e->key,e->priority,e->surface!=NULL,e->busy,e->failed,e->disk_tried,e->retry,e->used);
            }
            SDL_UnlockMutex(app->artwork->mutex);
        }
        fflush(stdout);
    }
    ui_cleanup(app); SDL_DestroyRenderer(app->renderer); SDL_FreeSurface(screen);
    SDL_DestroyMutex(app->catalog_mutex);free(app);TTF_Quit();IMG_Quit();SDL_Quit();
}
