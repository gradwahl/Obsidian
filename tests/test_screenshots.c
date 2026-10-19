#include "../src/screenshots.c"
#include <assert.h>
#include <unistd.h>

static const char *search_html =
    "<a href='/games/details/1-stray'><h3>Stray</h3><p>Windows</p></a>"
    "<a href='/games/details/2-stray'><h3>Stray</h3><p>Sony Playstation 4</p></a>"
    "<a disabled href='/games/details/3-stray'><h3>Stray</h3><p>Sony Playstation 5</p></a>"
    "<a href='/games/details/4-stray-deluxe'><h3>Stray Deluxe</h3><p>Sony Playstation 5</p></a>";
static const char *launchbox_html =
    "<h1 test-attribute>Stray</h1><a href='/platforms/games/219-sony-playstation-5'>Sony Playstation 5</a>"
    "<a href='https://images.launchbox-app.com/cover.jpg' data-title='Stray - Box - Front Image'></a>"
    "<a href='https://images.launchbox-app.com/title.jpg' data-title='Stray - Screenshot - Game Title Image'></a>"
    "<a href='https://images.launchbox-app.com/one.jpg?a=1&amp;b=2' data-title='Stray - Screenshot - Gameplay Image'></a>"
    "<a href='https://images.launchbox-app.com/one.jpg?a=1&amp;b=2' data-title='Stray - Screenshot - Gameplay Image'></a>"
    "<a href='https://images.launchbox-app.com.evil.test/no.jpg' data-title='Stray - Screenshot - Gameplay Image'></a>"
    "<a href='https://images.launchbox-app.com/two.jpg' data-title='Stray - Screenshot - Gameplay Image'></a>";
static const char *playstation_html =
    "<meta name='searchtitle' content='Tales of Arise'>"
    "<div data-component='image-block' data-age-gated='false' data-src='https://gmedia.playstation.com/is/image/SIEPDC/one'>"
    "<picture data-alt='Tales of Arise screenshot'></picture></div>"
    "<div data-component='image-block' data-src='https://gmedia.playstation.com/is/image/SIEPDC/logo'>"
    "<picture data-alt='Game logo'></picture></div>"
    "<div data-component='image-block' data-age-gated='true' data-src='https://gmedia.playstation.com/is/image/SIEPDC/gated'>"
    "<picture data-alt='Gallery screenshot'></picture></div>"
    "<div data-component='image-block' data-src-desktop='https://gmedia.playstation.com/is/image/SIEPDC/two?$100px$'>"
    "<picture data-alt='Gallery Screenshot 2'></picture></div>";

typedef struct { SDL_atomic_t calls; bool offline; } Fixture;
static char *fixture_fetch(void *userdata, const char *url) {
    Fixture *fixture = userdata;
    SDL_AtomicAdd(&fixture->calls,1);
    SDL_Delay(20);
    if (fixture->offline) return NULL;
    const char *html = strstr(url,"/results/") ? search_html :
                       strstr(url,"/images/") ? launchbox_html : playstation_html;
    char *copy = malloc(strlen(html)+1); assert(copy); strcpy(copy,html); return copy;
}
static ScreenshotSet wait_shots(WolodriveApp *app, GameItem *item) {
    ScreenshotSet set;
    Uint32 start = SDL_GetTicks();
    do { screenshots_get(app,item,&set); if (set.state != SHOTS_PENDING) return set; SDL_Delay(5); }
    while (SDL_GetTicks()-start < 4000);
    assert(!"Screenshot worker timed out"); return set;
}
int main(void) {
    assert(SDL_Init(SDL_INIT_TIMER) == 0);
    curl_global_init(CURL_GLOBAL_DEFAULT);
    const char *root = getenv("WOLODRIVE_SCREEN_TEST_DIR"); assert(root && chdir(root) == 0);
    ScreenshotQuery query = {0}; SDL_strlcpy(query.id,"PPSA12345",sizeof(query.id));
    SDL_strlcpy(query.title,"[exFAT] Stray",sizeof(query.title));
    char match[512] = {0};
    assert(shot_launchbox_match(search_html,&query,match,sizeof(match)));
    assert(!strcmp(match,"https://gamesdb.launchbox-app.com/games/images/3-stray"));
    char ambiguous[2048]; snprintf(ambiguous,sizeof(ambiguous),"%s<a href='/games/details/5-stray'><h3>Stray</h3><p>Sony Playstation 5</p></a>",search_html);
    assert(!shot_launchbox_match(ambiguous,&query,match,sizeof(match)));
    ScreenshotSet set = {0}; shot_parse_launchbox(launchbox_html,&query,&set);
    assert(set.count == 2 && !strcmp(set.images[0],"https://images.launchbox-app.com/one.jpg?a=1&b=2"));
    SDL_strlcpy(query.id,"CUSA12345",sizeof(query.id)); memset(&set,0,sizeof(set));
    shot_parse_launchbox(launchbox_html,&query,&set); assert(!set.count);
    SDL_strlcpy(query.title,"Stray Deluxe",sizeof(query.title));
    shot_parse_launchbox(launchbox_html,&query,&set); assert(!set.count);
    SDL_strlcpy(query.title,"Tales of Arise",sizeof(query.title));
    shot_parse_playstation(playstation_html,&query,&set);
    assert(set.count == 2 && strstr(set.images[0],"one?$1600px$") && strstr(set.images[1],"two?$1600px$"));
    assert(!shot_page_allowed("https://www.playstation.com.evil.test/en-us/games/stray/"));
    assert(!shot_page_allowed("http://www.playstation.com/en-us/games/stray/"));
    assert(!shot_image_allowed("https://images.launchbox-app.com@evil.test/one.jpg"));
    assert(!shot_safe_id("../escape"));

    WolodriveApp *app = calloc(1,sizeof(*app)); assert(app);
    app->catalog_mutex = SDL_CreateMutex();
    Fixture fixture = {0}; app->screenshots = screenshots_create(fixture_fetch,&fixture); assert(app->screenshots);
    GameItem item = {0}; strcpy(item.id_code,"PPSA12345"); strcpy(item.title,"Stray");
    set = wait_shots(app,&item); assert(set.state == SHOTS_READY && set.count == 2 && !strcmp(set.source,"LaunchBox"));
    int calls = SDL_AtomicGet(&fixture.calls); assert(calls == 2);
    /* Package prefixes share discovery, and a new service reuses the persisted list. */
    strcpy(item.title,"[pkg] Stray"); set = wait_shots(app,&item); assert(set.count == 2 && SDL_AtomicGet(&fixture.calls) == calls);
    screenshots_destroy(app->screenshots); app->screenshots = screenshots_create(fixture_fetch,&fixture);
    set = wait_shots(app,&item); assert(set.count == 2 && SDL_AtomicGet(&fixture.calls) == calls);
    strcpy(item.id_code,"CUSA54321"); strcpy(item.title,"Tales of Arise");
    set = wait_shots(app,&item); assert(set.count == 2 && !strcmp(set.source,"PlayStation"));
    /* A source change alters the discovery key; catalog screenshots take priority. */
    strcpy(item.playstation_url,"https://www.playstation.com/en-us/games/tales-of-arise/");
    int before = SDL_AtomicGet(&fixture.calls); set = wait_shots(app,&item);
    assert(set.count == 2 && SDL_AtomicGet(&fixture.calls) > before);
    item.screenshot_count = 1; strcpy(item.screenshot_urls[0],"https://example.test/catalog.jpg");
    before = SDL_AtomicGet(&fixture.calls); set = wait_shots(app,&item);
    assert(set.count == 1 && !strcmp(set.source,"Catalog") && SDL_AtomicGet(&fixture.calls) == before);
    item.screenshot_count = 0;
    strcpy(item.id_code,"PPSA00009"); strcpy(item.title,"Unknown title"); item.playstation_url[0] = 0;
    set = wait_shots(app,&item); assert(set.state == SHOTS_EMPTY && !set.count);
    screenshots_destroy(app->screenshots);
    fixture.offline = true; app->screenshots = screenshots_create(fixture_fetch,&fixture);
    strcpy(item.id_code,"PPSA00010");
    set = wait_shots(app,&item); assert(set.state == SHOTS_FAILED);
    screenshots_destroy(app->screenshots); app->screenshots = screenshots_create(fixture_fetch,&fixture);
    before = SDL_AtomicGet(&fixture.calls); set = wait_shots(app,&item);
    assert(set.state == SHOTS_FAILED && SDL_AtomicGet(&fixture.calls) == before);
    /* Local screenshots work without provider access and cannot escape their folder. */
    SHOT_MKDIR("assets/screenshots/PPSA00011");
    FILE *local = fopen("assets/screenshots/PPSA00011/01.jpg","wb"); assert(local); fputs("fixture",local); fclose(local);
    strcpy(item.id_code,"PPSA00011"); before = SDL_AtomicGet(&fixture.calls);
    set = wait_shots(app,&item); assert(set.count == 1 && !strcmp(set.source,"Local") && SDL_AtomicGet(&fixture.calls) == before);
    ScreenshotSet cached_set = {0};
    cached_set.state = SHOTS_READY; cached_set.count = 1;
    strcpy(cached_set.source,"LaunchBox"); strcpy(cached_set.page,"https://gamesdb.launchbox-app.com/games/images/3-stray");
    strcpy(cached_set.images[0],"https://images.launchbox-app.com/test.jpg");
    shot_cache_write(999,&cached_set);
    memset(&set,0,sizeof(set)); assert(shot_cache_read(999,&set) && set.count == 1);
    char path[256]; shot_cache_path(999,path,sizeof(path));
    FILE *broken = fopen(path,"wb"); assert(broken); fputs("WDSCREEN2\n1\n1\nLaunchBox\n\nhttps://images.launchbox-app.com/test.jpg\n",broken); fclose(broken);
    memset(&set,0,sizeof(set)); assert(!shot_cache_read(999,&set));
    broken = fopen(path,"wb"); assert(broken);
    fprintf(broken,"WDSCREEN2\n%lld\n1\nLaunchBox\nhttps://gamesdb.launchbox-app.com/games/images/3-stray\nhttps://evil.test/no.jpg\n",(long long)time(NULL)+1000); fclose(broken);
    assert(!shot_cache_read(999,&set));
    screenshots_destroy(app->screenshots); SDL_DestroyMutex(app->catalog_mutex); free(app);
    curl_global_cleanup(); SDL_Quit();
    puts("Screenshot discovery: exact platform/title, ambiguity, image types, entities, gated media, URL hosts, caches, source changes, catalog/local priority and worker shutdown passed.");
    return 0;
}
