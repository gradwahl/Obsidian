#include "emulator_download.h"
#include <curl/curl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <direct.h>
#define EMU_MKDIR(p) _mkdir(p)
#else
#define EMU_MKDIR(p) mkdir((p), 0755)
#endif

#ifdef __PS5__
#define EMULATOR_STORAGE_DIR WOLODRIVE_DATA_PATH "/emulators"
#else
#define EMULATOR_STORAGE_DIR "build/emulators"
#endif

/* A small standalone SHA-256 verifier keeps PS5 builds independent from an
   external OpenSSL library. No executable content is trusted without its
   matching GitHub release metadata digest and byte count. */
typedef struct {
    uint32_t h[8];
    uint64_t length;
    unsigned char block[64];
    unsigned fill;
} EmuSHA256;

static uint32_t rotate32(uint32_t value, int bits) {
    return (value >> bits) | (value << (32 - bits));
}
static const uint32_t sha_constants[64] = {
  0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
  0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
  0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
  0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
  0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
  0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
  0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
  0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
};
static void sha_compress(EmuSHA256 *ctx) {
    uint32_t w[64];
    for (int i = 0; i < 16; i++) {
        w[i] = ((uint32_t)ctx->block[4*i]<<24) | ((uint32_t)ctx->block[4*i+1]<<16) |
               ((uint32_t)ctx->block[4*i+2]<<8) | ctx->block[4*i+3];
    }
    for (int i = 16; i < 64; i++) {
        uint32_t x=w[i-15], y=w[i-2];
        w[i] = w[i-16] + (rotate32(x,7)^rotate32(x,18)^(x>>3)) +
               w[i-7] + (rotate32(y,17)^rotate32(y,19)^(y>>10));
    }
    uint32_t a=ctx->h[0],b=ctx->h[1],c=ctx->h[2],d=ctx->h[3];
    uint32_t e=ctx->h[4],f=ctx->h[5],g=ctx->h[6],h=ctx->h[7];
    for(int i=0;i<64;i++){
        uint32_t s1=rotate32(e,6)^rotate32(e,11)^rotate32(e,25);
        uint32_t ch=(e&f)^(~e&g);
        uint32_t t1=h+s1+ch+sha_constants[i]+w[i];
        uint32_t s0=rotate32(a,2)^rotate32(a,13)^rotate32(a,22);
        uint32_t maj=(a&b)^(a&c)^(b&c);
        uint32_t t2=s0+maj;
        h=g;g=f;f=e;e=d+t1;d=c;c=b;b=a;a=t1+t2;
    }
    ctx->h[0]+=a;ctx->h[1]+=b;ctx->h[2]+=c;ctx->h[3]+=d;
    ctx->h[4]+=e;ctx->h[5]+=f;ctx->h[6]+=g;ctx->h[7]+=h;
}
static void sha_init(EmuSHA256 *ctx) {
    const uint32_t init[8] = {0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,
                              0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19};
    memcpy(ctx->h,init,sizeof(init));
    ctx->length=0;ctx->fill=0;
}
static void sha_update(EmuSHA256 *ctx, const void *bytes, size_t length) {
    const unsigned char *p=bytes;
    ctx->length+=(uint64_t)length;
    for (size_t i=0;i<length;i++) {
        ctx->block[ctx->fill++]=p[i];
        if(ctx->fill==64){sha_compress(ctx);ctx->fill=0;}
    }
}
static void sha_final(EmuSHA256 *ctx, char hex[65]) {
    uint64_t bits=ctx->length*8;
    ctx->block[ctx->fill++]=0x80;
    if(ctx->fill>56){while(ctx->fill<64)ctx->block[ctx->fill++]=0;sha_compress(ctx);ctx->fill=0;}
    while(ctx->fill<56)ctx->block[ctx->fill++]=0;
    for(int i=7;i>=0;i--)ctx->block[ctx->fill++]=(unsigned char)(bits>>(i*8));
    sha_compress(ctx);
    static const char digits[]="0123456789abcdef";
    for(int i=0;i<8;i++)for(int j=0;j<4;j++){
        unsigned char byte=(unsigned char)(ctx->h[i]>>(24-8*j));
        hex[(i*4+j)*2]=digits[byte>>4];hex[(i*4+j)*2+1]=digits[byte&15];
    }
    hex[64]=0;
}

/* Status: 0 idle, 1 downloading, 2 verified on disk, 3 failed, 4 cancelled. */
struct EmulatorDownload {
    SDL_Thread *thread;
    SDL_atomic_t status, percent, cancel;
    const EmulatorReleaseAsset *asset;
};
const EmulatorReleaseAsset *emulator_release_for_console(const LibraryConsole *console) {
    if (!console) return NULL;
    static const struct {const char *name,*platform;} map[] = {
       {"PlayStation 1","sony-ps1"},
       {"PlayStation 2","sony-ps2"},
       {"PlayStation 3 (experimental)","sony-ps3"},
       {"Original Xbox","microsoft-xbox"},
       {"Xbox 360 (experimental)","microsoft-xbox360"},
       {"Wii U","nintendo-wiiu"},
       {"Nintendo DS","nintendo-ds"},
       {"Nintendo 3DS","nintendo-3ds"},
       {"Nintendo Switch","nintendo-switch"},
    };
    for(size_t i=0;i<sizeof(map)/sizeof(map[0]);i++)
        if(strcmp(console->name,map[i].name)==0)
            for(size_t j=0;j<EMULATOR_RELEASE_ASSET_COUNT;j++)
                if(strcmp(EMULATOR_RELEASE_ASSETS[j].platform,map[i].platform)==0)
                    return &EMULATOR_RELEASE_ASSETS[j];
    /* RetroArch is one shared release file for the 34 systems explicitly
       designated by the project owner. Do not claim individual cores work. */
    for(size_t i=0;i<EMULATOR_RETROARCH_SYSTEM_COUNT;i++)
        if(strcmp(console->name,EMULATOR_RETROARCH_SYSTEM_NAMES[i])==0)
            return &EMULATOR_RETROARCH_ASSET;
    return NULL;
}
typedef struct { FILE *fp; EmuSHA256 sha; unsigned long long bytes, limit;
                 struct EmulatorDownload *task; } EmulatorWrite;
static size_t emulator_write(void *bytes, size_t width, size_t count, void *user) {
    EmulatorWrite *context=user;
    if(width && count>SIZE_MAX/width)return 0;
    size_t size=width*count;
    if((unsigned long long)size>context->limit-context->bytes)return 0;
    if(size && fwrite(bytes,1,size,context->fp)!=size)return 0;
    sha_update(&context->sha,bytes,size);
    context->bytes+=size;
    if(context->limit)
        SDL_AtomicSet(&context->task->percent,(int)(100*context->bytes/context->limit));
    return size;
}
static int emulator_progress(void *user,curl_off_t total,curl_off_t current,curl_off_t uploaded,curl_off_t upload_now) {
    (void)total;(void)current;(void)uploaded;(void)upload_now;
    return SDL_AtomicGet(&((struct EmulatorDownload*)user)->cancel)?1:0;
}
static int emulator_download_worker(void *userdata) {
    struct EmulatorDownload *task=userdata;
    const EmulatorReleaseAsset *asset=task->asset;
    char output[680],part[690];
    if(!asset || strncmp(asset->asset_url,
            "https://github.com/PolDrive/PolDrive-Emulators/releases/download/",65)!=0 ||
       !asset->filename[0] || strchr(asset->filename,'/') ||
       strchr(asset->filename,'\\') || strstr(asset->filename,"..") ||
       strlen(asset->sha256)!=64 || !asset->size)goto failed;
#ifdef __PS5__
    EMU_MKDIR(WOLODRIVE_DATA_PATH);
#else
    EMU_MKDIR("build");
#endif
    EMU_MKDIR(EMULATOR_STORAGE_DIR);
    snprintf(output,sizeof(output),"%s/%s",EMULATOR_STORAGE_DIR,asset->filename);
    snprintf(part,sizeof(part),"%s.part",output);
    FILE *fp=fopen(part,"wb");
    if(!fp)goto failed;
    CURL *curl=curl_easy_init();
    if(!curl){fclose(fp);remove(part);goto failed;}
    EmulatorWrite writer={.fp=fp,.bytes=0,.limit=asset->size,.task=task};
    sha_init(&writer.sha);
    curl_easy_setopt(curl,CURLOPT_URL,asset->asset_url);
    curl_easy_setopt(curl,CURLOPT_FOLLOWLOCATION,1L);
    curl_easy_setopt(curl,CURLOPT_MAXREDIRS,5L);
#if LIBCURL_VERSION_NUM >= 0x075500
    curl_easy_setopt(curl,CURLOPT_PROTOCOLS_STR,"https");
    curl_easy_setopt(curl,CURLOPT_REDIR_PROTOCOLS_STR,"https");
#else
    curl_easy_setopt(curl,CURLOPT_PROTOCOLS,CURLPROTO_HTTPS);
    curl_easy_setopt(curl,CURLOPT_REDIR_PROTOCOLS,CURLPROTO_HTTPS);
#endif
    curl_easy_setopt(curl,CURLOPT_SSL_VERIFYPEER,1L);
    curl_easy_setopt(curl,CURLOPT_SSL_VERIFYHOST,2L);
    curl_easy_setopt(curl,CURLOPT_CONNECTTIMEOUT,15L);
    curl_easy_setopt(curl,CURLOPT_LOW_SPEED_LIMIT,100L);
    curl_easy_setopt(curl,CURLOPT_LOW_SPEED_TIME,60L);
    curl_easy_setopt(curl,CURLOPT_USERAGENT,"PolDrive/2.0 emulator-release-downloader");
    curl_easy_setopt(curl,CURLOPT_WRITEFUNCTION,emulator_write);
    curl_easy_setopt(curl,CURLOPT_WRITEDATA,&writer);
    curl_easy_setopt(curl,CURLOPT_XFERINFOFUNCTION,emulator_progress);
    curl_easy_setopt(curl,CURLOPT_XFERINFODATA,task);
    curl_easy_setopt(curl,CURLOPT_NOPROGRESS,0L);
    curl_easy_setopt(curl,CURLOPT_MAXFILESIZE_LARGE,(curl_off_t)asset->size);
    CURLcode code=curl_easy_perform(curl);
    long status=0;
    curl_easy_getinfo(curl,CURLINFO_RESPONSE_CODE,&status);
    curl_easy_cleanup(curl);
    int written=fclose(fp);
    char sha[65];
    sha_final(&writer.sha,sha);
    if(SDL_AtomicGet(&task->cancel)){remove(part);SDL_AtomicSet(&task->status,4);return 0;}
    if(code!=CURLE_OK||status<200||status>=300||written!=0||
       writer.bytes!=asset->size||strcmp(sha,asset->sha256)!=0){
        remove(part);goto failed;
    }
    if(rename(part,output)!=0){remove(part);goto failed;}
    SDL_AtomicSet(&task->percent,100);
    SDL_AtomicSet(&task->status,2);
    return 0;
failed:
    SDL_AtomicSet(&task->status,3);
    return 1;
}
bool emulator_start_download(WolodriveApp *app,const EmulatorReleaseAsset *asset) {
    if(!app||!asset)return false;
    struct EmulatorDownload *task=app->emulator_download;
    if(task && SDL_AtomicGet(&task->status)==1)return false;
    if(task){
        if(task->thread)SDL_WaitThread(task->thread,NULL);
        SDL_free(task);
        app->emulator_download=NULL;
    }
    task=SDL_calloc(1,sizeof(*task));
    if(!task)return false;
    task->asset=asset;
    SDL_AtomicSet(&task->status,1);
    app->emulator_download=task;
    task->thread=SDL_CreateThread(emulator_download_worker,"pd-emulator-download",task);
    if(!task->thread){
        SDL_free(task);app->emulator_download=NULL;return false;
    }
    return true;
}
int emulator_download_status(WolodriveApp *app) {
    return app&&app->emulator_download?
           SDL_AtomicGet(&app->emulator_download->status):0;
}
int emulator_download_percent(WolodriveApp *app) {
    return app&&app->emulator_download?
           SDL_AtomicGet(&app->emulator_download->percent):0;
}
void emulator_download_shutdown(WolodriveApp *app) {
    if(!app||!app->emulator_download)return;
    struct EmulatorDownload *task=app->emulator_download;
    SDL_AtomicSet(&task->cancel,1);
    if(task->thread)SDL_WaitThread(task->thread,NULL);
    SDL_free(task);
    app->emulator_download=NULL;
}
