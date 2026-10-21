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
static struct { SDL_Renderer *renderer; SDL_Texture *texture; SDL_Texture *liquid[3]; int h, state; } glass_cache[20];

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
    int slot=state*4+(h==30?0:h==40?1:h==44?2:3);
    if(glass_cache[slot].renderer!=r||glass_cache[slot].h!=h||glass_cache[slot].state!=state) {
        if(glass_cache[slot].texture) SDL_DestroyTexture(glass_cache[slot].texture);
        for(int j=0;j<3;j++) if(glass_cache[slot].liquid[j]) SDL_DestroyTexture(glass_cache[slot].liquid[j]);
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
            SDL_Color shadow={0,0,0,(Uint8)(spread<5?16:3)};
            glass_shape(r,pad-spread,pad+6-spread,size+2*spread,h+2*spread,radius+spread,shadow,shadow);
        }
        if(state==GLASS_ACTIVE||state==GLASS_PRIMARY) {
            for(int spread=22;spread>=1;spread--) {
                SDL_Color glow={229,9,20,4};
                glass_shape(r,pad-spread,pad+8-spread,size+2*spread,h+2*spread,radius+spread,glow,glow);
            }
        }
        glass_shape(r,pad,pad,size,h,radius,
                    (SDL_Color){255,255,255,(Uint8)(panel?(state==GLASS_PANEL_UBQ?28:115):53)},
                    (SDL_Color){255,255,255,(Uint8)(panel?(state==GLASS_PANEL_UBQ?14:46):17)});
        SDL_Color top={92,98,116,72},bottom={16,20,32,84};
        if(state==GLASS_ACTIVE) {
            top=(SDL_Color){253,115,150,84};
            bottom=(SDL_Color){59,15,40,98};
        }
        if(state==GLASS_PRIMARY) {top=(SDL_Color){255,30,39,255};bottom=(SDL_Color){200,8,18,255};}
        if(panel) top=bottom=state==GLASS_PANEL_UBQ?(SDL_Color){12,16,28,150}:(SDL_Color){12,12,12,250};
        SDL_SetRenderDrawBlendMode(r,SDL_BLENDMODE_NONE);
        glass_shape(r,pad+1,pad+1,size-2,h-2,radius-1,top,bottom);
        SDL_SetRenderDrawBlendMode(r,SDL_BLENDMODE_BLEND);
        if(state==GLASS_PANEL)
            glass_shape(r,pad+size*.06f,pad+1,size*.88f,h*.45f,SDL_min(radius,h*.225f),
                        (SDL_Color){255,255,255,41},(SDL_Color){255,255,255,0});
        else if(state!=GLASS_PANEL_UBQ)
            glass_shape(r,pad+1,pad+1,size-2,h-2,radius-1,
                        (SDL_Color){255,255,255,18},(SDL_Color){255,255,255,0});
        SDL_SetRenderTarget(r,previous); SDL_RenderSetScale(r,sx,sy); SDL_RenderSetViewport(r,&viewport); SDL_RenderSetClipRect(r,clipped?&clip:NULL);
        SDL_SetTextureBlendMode(tex,SDL_BLENDMODE_BLEND);
        glass_cache[slot].renderer=r;glass_cache[slot].texture=tex;glass_cache[slot].h=h;glass_cache[slot].state=state;

        /* Bake three heavily softened gel fields once per cached height/state.
           Their end caps are masked to the SAME parent silhouette, never
           drawn as little ovals. Underlying artwork stays visible. */
        if(!panel) for(int field=0;field<3;field++) {
            const int ww=(size+pad*2)*2, hh=(h+pad*2)*2;
            SDL_Surface *surface=SDL_CreateRGBSurfaceWithFormat(0,ww,hh,32,SDL_PIXELFORMAT_RGBA8888);
            if(!surface) continue;
            SDL_LockSurface(surface);
            for(int py=0;py<hh;py++) {
                Uint32 *row=(Uint32 *)((Uint8 *)surface->pixels+py*surface->pitch);
                for(int px=0;px<ww;px++) {
                    float x=px*.5f-pad,y=py*.5f-pad;
                    float mid=SDL_max(radius,SDL_min(size-radius,x));
                    float dist=hypotf(x-mid,y-h*.5f);
                    float edge=SDL_max(0.0f,SDL_min(1.0f,(radius-dist)*2.0f));
                    float u=x/size,v=y/h;
                    float cx=field==0?.29f:field==1?.73f:.51f;
                    float cy=field==0?.31f:field==1?.77f:.55f;
                    float rx=field==0?.37f:field==1?.29f:.43f;
                    float ry=field==0?.53f:field==1?.62f:.70f;
                    float d=(u-cx)*(u-cx)/(rx*rx)+(v-cy)*(v-cy)/(ry*ry);
                    float glow=expf(-d*1.1f);
                    Uint8 alpha=(Uint8)(edge*(1.0f+glow*(state==GLASS_ACTIVE?24.0f:17.0f)));
                    Uint8 rr=state==GLASS_ACTIVE?253:117;
                    Uint8 gg=state==GLASS_ACTIVE?115:124;
                    Uint8 bb=state==GLASS_ACTIVE?150:146;
                    row[px]=SDL_MapRGBA(surface->format,rr,gg,bb,alpha);
                }
            }
            SDL_UnlockSurface(surface);
            SDL_Texture *layer=SDL_CreateTextureFromSurface(r,surface);
            SDL_FreeSurface(surface);
            if(layer) { SDL_SetTextureBlendMode(layer,SDL_BLENDMODE_BLEND); glass_cache[slot].liquid[field]=layer; }
        }

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

    /* GPU-friendly compositor: three cached, softly blurred transparent fields.
       Distinct real-time phases drift a little beneath the same capsule rim. */
    if(!panel) {
        const float seconds=SDL_GetTicks()*.001f;
        SDL_RendererInfo info;
        bool software=SDL_GetRendererInfo(r,&info)==0 && (info.flags&SDL_RENDERER_SOFTWARE);
        const float periods[3]={13.0f,15.8f,9.7f};
        for(int layer=0;layer<3;layer++) {
            SDL_Texture *gel=glass_cache[slot].liquid[layer];
            if(!gel) continue;
            float phase=seconds*6.2831853f/periods[layer]+layer*2.1f;
            float breath=software?0.5f:(0.5f+0.5f*sinf(phase));
            Uint8 alpha=(Uint8)(software?170:175+25*breath);
            SDL_SetTextureAlphaMod(gel,alpha);
            int shift=software?0:(int)roundf(sinf(phase)*3.0f);
            for(int yy=0;yy<3;yy++) for(int xx=0;xx<3;xx++) {
                SDL_Rect src={(int)(xs[xx]*2),(int)(ys[yy]*2),
                    (int)((xs[xx+1]-xs[xx])*2),(int)((ys[yy+1]-ys[yy])*2)};
                SDL_FRect dst={dx[xx],dy[yy],dx[xx+1]-dx[xx],dy[yy+1]-dy[yy]};
                if(xx==1) src.x+=shift;
                if(src.w>0&&src.h>0&&dst.w>0&&dst.h>0) SDL_RenderCopyF(r,gel,&src,&dst);
            }
        }
        if(!software) for(int i=0;i<4;i++) {
            float period=10.3f+i*1.37f;
            float progress=fmodf(seconds/period+i*.27f,1.0f);
            float glow=sinf(progress*3.14159265f);
            Uint8 alpha=(Uint8)(glow*(state==GLASS_ACTIVE?11:7));
            SDL_SetRenderDrawColor(r,state==GLASS_ACTIVE?253:117,
                state==GLASS_ACTIVE?115:124,state==GLASS_ACTIVE?150:146,alpha);
            float x=rect.x+rect.w*(.26f+i*.15f)+sinf(progress*6.2831853f+i)*1.5f;
            float y=rect.y+rect.h*(.86f-progress*.60f);
            SDL_RenderDrawPointF(r,x,y);
        }
    }

}
static void drawHighlight(SDL_Renderer *r,HighlightAnim *a,SDL_FRect rect,bool snap) {
    updateHighlight(a,rect,snap); drawGlassPill(r,a->current,GLASS_ACTIVE);
}
static void glass_cleanup(SDL_Renderer *r) {
    for(int i=0;i<20;i++) if(glass_cache[i].renderer==r) {
        SDL_DestroyTexture(glass_cache[i].texture);
        for(int j=0;j<3;j++) if(glass_cache[i].liquid[j]) SDL_DestroyTexture(glass_cache[i].liquid[j]);
        memset(&glass_cache[i],0,sizeof(glass_cache[i]));
    }
}
#endif
