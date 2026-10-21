/* Included by the SDL renderer so file controls use its existing text/styles. */
#include "file_manager.h"
#include <errno.h>
#include <unistd.h>
static struct {
    FileEntry entries[FM_LIMIT];
    int count, selected, action, toolbar, opened, confirm, modal, move, has_clip;
    char path[FM_PATH], clip[FM_PATH], source[FM_PATH], destination[FM_PATH];
    char message[512];
    int operation, error;
    SDL_Thread *thread;
    SDL_atomic_t done;
} files = { .toolbar = 1 };
static const TextId file_labels[] = { TXT_FILES_OPEN, TXT_FILES_UP, TXT_FILES_REFRESH,
    TXT_FILES_COPY, TXT_FILES_MOVE, TXT_FILES_PASTE, TXT_FILES_DELETE, TXT_FILES_UNZIP };
static int files_relative(char *out, const char *parent, const char *name) {
    if (snprintf(out, FM_PATH, "%s%s%s", parent, *parent ? "/" : "", name) >= FM_PATH) {
        errno=ENAMETOOLONG; return -1;
    }
    return 0;
}
static void files_error(WolodriveApp *app, int error) {
    snprintf(files.message,sizeof(files.message),"%s: %s", TR(app,TXT_FILES_ERROR),strerror(error));
}
static void files_refresh(WolodriveApp *app) {
    files.count=0; files.selected=0; files.message[0]=0;
    int count=fm_list("/",files.path,files.entries,FM_LIMIT);
    if(count<0) files_error(app,errno);
    else { files.count=count; SDL_strlcpy(files.message,TR(app,TXT_FILES_DONE),sizeof(files.message)); }
}
static int files_worker(void *unused) {
    (void)unused;
    int result = files.operation==6 ? fm_delete("/",files.source) :
        files.operation==7 ? fm_unzip("/",files.source,files.destination) :
        fm_copy("/",files.source,files.destination,files.move);
    files.error=result ? errno : 0; SDL_AtomicSet(&files.done,1); return 0;
}
static void files_poll(WolodriveApp *app) {
    if(!files.thread || !SDL_AtomicGet(&files.done)) return;
    SDL_WaitThread(files.thread,NULL); files.thread=NULL;
    int error=files.error;
    if(!error && (files.operation==6 || (files.operation==5 && files.move))) files.has_clip=0;
    files_refresh(app);
    if(error) files_error(app,error);
    else if(!files.message[0]) SDL_strlcpy(files.message,TR(app,TXT_FILES_DONE),sizeof(files.message));
}
static void files_shutdown(void) {
    if(files.thread) { SDL_WaitThread(files.thread,NULL); files.thread=NULL; }
}
static int files_enabled(int action) {
    if(files.thread) return 0;
    if(action==0) return 1;
    if(action==1) return files.opened && *files.path;
    if(action==2) return files.opened;
    if(action==5) return files.opened && files.has_clip;
    if(!files.count) return 0;
    if(action==7) {
        const FileEntry *entry=&files.entries[files.selected]; size_t n=strlen(entry->name);
        return !entry->directory && n>=4 && !strcasecmp(entry->name+n-4,".zip");
    }
    return 1;
}
static void files_action(WolodriveApp *app, int action) {
    if(!files_enabled(action)) return;
    files.action=action; files.message[0]=0;
    FileEntry *entry=files.count ? &files.entries[files.selected] : NULL;
    if(action==0) {
        files.opened=1;
#ifdef __PS5__
        SDL_strlcpy(files.path,"data",sizeof(files.path));
#else
        char current[FM_PATH];
        if(!getcwd(current,sizeof(current))) { files_error(app,errno); return; }
        SDL_strlcpy(files.path,current[0]=='/' ? current+1 : "",sizeof(files.path));
#endif
        files_refresh(app); return;
    }
    if(action==1) {
        char *slash=strrchr(files.path,'/'); if(slash) *slash=0; else files.path[0]=0;
        files_refresh(app); return;
    }
    if(action==2) { files_refresh(app); return; }
    if(action==3 || action==4) {
        if(files_relative(files.clip,files.path,entry->name)) { files_error(app,errno); return; }
        files.move=action==4; files.has_clip=1;
        SDL_strlcpy(files.message,TR(app,TXT_FILES_DONE),sizeof(files.message)); return;
    }
    if(action==6 && !files.modal) { files.modal=1; files.confirm=0; return; }
    files.modal=0;
    if(action==5) {
        const char *name=strrchr(files.clip,'/'); name=name ? name+1 : files.clip;
        SDL_strlcpy(files.source,files.clip,sizeof(files.source));
        if(files_relative(files.destination,files.path,name)) { files_error(app,errno); return; }
    } else {
        if(files_relative(files.source,files.path,entry->name)) { files_error(app,errno); return; }
        if(action==7 && snprintf(files.destination,sizeof(files.destination),"%s.extracted",files.source)>=(int)sizeof(files.destination)) {
            files_error(app,ENAMETOOLONG); return;
        }
    }
    files.operation=action; SDL_AtomicSet(&files.done,0);
    files.thread=SDL_CreateThread(files_worker,"file-operation",NULL);
    if(!files.thread) files_error(app,EAGAIN);
}
static void files_open_entry(WolodriveApp *app) {
    if(!files.count || !files.entries[files.selected].directory || files.thread) return;
    char path[FM_PATH];
    if(files_relative(path,files.path,files.entries[files.selected].name)) { files_error(app,errno); return; }
    SDL_strlcpy(files.path,path,sizeof(files.path)); files.message[0]=0; files_refresh(app);
}
static SDL_Rect files_button(int index) { return (SDL_Rect){130+index*210,252,198,52}; }
static SDL_Rect files_row(int index) { return (SDL_Rect){130,332+(index%8)*68,1668,64}; }
static bool handle_files_input(WolodriveApp *app,const SDL_Event *event) {
    if(app->active_row!=VIEW_FILES) return false;
    if(files.thread) return true;
    int key=0;
    if(event->type==SDL_KEYDOWN) key=event->key.keysym.sym;
    if(event->type==SDL_CONTROLLERBUTTONDOWN) {
        switch(event->cbutton.button) {
            case SDL_CONTROLLER_BUTTON_A: key=SDLK_RETURN; break;
            case SDL_CONTROLLER_BUTTON_B: key=SDLK_ESCAPE; break;
            case SDL_CONTROLLER_BUTTON_Y: key=SDLK_r; break;
            case SDL_CONTROLLER_BUTTON_DPAD_UP: key=SDLK_UP; break;
            case SDL_CONTROLLER_BUTTON_DPAD_DOWN: key=SDLK_DOWN; break;
            case SDL_CONTROLLER_BUTTON_DPAD_LEFT: key=SDLK_LEFT; break;
            case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: key=SDLK_RIGHT; break;
        }
    }
    if(files.modal) {
        if(key==SDLK_ESCAPE || key==SDLK_BACKSPACE) files.modal=0;
        else if(key==SDLK_LEFT || key==SDLK_RIGHT || key==SDLK_UP || key==SDLK_DOWN) files.confirm=!files.confirm;
        else if(key==SDLK_RETURN || key==SDLK_SPACE) { if(files.confirm) files_action(app,6); else files.modal=0; }
        if(event->type==SDL_MOUSEBUTTONDOWN && event->button.button==SDL_BUTTON_LEFT) {
            if(contains_point((SDL_Rect){540,540,400,52},event->button.x,event->button.y)) files.modal=0;
            if(contains_point((SDL_Rect){980,540,400,52},event->button.x,event->button.y)) files_action(app,6);
        }
        return true;
    }
    if(event->type==SDL_MOUSEBUTTONDOWN && event->button.button==SDL_BUTTON_LEFT) {
        for(int i=0;i<8;i++) if(contains_point(files_button(i),event->button.x,event->button.y)) {
            files.toolbar=1; files_action(app,i); return true;
        }
        int first=files.selected/8*8;
        for(int i=first;i<files.count && i<first+8;i++) if(contains_point(files_row(i),event->button.x,event->button.y)) {
            files.selected=i; files.toolbar=0;
            if(event->button.clicks>=2) files_open_entry(app);
            return true;
        }
        if(contains_point((SDL_Rect){266,890,60,36},event->button.x,event->button.y) && first>0) files.selected=first-8;
        if(contains_point((SDL_Rect){342,890,60,36},event->button.x,event->button.y) && first+8<files.count) files.selected=first+8;
    }
    if(key==SDLK_ESCAPE || key==SDLK_BACKSPACE) {
        if(*files.path && files.opened) files_action(app,1); else app->sidebar_focus=true;
    } else if(key==SDLK_r) files_action(app,2);
    else if(key==SDLK_RETURN || key==SDLK_SPACE) {
        if(files.toolbar) files_action(app,files.action); else files_open_entry(app);
    } else if(files.toolbar) {
        if(key==SDLK_LEFT) { if(!files.action) app->sidebar_focus=true; else files.action--; }
        if(key==SDLK_RIGHT && files.action<7) files.action++;
        if(key==SDLK_DOWN && files.count) files.toolbar=0;
    } else {
        if(key==SDLK_LEFT) app->sidebar_focus=true;
        if(key==SDLK_RIGHT || (key==SDLK_UP && !files.selected)) files.toolbar=1;
        if(key==SDLK_UP && files.selected>0) files.selected--;
        if(key==SDLK_DOWN && files.selected+1<files.count) files.selected++;
    }
    return true;
}
static void render_files(WolodriveApp *app) {
    SDL_Renderer *r=app->renderer;
    SDL_Color white={245,245,245,255}, muted={165,165,165,255};
    draw_text(app,app->font_hero_title,TR(app,TXT_NAV_FILES),130,100,white,1668);
    draw_text(app,app->font_small,TR(app,TXT_FILES_DESCRIPTION),130,172,muted,1668);
    char path[FM_PATH+2]; snprintf(path,sizeof(path),"/%s",files.path);
    draw_text(app,app->font_small,files.opened ? path : TR(app,TXT_FILES_CHOOSE),130,206,white,1668);
    for(int i=0;i<8;i++) {
        int shade=files_enabled(i)?24:8;
        SDL_Rect button=files_button(i); fill_rect(r,button.x,button.y,button.w,button.h,shade,shade,shade,255);
        SDL_Rect text={button.x+8,button.y+10,button.w-16,32};
        draw_fitted_text(app,app->font_small,TR(app,file_labels[i]),text,files_enabled(i)?white:(SDL_Color){86,86,86,255},text,true);
        if(!app->sidebar_focus && files.toolbar && files.action==i) draw_border(r,button.x,button.y,button.w,button.h,3,245,245,245,255);
    }
    int first=files.selected/8*8;
    for(int i=first;i<files.count && i<first+8;i++) {
        SDL_Rect row=files_row(i); int shade=i==files.selected?48:24;
        fill_rect(r,row.x,row.y,row.w,row.h,shade,shade,shade,255);
        draw_text(app,app->font_micro,files.entries[i].directory?"[DIR]":"[FILE]",148,row.y+20,muted,92);
        draw_text(app,app->font_body,files.entries[i].name,264,row.y+17,white,1460);
        if(files.entries[i].directory) draw_text(app,app->font_body,"→",1750,row.y+17,white,40);
        if(!app->sidebar_focus && !files.toolbar && i==files.selected) draw_border(r,row.x,row.y,row.w,row.h,3,245,245,245,255);
    }
    if(!files.count) draw_text(app,app->font_body,TR(app,files.opened?TXT_FILES_EMPTY:TXT_FILES_CHOOSE),130,356,muted,1668);
    char position[64]; snprintf(position,sizeof(position),"%d / %d",files.count?files.selected+1:0,files.count);
    draw_text(app,app->font_micro,position,130,898,white,120);
    fill_rect(r,266,890,60,36,24,24,24,255); fill_rect(r,342,890,60,36,24,24,24,255);
    draw_text(app,app->font_micro,"←",284,898,first>0?white:muted,30);
    draw_text(app,app->font_micro,"→",360,898,first+8<files.count?white:muted,30);
    if(files.has_clip) {
        char clip[FM_PATH+128]; snprintf(clip,sizeof(clip),"%s: %s",TR(app,files.move?TXT_FILES_MOVE:TXT_FILES_COPY),strrchr(files.clip,'/')?strrchr(files.clip,'/')+1:files.clip);
        draw_text(app,app->font_micro,clip,418,898,white,1350);
    }
    draw_text(app,app->font_micro,files.thread?TR(app,TXT_FILES_BUSY):files.message,130,928,white,1668);
    draw_text(app,app->font_micro,TR(app,TXT_FILES_HINT),130,968,muted,1668);
}
static void render_files_dialog(WolodriveApp *app) {
    SDL_Renderer *r=app->renderer;
    SDL_Color white={245,245,245,255};
    if(files.modal) {
        fill_rect(r,0,0,1920,1080,0,0,0,187); fill_rect(r,500,380,920,240,24,24,24,255);
        SDL_Rect title={540,412,840,36};
        draw_fitted_text(app,app->font_body,TR(app,TXT_FILES_CONFIRM),title,white,title,false);
        draw_text(app,app->font_body,files.entries[files.selected].name,540,456,white,840);
        for(int i=0;i<2;i++) {
            SDL_Rect button={540+i*440,540,400,52},text={550+i*440,550,380,32};
            fill_rect(r,button.x,button.y,button.w,button.h,48,48,48,255);
            draw_fitted_text(app,app->font_body,TR(app,i?TXT_FILES_DELETE:TXT_FILES_CANCEL),text,white,text,true);
            if(files.confirm==i) draw_border(r,button.x,button.y,button.w,button.h,3,245,245,245,255);
        }
    }
}
