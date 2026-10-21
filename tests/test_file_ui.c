/* Test actual SDL controller, keyboard, pointer and modal paths offline. */
#include <assert.h>
#include "../src/ui_netflix.c"
static void key(WolodriveApp *app, SDL_Keycode code) {
    SDL_Event event={0}; event.type=SDL_KEYDOWN; event.key.keysym.sym=code;
    ui_handle_input(app,&event);
}
static void snapshot(WolodriveApp *app,const char *name) {
    SDL_Surface *image=SDL_CreateRGBSurfaceWithFormat(0,1920,1080,32,SDL_PIXELFORMAT_RGBA32);
    assert(image); SDL_SetRenderDrawColor(app->renderer,0,0,0,255); SDL_RenderClear(app->renderer);
    render_files(app); render_files_dialog(app);
    assert(SDL_RenderReadPixels(app->renderer,NULL,SDL_PIXELFORMAT_RGBA32,image->pixels,image->pitch)==0);
    char path[256]; snprintf(path,sizeof(path),"build/%s.png",name); assert(IMG_SavePNG(image,path)==0); SDL_FreeSurface(image);
}
int main(void) {
    assert(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER)==0); assert(TTF_Init()==0);
    WolodriveApp *app=calloc(1,sizeof(*app)); assert(app);
    app->window=SDL_CreateWindow("Files test",0,0,1920,1080,SDL_WINDOW_HIDDEN); assert(app->window);
    app->renderer=SDL_CreateRenderer(app->window,-1,SDL_RENDERER_SOFTWARE); assert(app->renderer);
    SDL_SetRenderDrawBlendMode(app->renderer,SDL_BLENDMODE_BLEND);
    assert(ui_init(app)); app->active_row=VIEW_FILES;
    snapshot(app,"files-native-empty");
    files.count=4; files.opened=1; files.toolbar=0;
    strcpy(files.path,"wolodrive-files-test-only");
    strcpy(files.entries[0].name,"Destination"); files.entries[0].directory=1;
    strcpy(files.entries[1].name,"Folder"); files.entries[1].directory=1;
    strcpy(files.entries[2].name,"archive.zip"); strcpy(files.entries[3].name,"hello.txt");
    snapshot(app,"files-native-list");
    key(app,SDLK_DOWN); assert(files.selected==1);
    SDL_Event pad={0}; pad.type=SDL_CONTROLLERBUTTONDOWN; pad.cbutton.button=SDL_CONTROLLER_BUTTON_DPAD_DOWN;
    ui_handle_input(app,&pad); assert(files.selected==2);
    key(app,SDLK_RIGHT); assert(files.toolbar);
    files.action=3; key(app,SDLK_RETURN); assert(files.has_clip && !files.move);
    assert(strcmp(files.clip,"wolodrive-files-test-only/archive.zip")==0);
    files.action=4; key(app,SDLK_RETURN); assert(files.move);
    files.action=6; key(app,SDLK_RETURN); assert(files.modal && !files.confirm && !files.thread);
    snapshot(app,"files-native-delete");
    /* The modal blocks the rail and catalog refresh. Cancel is the default. */
    SDL_Event click={0}; click.type=SDL_MOUSEBUTTONDOWN; click.button.button=SDL_BUTTON_LEFT;
    click.button.x=30; click.button.y=250; ui_handle_input(app,&click);
    assert(app->active_row==VIEW_FILES && files.modal);
    key(app,SDLK_r); assert(files.modal && !files.thread);
    key(app,SDLK_RETURN); assert(!files.modal && !files.thread);
    key(app,SDLK_RETURN); assert(files.modal);
    key(app,SDLK_ESCAPE); assert(!files.modal && !files.thread);
    files.toolbar=1; files.action=0; key(app,SDLK_LEFT); assert(app->sidebar_focus);
    key(app,SDLK_RIGHT); assert(!app->sidebar_focus);
    ui_cleanup(app); SDL_DestroyRenderer(app->renderer); SDL_DestroyWindow(app->window);
    free(app); TTF_Quit(); SDL_Quit();
    puts("PASS: Files SDL keyboard/controller, clipboard, modal isolation, cancel and rail navigation"); return 0;
}
