#include "../src/screenshots.c"
int main(int argc, char **argv) {
    SDL_Init(SDL_INIT_TIMER); curl_global_init(CURL_GLOBAL_DEFAULT);
    struct ScreenshotService service = {0}; service.fetch=shot_http; service.userdata=&service;
    ScreenshotQuery query = {0}; SDL_strlcpy(query.id,argc>2?argv[2]:"PPSA00000",sizeof(query.id));
    SDL_strlcpy(query.title,argc>1?argv[1]:"Stray",sizeof(query.title));
    if(argc>3) SDL_strlcpy(query.launchbox,"https://gamesdb.launchbox-app.com/games/images/405297-stray",sizeof(query.launchbox));
    bool cached=false; ScreenshotSet set=shot_resolve(&service,query,&cached);
    printf("state=%d count=%d source=%s page=%s\n",set.state,set.count,set.source,set.page);
    for(int i=0;i<set.count;i++) printf("%s\n",set.images[i]);
    curl_global_cleanup(); SDL_Quit(); return set.count?0:1;
}
