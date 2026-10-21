#ifndef UI_GLASS_H
#define UI_GLASS_H

/* Shared geometry and real-time motion for menu, categories and languages. */
typedef struct {
    SDL_FRect current, from, target;
    Uint32 started;
    float duration;
    bool initialized;
} HighlightAnim;

static float glass_sidebar_ease(float t) {
    float lo=0,hi=1,u=t;
    for(int i=0;i<14;i++) {
        u=(lo+hi)*.5f;
        float x=3*(1-u)*(1-u)*u*.4f+3*(1-u)*u*u*.2f+u*u*u;
        if(x<t) lo=u; else hi=u;
    }
    return 3*(1-u)*u*u+u*u*u;
}
static float glass_ease(float t) { return 1.0f - powf(1.0f - t, 3.0f); }
static void updateHighlight(HighlightAnim *a, SDL_FRect target, bool snap) {
    Uint32 now = SDL_GetTicks();
    if (a->initialized) {
        float t = SDL_min(1.0f, (now - a->started) / a->duration);
        float lead = glass_ease(t);
        float trail = 1.0f - powf(1.0f - SDL_max(0.0f, (now-a->started-40.0f)/(a->duration-40.0f)), 4.0f);
        float fx1=a->from.x, fx2=fx1+a->from.w, fy1=a->from.y, fy2=fy1+a->from.h;
        float tx1=a->target.x, tx2=tx1+a->target.w, ty1=a->target.y, ty2=ty1+a->target.h;
        float x1=fx1+(tx1-fx1)*(tx1<fx1?lead:trail), x2=fx2+(tx2-fx2)*(tx2>fx2?lead:trail);
        float y1=fy1+(ty1-fy1)*(ty1<fy1?lead:trail), y2=fy2+(ty2-fy2)*(ty2>fy2?lead:trail);
        a->current=(SDL_FRect){x1,y1,x2-x1,y2-y1};
    }
    if (!a->initialized || snap) {
        a->initialized=true; a->current=a->from=a->target=target;
        a->started=now; a->duration=280;
    } else if (memcmp(&target,&a->target,sizeof(target))) {
        a->from=a->current; a->target=target; a->started=now;
        a->duration=SDL_min(520.0f,280.0f+hypotf(target.x-a->current.x,target.y-a->current.y)*.25f);
    }
}

/* Cached at 2x, including halo and gloss. Nine-slice copies preserve the
   circular caps during liquid stretching, without generating per-frame textures. */
enum { GLASS_NEUTRAL, GLASS_ACTIVE, GLASS_PANEL, GLASS_PANEL_UBQ, GLASS_PRIMARY };
static struct { SDL_Renderer *renderer; SDL_Texture *texture; int h, state; } glass_cache[12];

static void glass_shape(SDL_Renderer *r, float x,float y,float w,float h,float radius,
                        SDL_Color top, SDL_Color bottom) {
    SDL_Vertex vertices[69]; int indices[204], n=1;
    vertices[0]=(SDL_Vertex){{x+w/2,y+h/2},
        {(top.r+bottom.r)/2,(top.g+bottom.g)/2,(top.b+bottom.b)/2,(top.a+bottom.a)/2},{0,0}};
    for(int corner=0;corner<4;corner++) {
        float cx=x+(corner<2?w-radius:radius);
        float cy=y+(corner==0||corner==3?radius:h-radius);
        for(int j=0;j<=16;j++) {
            float angle=(-90.0f+corner*90.0f+j*90.0f/16.0f)*3.14159265f/180.0f;
            float px=cx+cosf(angle)*radius, py=cy+sinf(angle)*radius;
            float t=(py-y)/h;
            SDL_Color c={(Uint8)(top.r+(bottom.r-top.r)*t),(Uint8)(top.g+(bottom.g-top.g)*t),
                         (Uint8)(top.b+(bottom.b-top.b)*t),(Uint8)(top.a+(bottom.a-top.a)*t)};
            vertices[n++]=(SDL_Vertex){{px,py},c,{0,0}};
        }
    }
    for(int i=0;i<n-1;i++) { indices[3*i]=0; indices[3*i+1]=i+1; indices[3*i+2]=i+2<n?i+2:1; }
    SDL_RenderGeometry(r,NULL,vertices,n,indices,(n-1)*3);
}

static void drawGlassPill(SDL_Renderer *r, SDL_FRect rect, int state) {
    const int pad=32, size=128; /* Fixed source; edges never stretch. */
    int h=SDL_min(128,(int)ceilf(rect.h));
    if(rect.w<=0||h<=0) return;
    bool panel=state==GLASS_PANEL||state==GLASS_PANEL_UBQ;
    int slot=(h==30?0:h==40?1:h==44?2:3)+state*2;
    slot%=12;
    if(glass_cache[slot].renderer!=r||glass_cache[slot].h!=h||glass_cache[slot].state!=state) {
        if(glass_cache[slot].texture) SDL_DestroyTexture(glass_cache[slot].texture);
        memset(&glass_cache[slot],0,sizeof(glass_cache[slot]));
    }
    SDL_Texture *tex=glass_cache[slot].texture;
    if(!tex) {
        SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY,"linear");
        tex=SDL_CreateTexture(r,SDL_PIXELFORMAT_RGBA8888,SDL_TEXTUREACCESS_TARGET,(size+pad*2)*2,(h+pad*2)*2);
        if(!tex) return;
        SDL_Texture *previous=SDL_GetRenderTarget(r);
        SDL_Rect viewport,clip; SDL_RenderGetViewport(r,&viewport); SDL_RenderGetClipRect(r,&clip);
        bool clipped=SDL_RenderIsClipEnabled(r); float sx,sy; SDL_RenderGetScale(r,&sx,&sy);
        SDL_SetRenderTarget(r,tex); SDL_RenderSetViewport(r,NULL); SDL_RenderSetClipRect(r,NULL); SDL_RenderSetScale(r,2,2);
        SDL_SetRenderDrawColor(r,0,0,0,0); SDL_RenderClear(r); SDL_SetRenderDrawBlendMode(r,SDL_BLENDMODE_BLEND);
        float radius=panel?34:h/2.0f;
        for(int spread=24;spread>=1;spread--) {
            SDL_Color shadow={0,0,0,(Uint8)(spread<5?20:3)};
            glass_shape(r,pad-spread,pad+6-spread,size+2*spread,h+2*spread,radius+spread,shadow,shadow);
        }
        if(state==GLASS_ACTIVE||state==GLASS_PRIMARY) {
            for(int spread=22;spread>=1;spread--) {
                SDL_Color glow={229,9,20,4};
                glass_shape(r,pad-spread,pad+8-spread,size+2*spread,h+2*spread,radius+spread,glow,glow);
            }
        }
        glass_shape(r,pad,pad,size,h,radius,(SDL_Color){255,255,255,115},(SDL_Color){255,255,255,46});
        SDL_Color top={28,28,38,184},bottom={22,22,30,199};
        if(state==GLASS_ACTIVE) top=bottom=(SDL_Color){229,9,20,89};
        if(state==GLASS_PRIMARY) {top=(SDL_Color){255,30,39,255};bottom=(SDL_Color){200,8,18,255};}
        if(panel) top=bottom=state==GLASS_PANEL_UBQ?(SDL_Color){7,8,11,245}:(SDL_Color){12,12,12,250};
        SDL_SetRenderDrawBlendMode(r,SDL_BLENDMODE_NONE);
        glass_shape(r,pad+1,pad+1,size-2,h-2,radius-1,top,bottom);
        SDL_SetRenderDrawBlendMode(r,SDL_BLENDMODE_BLEND);
        glass_shape(r,pad+size*.06f,pad+1,size*.88f,h*.45f,SDL_min(radius,h*.225f),
                    (SDL_Color){255,255,255,41},(SDL_Color){255,255,255,0});
        SDL_SetRenderTarget(r,previous); SDL_RenderSetScale(r,sx,sy); SDL_RenderSetViewport(r,&viewport); SDL_RenderSetClipRect(r,clipped?&clip:NULL);
        SDL_SetTextureBlendMode(tex,SDL_BLENDMODE_BLEND);
        glass_cache[slot].renderer=r;glass_cache[slot].texture=tex;glass_cache[slot].h=h;glass_cache[slot].state=state;
    }
    float cap=panel?34:h/2.0f-.5f;
    float xs[]={0,pad+cap,pad+size-cap,(float)(size+2*pad)};
    float ys[]={0,pad+cap,pad+h-cap,(float)(h+2*pad)};
    float dx[]={rect.x-pad,rect.x+cap,rect.x+rect.w-cap,rect.x+rect.w+pad};
    float dy[]={rect.y-pad,rect.y+cap,rect.y+rect.h-cap,rect.y+rect.h+pad};
    for(int y=0;y<3;y++) for(int x=0;x<3;x++) {
        SDL_Rect src={(int)(xs[x]*2),(int)(ys[y]*2),(int)((xs[x+1]-xs[x])*2),(int)((ys[y+1]-ys[y])*2)};
        SDL_FRect dst={dx[x],dy[y],dx[x+1]-dx[x],dy[y+1]-dy[y]};
        if(src.w>0&&src.h>0&&dst.w>0&&dst.h>0) SDL_RenderCopyF(r,tex,&src,&dst);
    }
}
static void drawHighlight(SDL_Renderer *r,HighlightAnim *a,SDL_FRect rect,bool snap) {
    updateHighlight(a,rect,snap); drawGlassPill(r,a->current,GLASS_ACTIVE);
}
static void glass_cleanup(SDL_Renderer *r) {
    for(int i=0;i<12;i++) if(glass_cache[i].renderer==r) {
        SDL_DestroyTexture(glass_cache[i].texture); memset(&glass_cache[i],0,sizeof(glass_cache[i]));
    }
}
#endif
