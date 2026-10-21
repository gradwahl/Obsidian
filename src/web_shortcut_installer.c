/* Prepare PolDrive's browser shortcut and register it with the PS5 app service. */
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "ps5_shortcut_icon.h"
#ifdef WOLO_SINGLE_SHORTCUT
#include "ps5_install_contract.h"
#endif
#if (defined(WOLO_FULLSCREEN_WEBAPP) || defined(WOLO_FULLSCREEN_RESTORE)) && !defined(WOLO_SINGLE_SHORTCUT)
#error "Fullscreen web-app experiment requires the single installer service"
#endif
#if defined(WOLO_FULLSCREEN_WEBAPP) && defined(WOLO_FULLSCREEN_RESTORE)
#error "Choose test or restore, not both"
#endif
#ifdef WOLO_EMBED_APP
#include "ps5_app_payload.h"
#endif
#ifndef POLDRIVE_APP_PATH
#define POLDRIVE_APP_PATH "/data/pldmgr/payloads/poldrive/poldrive-app.elf"
#define POLDRIVE_CREATE_APP_DIRS
#endif
#ifdef __PS5__
#include <stdint.h>
#include <ps5/kernel.h>
#endif

extern int sceAppInstUtilInitialize(void);
extern int sceAppInstUtilTerminate(void);
extern int sceAppInstUtilAppInstallAll(void *params);
#ifndef __PS5__
extern int sceAppInstUtilAppInstallTitleDir(const char *title, const char *root, void *params);
#endif


#ifndef WOLO_APP_ROOT
#define WOLO_APP_ROOT "/user/app"
#endif
#ifndef WOLO_INSTALL_LOG
#ifdef WOLO_FULLSCREEN_WEBAPP
#define WOLO_INSTALL_LOG "/data/poldrive-fullscreen-shortcut.log"
#elif defined(WOLO_FULLSCREEN_RESTORE)
#define WOLO_INSTALL_LOG "/data/poldrive-fullscreen-restore.log"
#else
#define WOLO_INSTALL_LOG "/data/wolodrive-shortcut.log"
#endif
#endif
#define TITLE_ID "WOLO00001"
#define APP_DIR WOLO_APP_ROOT "/" TITLE_ID
#define SYS_DIR APP_DIR "/sce_sys"
#define MARKER SYS_DIR "/wolodrive-shortcut.marker"
#define REGISTERED SYS_DIR "/wolodrive-shortcut.registered"
/* Shared between the independent shortcut helper and unified launcher.
   Unlike /user/app, /data is also where both payloads already write logs. */
#ifndef SHORTCUT_STATUS
#define SHORTCUT_STATUS "/data/wolodrive-shortcut.status"
#endif

static int register_shortcut(FILE *log) {
#ifdef __PS5__
    uint32_t handle = 0;
    int found = kernel_dynlib_handle(getpid(), "libSceAppInstUtil.sprx", &handle);
    fprintf(log, "TitleDir module lookup: %d, handle 0x%x\n", found, handle);
    int (*install_title)(const char *, const char *, void *) = NULL;
    if (!found) install_title = (void *)kernel_dynlib_resolve(getpid(), handle, "Wudg3Xe3heE");
    fprintf(log, "TitleDir entry point: %s\n", install_title ? "available" : "unavailable");
    fflush(log);
    if (install_title) return install_title(TITLE_ID, WOLO_APP_ROOT "/", NULL);
    /* Same firmware fallback as Payload Manager: scan prepared app metadata.
       Only this title's files are written; no existing title is uninstalled. */
    fprintf(log, "Using app-service registration scan (TitleDir unavailable)\n");
    fflush(log);
    return sceAppInstUtilAppInstallAll(NULL);
#else
    (void)log;
    return sceAppInstUtilAppInstallTitleDir(TITLE_ID, WOLO_APP_ROOT "/", NULL);
#endif
}

#ifdef WOLO_SINGLE_SHORTCUT
/* The single-file installer verifies the quiet service before sending us. */
#define SHORTCUT_TARGET POLDRIVE_DIRECT_URL
#define SHORTCUT_LOG_TARGET "Single installer Media target: " POLDRIVE_DIRECT_URL
#elif defined(WOLO_SHORTCUT_WEBFIRST)
/* Browser-only Media tile. This deliberately does not start the payload.
   Install only AFTER the fixed-port background server is configured. */
#define SHORTCUT_TARGET "http://127.0.0.1:8097/"
#define SHORTCUT_LOG_TARGET "Web-first Media target: http://127.0.0.1:8097/"
#else
/* Recovery/legacy route remains available in the ordinary unified installer. */
#define SHORTCUT_TARGET "http://127.0.0.1:8084/loadpayload:" POLDRIVE_APP_PATH
#define SHORTCUT_LOG_TARGET "Media launch target: " POLDRIVE_APP_PATH
#endif

static const char param_json[] =
#ifdef WOLO_FULLSCREEN_WEBAPP
    /* Experimental app-mode registration, following SVT Play / TV4 Play.
       Plain local HTTP and borderless behavior require actual PS5 testing. */
    "{\"titleId\":\"" TITLE_ID "\",\"applicationCategoryType\":66048,"
    "\"downloadDataSize\":256,\"webAppUri\":\"" SHORTCUT_TARGET "\","
#else
    "{\"titleId\":\"" TITLE_ID "\",\"applicationCategoryType\":65536,"
    "\"deeplinkUri\":\"" SHORTCUT_TARGET "\","
#endif
    "\"localizedParameters\":{\"defaultLanguage\":\"en-US\","
    "\"en-US\":{\"titleName\":\"PolDrive\"}}}\n";

static int write_asset(const char *path, const void *bytes, size_t size) {
    FILE *file = fopen(path, "wb");
    if (!file) return -1;
    int failed = fwrite(bytes, 1, size, file) != size;
    if (fclose(file)) failed = 1;
    return failed ? -1 : 0;
}

#if defined(WOLO_FULLSCREEN_WEBAPP) || defined(WOLO_FULLSCREEN_RESTORE)
#include "ps5_webapp_layout.h"
#endif

static int publish_shortcut_status(const char *status) {
    char temporary[128];
    int n = snprintf(temporary, sizeof(temporary), SHORTCUT_STATUS ".tmp.%ld", (long)getpid());
    if (n < 0 || (size_t)n >= sizeof(temporary)) return -1;
    FILE *file = fopen(temporary, "wb");
    if (!file) return -1;
    size_t length = strlen(status);
    int failed = fwrite(status, 1, length, file) != length;
    if (fclose(file)) failed = 1;
    if (!failed && rename(temporary, SHORTCUT_STATUS) == 0) return 0;
    remove(temporary);
    return -1;
}

#if !defined(WOLO_EMBED_APP) && !defined(WOLO_SINGLE_SHORTCUT)
static int asset_matches(const char *path, const void *data, size_t size) {
    FILE *file = fopen(path, "rb");
    if (!file) return 0;
    const unsigned char *bytes = data;
    int equal = 1;
    for (size_t i = 0; i < size; i++) {
        if (fgetc(file) != bytes[i]) { equal = 0; break; }
    }
    if (equal && fgetc(file) != EOF) equal = 0;
    fclose(file);
    return equal;
}

#endif

int main(void) {
    FILE *log = fopen(WOLO_INSTALL_LOG, "w");
    if (!log) return 1;
    fprintf(log, SHORTCUT_LOG_TARGET "\n");
#ifdef WOLO_FULLSCREEN_WEBAPP
    fprintf(log, "EXPERIMENT v2: system launch scaffold + webAppUri, category 66048, downloadDataSize 256. Fullscreen/local HTTP not device-verified.\n");
#endif
    struct stat info;
    int exists = stat(APP_DIR, &info) == 0;
    if (exists && stat(MARKER, &info)) {
        fprintf(log, "Refusing to replace an existing unowned title " TITLE_ID "\n");
        fclose(log);
        return 1;
    }
#ifdef WOLO_FULLSCREEN_WEBAPP
    if (webapp_prepare(log)) { fclose(log); return 1; }
#elif defined(WOLO_FULLSCREEN_RESTORE)
    if (webapp_restore(log)) { fclose(log); return 1; }
#endif
#ifdef WOLO_EMBED_APP
    /* Install the exact dashboard runtime before publishing its shortcut.
       Keep it under Payload Manager's allowed payload root for /loadpayload:. */
#ifdef POLDRIVE_CREATE_APP_DIRS
    const char *parents[] = {"/data/pldmgr", "/data/pldmgr/payloads",
                             "/data/pldmgr/payloads/poldrive"};
    for (size_t i = 0; i < sizeof(parents)/sizeof(parents[0]); ++i) {
        if (mkdir(parents[i], 0755) && errno != EEXIST) {
            fprintf(log, "Cannot prepare dashboard app directory: %s\n", strerror(errno));
            fclose(log);
            return 1;
        }
    }
#endif
    const char *temporary = POLDRIVE_APP_PATH ".tmp";
    if (write_asset(temporary, app_payload_elf, sizeof(app_payload_elf)) ||
        chmod(temporary, 0755) || rename(temporary, POLDRIVE_APP_PATH)) {
        fprintf(log, "Cannot save dashboard app: %s\n", strerror(errno));
        remove(temporary);
        fclose(log);
        return 1;
    }
    /* A file marker cannot establish that the dashboard still has the tile.
       An explicit installer invocation always retries app-service registration. */
#else
#ifndef WOLO_SINGLE_SHORTCUT
    if (!stat(REGISTERED, &info) &&
        asset_matches(SYS_DIR "/param.json", param_json, sizeof(param_json) - 1) &&
        asset_matches(SYS_DIR "/icon0.png", shortcut_icon_png, sizeof(shortcut_icon_png))) {
        fprintf(log, "Shortcut already current; no registration required\n");
        if (publish_shortcut_status("CURRENT\n")) {
            fprintf(log, "Unable to publish shortcut confirmation to /data: %s\n", strerror(errno));
            fclose(log);
            return 1;
        }
        fprintf(log, "Shortcut status: CURRENT\n");
        fclose(log);
        return 0;
    }
#endif
#endif
    /* A changed shortcut must be registered successfully before it is current. */
    if (remove(REGISTERED) && errno != ENOENT) {
        fprintf(log, "Cannot invalidate old registration marker: %s\n", strerror(errno));
        fclose(log);
        return 1;
    }
    if ((!exists && mkdir(APP_DIR, 0755)) || (mkdir(SYS_DIR, 0755) && errno != EEXIST) ||
        write_asset(MARKER, "wolodrive-shortcut-v1\n", sizeof("wolodrive-shortcut-v1\n") - 1) ||
        write_asset(SYS_DIR "/param.json", param_json, sizeof(param_json) - 1) ||
        write_asset(SYS_DIR "/icon0.png", shortcut_icon_png, sizeof(shortcut_icon_png))) {
        fprintf(log, "Shortcut file preparation failed: %s\n", strerror(errno));
        fclose(log);
        return 1;
    }
    int result = sceAppInstUtilInitialize();
    fprintf(log, "AppInst initialize: 0x%x\n", (unsigned)result);
    if (!result) {
        result = register_shortcut(log);
        fprintf(log, "Register PolDrive (%s) under Media: 0x%x\n", TITLE_ID, (unsigned)result);
        sceAppInstUtilTerminate();
    }
    if (!result) {
        if (write_asset(REGISTERED, "v1\n", 3)) {
            fprintf(log, "Could not save registration marker; will retry on next launch\n");
            result = 1;
        } else if (publish_shortcut_status("INSTALLED\n")) {
            fprintf(log, "Could not publish shortcut confirmation: %s\n", strerror(errno));
            result = 1;
        } else {
            fprintf(log, "Shortcut status: INSTALLED\n");
            fprintf(log, "Shortcut registered. Payload Manager and ELF loader must be active after jailbreak.\n");
        }
    }
    fclose(log);
    return result ? 1 : 0;
}
