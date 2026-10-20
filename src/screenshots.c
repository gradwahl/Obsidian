#include "screenshots.h"
#include "text.h"
#include <curl/curl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <direct.h>
#define SHOT_MKDIR(path) _mkdir(path)
#else
#define SHOT_MKDIR(path) mkdir(path, 0777)
#endif
#ifdef __PS5__
#define SHOT_ROOT WOLODRIVE_DATA_PATH "/screenshots"
#else
#define SHOT_ROOT "assets/screenshots"
#endif
#define SHOT_SLOTS 32
#define SHOT_HTML_LIMIT (2u * 1024u * 1024u)

typedef struct {
    char id[32], title[128], launchbox[512], playstation[512];
    bool dlc;
} ScreenshotQuery;
typedef struct {
    Uint64 key;
    ScreenshotQuery query;
    ScreenshotSet result;
    Uint32 used, refresh_at;
    bool busy;
} ScreenshotEntry;
struct ScreenshotService {
    SDL_mutex *mutex;
    SDL_cond *condition;
    SDL_Thread *thread;
    SDL_atomic_t stop;
    ScreenshotFetch fetch;
    void *userdata;
    Uint32 last_launchbox, last_playstation;
    ScreenshotEntry entries[SHOT_SLOTS];
};

static bool shot_prefix(const char *value, const char *prefix) {
    return strncmp(value, prefix, strlen(prefix)) == 0;
}
static bool shot_page_allowed(const char *url) {
    return shot_prefix(url, "https://gamesdb.launchbox-app.com/games/") ||
           shot_prefix(url, "https://www.playstation.com/en-us/games/");
}
static bool shot_image_allowed(const char *url) {
    return shot_prefix(url, "https://images.launchbox-app.com/") ||
           shot_prefix(url, "https://gmedia.playstation.com/is/image/");
}
static bool shot_safe_id(const char *id) {
    if (!*id) return false;
    for (; *id; id++) if (!SDL_isalnum((unsigned char)*id) && *id != '_' && *id != '-') return false;
    return true;
}

/* Bounded HTML attributes, including single quotes and entity-escaped URLs. */
static bool shot_attr(const char *tag, const char *end, const char *name, char *out, size_t capacity) {
    const char *p = tag + 1;
    while (p < end && !SDL_isspace((unsigned char)*p)) p++;
    while (p < end) {
        while (p < end && (SDL_isspace((unsigned char)*p) || *p == '/')) p++;
        const char *start = p;
        while (p < end && (SDL_isalnum((unsigned char)*p) || *p == '-' || *p == '_' || *p == ':')) p++;
        size_t length = p-start;
        if (!length) { p++; continue; }
        while (p < end && SDL_isspace((unsigned char)*p)) p++;
        if (p >= end || *p != '=') continue;
        p++;
        while (p < end && SDL_isspace((unsigned char)*p)) p++;
        if (p >= end || (*p != '"' && *p != '\'')) continue;
        char quote = *p++;
        const char *value = p;
        while (p < end && *p != quote) p++;
        if (length == strlen(name) && SDL_strncasecmp(start,name,length) == 0) {
            size_t bytes = p-value;
            if (bytes >= capacity || bytes >= 2048) return false;
            char raw[2048]; memcpy(raw,value,bytes); raw[bytes] = 0;
            text_plain_description(raw,out,capacity);
            return out[0] != 0;
        }
        if (p < end) p++;
    }
    return false;
}
static const char *shot_tag_end(const char *start) {
    char quote = 0;
    for (; *start; start++) {
        if (quote) { if (*start == quote) quote = 0; }
        else if (*start == '"' || *start == '\'') quote = *start;
        else if (*start == '>') return start;
    }
    return NULL;
}
static void shot_text(const char *start, const char *end, char *out, size_t capacity) {
    char raw[512];
    if (!start || !end || end < start || (size_t)(end-start) >= sizeof(raw)) { out[0] = 0; return; }
    memcpy(raw,start,end-start); raw[end-start] = 0;
    text_plain_description(raw,out,capacity);
}
static void shot_clean_title(const char *title, char *out, size_t capacity, bool slug) {
    /* Strip source package prefixes, but retain named editions and sequels. */
    while (*title == '[') {
        const char *last = strchr(title,']');
        if (!last || last-title > 16) break;
        char prefix[20]; memcpy(prefix,title+1,last-title-1); prefix[last-title-1] = 0;
        if (SDL_strcasecmp(prefix,"exfat") && SDL_strcasecmp(prefix,"ffpkg") &&
            SDL_strcasecmp(prefix,"pkg") && SDL_strcasecmp(prefix,"ps4") && SDL_strcasecmp(prefix,"ps5")) break;
        title = last+1; while (*title == ' ') title++;
    }
    size_t used = 0;
    for (const unsigned char *p = (const unsigned char *)title; *p && used+1 < capacity; p++) {
        if (*p < 128 && SDL_isalnum(*p)) out[used++] = SDL_tolower(*p);
        else if (*p >= 128 || *p == '\'') continue;
        else if (slug && used && out[used-1] != '-') out[used++] = '-';
    }
    if (used && out[used-1] == '-') used--;
    out[used] = 0;
}
static bool shot_same_title(const char *a, const char *b) {
    char first[128], second[128];
    shot_clean_title(a,first,sizeof(first),false); shot_clean_title(b,second,sizeof(second),false);
    return first[0] && strcmp(first,second) == 0;
}
static void shot_add(ScreenshotSet *set, const char *url) {
    if (set->count >= MAX_GAME_SCREENSHOTS || strlen(url) >= sizeof(set->images[0])) return;
    for (int i = 0; i < set->count; i++) if (strcmp(url,set->images[i]) == 0) return;
    SDL_strlcpy(set->images[set->count++],url,sizeof(set->images[0]));
}
static bool shot_heading_matches(const char *html, const char *title) {
    const char *start = strstr(html,"<h1");
    const char *end = start ? strstr(start,"</h1>") : NULL;
    char heading[128]; shot_text(start,end,heading,sizeof(heading));
    return shot_same_title(heading,title);
}
static bool shot_launchbox_match(const char *html, const ScreenshotQuery *query, char *out, size_t capacity) {
    const char *p = html;
    const char *platform = shot_prefix(query->id,"CUSA") ? "Sony Playstation 4" :
                           shot_prefix(query->id,"PPSA") ? "Sony Playstation 5" : NULL;
    if (!platform) return false;
    int matches = 0;
    while ((p = strstr(p,"<a")) != NULL) {
        const char *end = shot_tag_end(p); if (!end) break;
        char href[512];
        if (!shot_attr(p,end,"href",href,sizeof(href)) || !shot_prefix(href,"/games/details/")) { p = end+1; continue; }
        const char *last = strstr(end,"</a>"); if (!last) break;
        const char *heading = strstr(end,"<h3"), *heading_end = heading ? strstr(heading,"</h3>") : NULL;
        const char *plat = heading_end ? strstr(heading_end,"<p") : NULL, *plat_end = plat ? strstr(plat,"</p>") : NULL;
        char title[128], console[80];
        if (heading_end && plat_end && heading_end < last && plat_end < last) {
            shot_text(heading,heading_end,title,sizeof(title)); shot_text(plat,plat_end,console,sizeof(console));
            if (shot_same_title(title,query->title) && SDL_strcasecmp(console,platform) == 0) {
                const char *suffix = href+strlen("/games/details/");
                if (strlen(suffix)+48 >= 512) { p = last+4; continue; }
                char candidate[512]; SDL_strlcpy(candidate,"https://gamesdb.launchbox-app.com/games/images/",sizeof(candidate));
                SDL_strlcat(candidate,suffix,sizeof(candidate));
                if (!matches || strcmp(candidate,out)) { SDL_strlcpy(out,candidate,capacity); matches++; }
            }
        }
        p = last+4;
    }
    return matches == 1;
}
static void shot_parse_launchbox(const char *html, const ScreenshotQuery *query, ScreenshotSet *set) {
    if (!shot_heading_matches(html,query->title)) return;
    const char *platform = shot_prefix(query->id,"CUSA") ? "Sony Playstation 4" :
                           shot_prefix(query->id,"PPSA") ? "Sony Playstation 5" : NULL;
    if (!platform) return;
    bool correct_platform = false;
    const char *platform_tag = html;
    while ((platform_tag = strstr(platform_tag,"<a")) != NULL) {
        const char *end = shot_tag_end(platform_tag); if (!end) break;
        const char *last = strstr(end,"</a>");
        char href[256], name[80];
        if (last && shot_attr(platform_tag,end,"href",href,sizeof(href)) && shot_prefix(href,"/platforms/games/")) {
            shot_text(end+1,last,name,sizeof(name));
            if (!SDL_strcasecmp(name,platform)) correct_platform = true;
        }
        platform_tag = end+1;
    }
    if (!correct_platform) return;
    const char *p = html;
    while (set->count < MAX_GAME_SCREENSHOTS && (p = strstr(p,"<a")) != NULL) {
        const char *end = shot_tag_end(p); if (!end) break;
        char kind[256], image[512];
        if (shot_attr(p,end,"data-title",kind,sizeof(kind)) && strstr(kind,"Screenshot - Gameplay") &&
            shot_attr(p,end,"href",image,sizeof(image)) && shot_prefix(image,"https://images.launchbox-app.com/")) shot_add(set,image);
        p = end+1;
    }
}
static void shot_parse_playstation(const char *html, const ScreenshotQuery *query, ScreenshotSet *set) {
    const char *p = html;
    bool title_matches = false;
    while ((p = strstr(p,"<meta")) != NULL) {
        const char *end = shot_tag_end(p); if (!end) break;
        char name[64], content[256];
        if (shot_attr(p,end,"name",name,sizeof(name)) && !strcmp(name,"searchtitle") &&
            shot_attr(p,end,"content",content,sizeof(content))) title_matches = shot_same_title(content,query->title);
        p = end+1;
    }
    if (!title_matches) return;
    p = html;
    while (set->count < MAX_GAME_SCREENSHOTS && (p = strstr(p,"<div")) != NULL) {
        const char *end = shot_tag_end(p); if (!end) break;
        char component[64], gated[32], image[512];
        if (shot_attr(p,end,"data-component",component,sizeof(component)) && !strcmp(component,"image-block") &&
            (!shot_attr(p,end,"data-age-gated",gated,sizeof(gated)) || strcmp(gated,"true")) &&
            (shot_attr(p,end,"data-src-desktop",image,sizeof(image)) || shot_attr(p,end,"data-src",image,sizeof(image))) &&
            shot_prefix(image,"https://gmedia.playstation.com/is/image/")) {
            const char *picture = strstr(end,"<picture");
            const char *next = strstr(end,"data-component=");
            if (picture && picture-end < 4096 && (!next || picture < next)) {
                const char *picture_end = shot_tag_end(picture);
                char label[256];
                if (picture_end && shot_attr(picture,picture_end,"data-alt",label,sizeof(label))) {
                    for (char *c = label; *c; c++) *c = SDL_tolower((unsigned char)*c);
                    if (strstr(label,"screenshot")) {
                        char *query_start = strchr(image,'?'); if (query_start) *query_start = 0;
                        if (strlen(image)+12 < sizeof(image)) { SDL_strlcat(image,"?$1600px$",sizeof(image)); shot_add(set,image); }
                    }
                }
            }
        }
        p = end+1;
    }
}

typedef struct { char *data; size_t size; } ShotBuffer;
static size_t shot_write(void *data, size_t size, size_t count, void *userdata) {
    ShotBuffer *buffer = userdata;
    size_t bytes = size*count;
    if (bytes > SHOT_HTML_LIMIT-buffer->size) return 0;
    char *grown = realloc(buffer->data,buffer->size+bytes+1); if (!grown) return 0;
    buffer->data = grown; memcpy(grown+buffer->size,data,bytes); buffer->size += bytes; grown[buffer->size] = 0;
    return bytes;
}
static int shot_progress(void *userdata, curl_off_t a, curl_off_t b, curl_off_t c, curl_off_t d) {
    (void)a; (void)b; (void)c; (void)d;
    return SDL_AtomicGet(&((struct ScreenshotService *)userdata)->stop);
}
static char *shot_http(void *userdata, const char *url) {
    struct ScreenshotService *service = userdata;
    char current[1024]; SDL_strlcpy(current,url,sizeof(current));
    for (int redirects = 0; redirects <= 3 && shot_page_allowed(current); redirects++) {
        bool ps = shot_prefix(current,"https://www.playstation.com/");
        Uint32 *last = ps ? &service->last_playstation : &service->last_launchbox;
        Uint32 delay = ps ? 5000 : 1000;
        while (*last && SDL_GetTicks()-*last < delay) {
            if (SDL_AtomicGet(&service->stop)) return NULL;
            SDL_Delay(25);
        }
        if (SDL_AtomicGet(&service->stop)) return NULL;
        *last = SDL_GetTicks();
        CURL *curl = curl_easy_init(); if (!curl) return NULL;
        ShotBuffer buffer = {0};
        curl_easy_setopt(curl,CURLOPT_URL,current);
        curl_easy_setopt(curl,CURLOPT_USERAGENT,"PolDrive/2.0 (game screenshot gallery)");
        curl_easy_setopt(curl,CURLOPT_CONNECTTIMEOUT,5L);
        curl_easy_setopt(curl,CURLOPT_TIMEOUT,12L);
        curl_easy_setopt(curl,CURLOPT_WRITEFUNCTION,shot_write);
        curl_easy_setopt(curl,CURLOPT_WRITEDATA,&buffer);
        curl_easy_setopt(curl,CURLOPT_XFERINFOFUNCTION,shot_progress);
        curl_easy_setopt(curl,CURLOPT_XFERINFODATA,service);
        curl_easy_setopt(curl,CURLOPT_NOPROGRESS,0L);
        CURLcode result = curl_easy_perform(curl);
        long status = 0; curl_easy_getinfo(curl,CURLINFO_RESPONSE_CODE,&status);
        char *redirect = NULL; curl_easy_getinfo(curl,CURLINFO_REDIRECT_URL,&redirect);
        bool follow = result == CURLE_OK && status >= 300 && status < 400 && redirect && shot_page_allowed(redirect);
        if (follow) SDL_strlcpy(current,redirect,sizeof(current));
        curl_easy_cleanup(curl);
        if (result == CURLE_OK && status >= 200 && status < 300 && buffer.data) return buffer.data;
        free(buffer.data);
        if (!follow) break;
    }
    return NULL;
}
static Uint64 shot_key(const ScreenshotQuery *query) {
    char canonical[128]; shot_clean_title(query->title,canonical,sizeof(canonical),false);
    const char *fields[] = {query->id,canonical,query->launchbox,query->playstation};
    Uint64 key = 14695981039346656037ULL;
    for (int i = 0; i < 4; i++) {
        for (const unsigned char *p = (const unsigned char *)fields[i]; *p; p++) { key ^= *p; key *= 1099511628211ULL; }
        key ^= '\n'; key *= 1099511628211ULL;
    }
    if (shot_safe_id(query->id)) {
        char path[256]; struct stat st;
        snprintf(path,sizeof(path),"%s/%s.sources",SHOT_ROOT,query->id);
        if (stat(path,&st) == 0) key ^= (Uint64)st.st_mtime * 1099511628211ULL ^ (Uint64)st.st_size;
        snprintf(path,sizeof(path),"%s/%s",SHOT_ROOT,query->id);
        if (stat(path,&st) == 0) key ^= (Uint64)st.st_mtime * 16777619ULL;
    }
    return key ^ query->dlc;
}
static void shot_cache_path(Uint64 key, char *path, size_t capacity) {
    snprintf(path,capacity,"%s/%016llx.list",SHOT_ROOT,(unsigned long long)key);
}
static bool shot_cache_read(Uint64 key, ScreenshotSet *set) {
    char path[256], line[600]; shot_cache_path(key,path,sizeof(path));
    FILE *file = fopen(path,"rb"); if (!file) return false;
    long long expires = 0;
    bool valid = fgets(line,sizeof(line),file) && !strcmp(line,"WDSCREEN2\n") &&
                 fgets(line,sizeof(line),file) && (expires = strtoll(line,NULL,10)) > (long long)time(NULL);
    int state = SHOTS_PENDING;
    valid = valid && fgets(line,sizeof(line),file) && (state = atoi(line)) >= SHOTS_READY && state <= SHOTS_FAILED;
    if (valid && fgets(set->source,sizeof(set->source),file) && fgets(set->page,sizeof(set->page),file)) {
        set->source[strcspn(set->source,"\r\n")] = 0; set->page[strcspn(set->page,"\r\n")] = 0;
        if (set->page[0] && !shot_page_allowed(set->page)) valid = false;
        while (valid && fgets(line,sizeof(line),file)) {
            line[strcspn(line,"\r\n")] = 0;
            if (!shot_image_allowed(line)) valid = false;
            else shot_add(set,line);
        }
    } else valid = false;
    if (valid && ((state == SHOTS_READY) != (set->count > 0))) valid = false;
    fclose(file);
    if (!valid) { memset(set,0,sizeof(*set)); return false; }
    set->state = (ScreenshotState)state;
    return true;
}
static void shot_cache_write(Uint64 key, const ScreenshotSet *set) {
    SHOT_MKDIR("assets");
#ifdef __PS5__
    SHOT_MKDIR("/data/homebrew"); SHOT_MKDIR(WOLODRIVE_DATA_PATH);
#endif
    SHOT_MKDIR(SHOT_ROOT);
    char path[256], temporary[280]; shot_cache_path(key,path,sizeof(path));
    snprintf(temporary,sizeof(temporary),"%s.tmp",path);
    FILE *file = fopen(temporary,"wb"); if (!file) return;
    fprintf(file,"WDSCREEN2\n%lld\n%d\n%s\n%s\n",(long long)time(NULL)+(set->count ? 7*86400 : 1800),set->state,set->source,set->page);
    for (int i = 0; i < set->count; i++) fprintf(file,"%s\n",set->images[i]);
    bool okay = !ferror(file); if (fclose(file)) okay = false;
    if (okay) { remove(path); if (rename(temporary,path) == 0) return; }
    remove(temporary);
}
static bool shot_local(const ScreenshotQuery *query, ScreenshotSet *set) {
    if (!shot_safe_id(query->id)) return false;
    const char *extensions[] = {"jpg","png","webp"};
    for (int i = 1; i <= MAX_GAME_SCREENSHOTS; i++) for (int e = 0; e < 3; e++) {
        char path[256]; struct stat st;
        snprintf(path,sizeof(path),"%s/%s/%02d.%s",SHOT_ROOT,query->id,i,extensions[e]);
        if (stat(path,&st) == 0 && st.st_size > 0) { shot_add(set,path); break; }
    }
    if (!set->count) return false;
    SDL_strlcpy(set->source,"Local",sizeof(set->source)); set->state = SHOTS_READY;
    return true;
}
static ScreenshotSet shot_resolve(struct ScreenshotService *service, ScreenshotQuery query, bool *cached) {
    *cached = false;
    bool fetched = false;
    ScreenshotSet set = {0};
    if (shot_local(&query,&set)) return set;
    if (shot_cache_read(shot_key(&query),&set)) { *cached = true; return set; }
    if (shot_safe_id(query.id)) {
        char path[256], line[600]; snprintf(path,sizeof(path),"%s/%s.sources",SHOT_ROOT,query.id);
        FILE *file = fopen(path,"rb");
        if (file) {
            while (fgets(line,sizeof(line),file)) {
                line[strcspn(line,"\r\n")] = 0;
                if (shot_prefix(line,"https://gamesdb.launchbox-app.com/games/images/")) SDL_strlcpy(query.launchbox,line,sizeof(query.launchbox));
                if (shot_prefix(line,"https://www.playstation.com/en-us/games/")) SDL_strlcpy(query.playstation,line,sizeof(query.playstation));
            }
            fclose(file);
        }
    }
    char url[1024], matched[512] = {0};
    if (query.launchbox[0] && shot_prefix(query.launchbox,"https://gamesdb.launchbox-app.com/games/images/"))
        SDL_strlcpy(matched,query.launchbox,sizeof(matched));
    else if (!query.dlc) {
        char title[128]; shot_clean_title(query.title,title,sizeof(title),true);
        for (char *c = title; *c; c++) if (*c == '-') *c = ' ';
        char keyword[128] = {0};
        for (const char *word = title; *word;) {
            const char *last = strchr(word,' '); if (!last) last = word+strlen(word);
            if ((size_t)(last-word) > strlen(keyword)) {
                memcpy(keyword,word,last-word); keyword[last-word] = 0;
            }
            word = *last ? last+1 : last;
        }
        const char *terms[] = {title,keyword};
        for (int attempt = 0; attempt < 2 && !matched[0]; attempt++) {
            if (!terms[attempt][0] || (attempt && (strlen(keyword) < 4 || !strcmp(title,keyword)))) break;
            CURL *curl = curl_easy_init();
            char *escaped = curl ? curl_easy_escape(curl,terms[attempt],0) : NULL;
            if (escaped) {
                snprintf(url,sizeof(url),"https://gamesdb.launchbox-app.com/games/results/%s?searchText=%s",escaped,escaped);
                char *html = service->fetch(service->userdata,url);
                if (html) { fetched = true; if (!shot_launchbox_match(html,&query,matched,sizeof(matched))) matched[0] = 0; free(html); }
            }
            curl_free(escaped); if (curl) curl_easy_cleanup(curl);
        }
    }
    if (matched[0]) {
        char *html = service->fetch(service->userdata,matched);
        if (html) { fetched = true; shot_parse_launchbox(html,&query,&set); free(html); }
        if (set.count) { SDL_strlcpy(set.source,"LaunchBox",sizeof(set.source)); SDL_strlcpy(set.page,matched,sizeof(set.page)); }
    }
    if (!set.count && !query.dlc) {
        if (query.playstation[0] && shot_prefix(query.playstation,"https://www.playstation.com/en-us/games/")) SDL_strlcpy(url,query.playstation,sizeof(url));
        else { char slug[128]; shot_clean_title(query.title,slug,sizeof(slug),true); snprintf(url,sizeof(url),"https://www.playstation.com/en-us/games/%s/",slug); }
        char *html = service->fetch(service->userdata,url);
        if (html) { fetched = true; shot_parse_playstation(html,&query,&set); free(html); }
        if (set.count) { SDL_strlcpy(set.source,"PlayStation",sizeof(set.source)); SDL_strlcpy(set.page,url,sizeof(set.page)); }
    }
    set.state = set.count ? SHOTS_READY : fetched ? SHOTS_EMPTY : SHOTS_FAILED;
    /* Persist against the original query, including catalog-supplied source URLs. */
    return set;
}
static int shot_worker(void *userdata) {
    struct ScreenshotService *service = userdata;
    SDL_LockMutex(service->mutex);
    while (!SDL_AtomicGet(&service->stop)) {
        ScreenshotEntry *next = NULL;
        for (int i = 0; i < SHOT_SLOTS; i++) {
            ScreenshotEntry *entry = &service->entries[i];
            if (entry->key && !entry->busy && entry->result.state == SHOTS_PENDING && (!next || entry->used > next->used)) next = entry;
        }
        if (!next) { SDL_CondWait(service->condition,service->mutex); continue; }
        next->busy = true;
        ScreenshotQuery query = next->query;
        Uint64 key = next->key;
        SDL_UnlockMutex(service->mutex);
        bool cached = false;
        ScreenshotSet result = shot_resolve(service,query,&cached);
        if (!SDL_AtomicGet(&service->stop) && !cached && strcmp(result.source,"Local")) shot_cache_write(key,&result);
        SDL_LockMutex(service->mutex);
        next->result = result; next->busy = false;
        next->refresh_at = SDL_GetTicks() + (result.count ? 3600000 : 1800000);
    }
    SDL_UnlockMutex(service->mutex); return 0;
}
struct ScreenshotService *screenshots_create(ScreenshotFetch fetch, void *userdata) {
    struct ScreenshotService *service = calloc(1,sizeof(*service)); if (!service) return NULL;
    service->mutex = SDL_CreateMutex(); service->condition = SDL_CreateCond();
    if (!service->mutex || !service->condition) { screenshots_destroy(service); return NULL; }
    service->fetch = fetch ? fetch : shot_http; service->userdata = fetch ? userdata : service;
    service->thread = SDL_CreateThread(shot_worker,"screenshots",service);
    if (!service->thread) { screenshots_destroy(service); return NULL; }
    return service;
}
void screenshots_get(WolodriveApp *app, const GameItem *item, ScreenshotSet *out) {
    memset(out,0,sizeof(*out));
    ScreenshotQuery query = {0};
    if (app->catalog_mutex) SDL_LockMutex(app->catalog_mutex);
    SDL_strlcpy(query.id,item->id_code,sizeof(query.id)); SDL_strlcpy(query.title,item->title,sizeof(query.title));
    SDL_strlcpy(query.launchbox,item->launchbox_url,sizeof(query.launchbox)); SDL_strlcpy(query.playstation,item->playstation_url,sizeof(query.playstation));
    query.dlc = item->is_dlc;
    for (int i = 0; i < item->screenshot_count && i < MAX_GAME_SCREENSHOTS; i++) {
        const char *url = item->screenshot_urls[i];
        if (shot_prefix(url,"https://") || shot_prefix(url,"http://")) shot_add(out,url);
    }
    if (app->catalog_mutex) SDL_UnlockMutex(app->catalog_mutex);
    if (out->count) { out->state = SHOTS_READY; SDL_strlcpy(out->source,"Catalog",sizeof(out->source)); return; }
    if (!app->screenshots) app->screenshots = screenshots_create(NULL,NULL);
    struct ScreenshotService *service = app->screenshots;
    if (!service) { out->state = SHOTS_FAILED; return; }
    Uint64 key = shot_key(&query);
    SDL_LockMutex(service->mutex);
    ScreenshotEntry *entry = NULL, *oldest = NULL;
    for (int i = 0; i < SHOT_SLOTS; i++) {
        ScreenshotEntry *candidate = &service->entries[i];
        if (candidate->key == key) { entry = candidate; break; }
        if (!candidate->busy && (!oldest || !candidate->key || candidate->used < oldest->used)) oldest = candidate;
    }
    if (!entry && oldest) {
        memset(oldest,0,sizeof(*oldest)); oldest->key = key; oldest->query = query; entry = oldest;
    }
    if (entry) {
        if (!entry->busy && entry->result.state != SHOTS_PENDING && (Sint32)(SDL_GetTicks()-entry->refresh_at) >= 0)
            memset(&entry->result,0,sizeof(entry->result));
        entry->used = SDL_GetTicks(); *out = entry->result; SDL_CondSignal(service->condition);
    }
    SDL_UnlockMutex(service->mutex);
}
void screenshots_destroy(struct ScreenshotService *service) {
    if (!service) return;
    SDL_AtomicSet(&service->stop,1);
    if (service->condition) SDL_CondSignal(service->condition);
    if (service->thread) SDL_WaitThread(service->thread,NULL);
    SDL_DestroyCond(service->condition); SDL_DestroyMutex(service->mutex); free(service);
}
