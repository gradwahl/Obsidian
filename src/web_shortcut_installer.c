/* Prepare Poldrive's browser shortcut and register it with the PS5 app service. */
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include "ps5_shortcut_icon.h"
#ifdef __PS5__
#include <stdint.h>
#include <unistd.h>
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
#define WOLO_INSTALL_LOG "/data/wolodrive-shortcut.log"
#endif
#define TITLE_ID "WOLO00001"
#define APP_DIR WOLO_APP_ROOT "/" TITLE_ID
#define SYS_DIR APP_DIR "/sce_sys"
#define MARKER SYS_DIR "/wolodrive-shortcut.marker"
#define REGISTERED SYS_DIR "/wolodrive-shortcut.registered"

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

static const char param_json[] =
    "{\"titleId\":\"" TITLE_ID "\",\"applicationCategoryType\":65536,"
    "\"deeplinkUri\":\"http://127.0.0.1:8084/loadpayload:/data/pldmgr/payloads/"
    "Poldrive/Poldrive.elf\","
    "\"localizedParameters\":{\"defaultLanguage\":\"en-US\","
    "\"en-US\":{\"titleName\":\"Poldrive\"}}}\n";

static int write_asset(const char *path, const void *bytes, size_t size) {
    FILE *file = fopen(path, "wb");
    if (!file) return -1;
    int failed = fwrite(bytes, 1, size, file) != size;
    if (fclose(file)) failed = 1;
    return failed ? -1 : 0;
}

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

int main(void) {
    FILE *log = fopen(WOLO_INSTALL_LOG, "w");
    if (!log) return 1;
    struct stat info;
    int exists = stat(APP_DIR, &info) == 0;
    if (exists && stat(MARKER, &info)) {
        fprintf(log, "Refusing to replace an existing unowned title " TITLE_ID "\n");
        fclose(log);
        return 1;
    }
    if (!stat(REGISTERED, &info) &&
        asset_matches(SYS_DIR "/param.json", param_json, sizeof(param_json) - 1) &&
        asset_matches(SYS_DIR "/icon0.png", shortcut_icon_png, sizeof(shortcut_icon_png))) {
        fprintf(log, "Shortcut already current; no registration required\n");
        fclose(log);
        return 0;
    }
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
        fprintf(log, "Register Poldrive (%s) under Media: 0x%x\n", TITLE_ID, (unsigned)result);
        sceAppInstUtilTerminate();
    }
    if (!result) {
        if (write_asset(REGISTERED, "v1\n", 3))
            fprintf(log, "Could not save registration marker; will retry on next launch\n");
        fprintf(log, "Shortcut registered. Payload Manager and ELF loader must be active after jailbreak.\n");
    }
    fclose(log);
    return result ? 1 : 0;
}
