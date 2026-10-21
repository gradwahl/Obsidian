/* Metadata and columns shared by native Home, console lists and Search. */
static const int GAME_LIST_COLUMN_WEIGHTS[4] = {17, 39, 32, 12};
static const char *game_list_console(const GameItem *item) {
    for (int pass=0; pass<2; pass++) for(int c=0;c<library_company_count();c++) {
        const LibraryCompany *company=library_company_at(c);
        for(int i=0;i<company->console_count;i++) {
            const LibraryConsole *console=&company->consoles[i];
            if ((pass==0 && library_console_matches_platform(console,item->platform)) ||
                (pass==1 && library_console_matches_id(console,item->id_code))) return console->name;
        }
    }
    return item->platform[0] ? item->platform : "—";
}
static const char *game_list_format(const GameItem *item) {
    switch(item->format) {
        case FMT_EXFAT: return "EXFAT";
        case FMT_FFPKG: return "FFPKG";
        case FMT_PKG: return "PKG";
        default: return "—";
    }
}
static void game_list_size(const GameItem *item, char out[64]) {
    double gb=item->size_gb;
    if(!isfinite(gb) || gb<=0) SDL_strlcpy(out,"—",64);
    else if(gb>=1000) snprintf(out,64,"%.1f TB",gb/1000);
    else if(gb>=1) snprintf(out,64,"%.1f GB",gb);
    else if(gb>=0.001) snprintf(out,64,"%.1f MB",gb*1000);
    else if(gb>=0.000001) snprintf(out,64,"%.1f KB",gb*1000000);
    else snprintf(out,64,"%.0f B",fmax(1,round(gb*1000000000)));
}
static SDL_Rect game_list_column(SDL_Rect box,int column) {
    int available=box.w-100, preceding=0;
    for(int i=0;i<column;i++) preceding+=GAME_LIST_COLUMN_WEIGHTS[i];
    return (SDL_Rect){box.x+20+column*20+available*preceding/100,
        box.y+12,available*(preceding+GAME_LIST_COLUMN_WEIGHTS[column])/100-available*preceding/100,40};
}
static void render_game_list_headers(WolodriveApp *app, SDL_Rect box) {
    const TextId labels[]={TXT_LIST_CONSOLE,TXT_LIST_NAME,TXT_LIST_FORMAT,TXT_LIST_SIZE};
    SDL_Color muted={188,190,200,255};
    for(int i=0;i<4;i++) {
        SDL_Rect cell=game_list_column(box,i); cell.y=box.y; cell.h=24;
        const char *label=TR(app,labels[i]);
        if(i==3) { int w=0,h=0; TTF_SizeUTF8(app->font_micro,label,&w,&h); cell.x+=SDL_max(0,cell.w-w); }
        draw_single_line(app,app->font_micro,label,cell.x,cell.y,muted,cell);
    }
}
static void render_game_list_columns(WolodriveApp *app, GameItem *item, SDL_Rect box) {
    char size[64]; game_list_size(item,size);
    const char *values[]={game_list_console(item),item->title,game_list_format(item),size};
    for(int i=0;i<4;i++) {
        SDL_Rect cell=game_list_column(box,i);
        TTF_Font *font=i==1 ? app->font_body : app->font_small;
        SDL_Color color=i==1 ? (SDL_Color){246,246,249,255} : (SDL_Color){188,190,200,255};
        int y=box.y+(i==1?12:20);
        if(i==3) { int w=0,h=0; TTF_SizeUTF8(font,values[i],&w,&h); cell.x+=SDL_max(0,cell.w-w); }
        draw_single_line(app,font,values[i],cell.x,y,color,cell);
    }
}
