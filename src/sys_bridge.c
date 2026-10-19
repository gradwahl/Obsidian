#include "wolodrive.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <sys/statvfs.h>
#include <sys/stat.h>

#ifdef __PS5__
typedef struct {
    int type;
    int req_id;
    int priority;
    int msg_id;
    int target_id;
    int user_id;
    int unk1;
    int unk2;
    int app_id;
    int error_num;
    int unk3;
    char use_icon_image_uri;
    char message[1024];
    char icon_uri[1024];
    char unk4[1024];
} OrbisNotificationRequest;

extern int sceKernelSendNotificationRequest(int device, OrbisNotificationRequest *req, size_t size, int blocking);
extern int sceKernelGetCpuTemperature(int *temp);
extern int sceKernelGetSocSensorTemperature(int sensor_id, int *temp);
#endif

const char *sys_format_to_string(WolodriveApp *app, PackageFormat fmt) {
    switch (fmt) {
        case FMT_EXFAT: return ".exfat (ShadowMountPlus)";
        case FMT_FFPKG: return TR(app, TXT_FORMAT_FOLDER);
        case FMT_PKG:   return ".pkg (PS4 FPKG)";
        case FMT_CATALOG: return "Catalog metadata (website only)";
        default:        return ".bin";
    }
}

const char *sys_format_short_badge(PackageFormat fmt) {
    switch (fmt) {
        case FMT_EXFAT: return "EXFAT";
        case FMT_FFPKG: return "FFPKG";
        case FMT_PKG:   return "FPKG";
        case FMT_CATALOG: return "WEB";
        default:        return "PKG";
    }
}

void sys_send_notification(WolodriveApp *app, const char *fmt, ...) {
    char buffer[512];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    printf("[WOLODRIVE NOTIFY] %s\n", buffer);

    if (app) {
        snprintf(app->toast_message, sizeof(app->toast_message), "%s", buffer);
        app->toast_expire_tick = SDL_GetTicks() + 4200;
    }

#ifdef __PS5__
    OrbisNotificationRequest req;
    memset(&req, 0, sizeof(req));
    req.target_id = -1;
    req.use_icon_image_uri = 1;
    snprintf(req.message, sizeof(req.message), "[Wolodrive] %s", buffer);
    snprintf(req.icon_uri, sizeof(req.icon_uri), "cxml://psnotification/tex_icon_system");
    sceKernelSendNotificationRequest(0, &req, sizeof(req), 0);
#endif
}

static void query_mount_space(const char *path, float *free_gb, float *total_gb, float fallback_free, float fallback_total) {
    struct statvfs vfs;
    if (statvfs(path, &vfs) == 0 && vfs.f_blocks > 0) {
        double total = (double)vfs.f_blocks * (double)vfs.f_frsize / (1024.0 * 1024.0 * 1024.0);
        double avail = (double)vfs.f_bavail * (double)vfs.f_frsize / (1024.0 * 1024.0 * 1024.0);
        *total_gb = (float)total;
        *free_gb  = (float)avail;
    } else {
        *free_gb  = fallback_free;
        *total_gb = fallback_total;
    }
}

void sys_update_stats(SystemStats *stats) {
#ifdef __PS5__
    query_mount_space("/data", &stats->data_free_gb, &stats->data_total_gb, 412.4f, 667.2f);
    query_mount_space("/mnt/ext0", &stats->ext0_free_gb, &stats->ext0_total_gb, 1280.5f, 2000.0f);

    int cpu_t = 58, gpu_t = 62;
    if (sceKernelGetCpuTemperature(&cpu_t) == 0) stats->cpu_temp_c = cpu_t;
    if (sceKernelGetSocSensorTemperature(0, &gpu_t) == 0) stats->gpu_temp_c = gpu_t;
    stats->network_online = true;
    snprintf(stats->ip_address, sizeof(stats->ip_address), "PS5 13.60 ONLINE");
#else
    query_mount_space("/", &stats->data_free_gb, &stats->data_total_gb, 245.8f, 512.0f);
    stats->ext0_free_gb  = 1420.6f;
    stats->ext0_total_gb = 2000.0f;

    FILE *f = fopen("/sys/class/thermal/thermal_zone0/temp", "r");
    if (f) {
        int millideg = 0;
        if (fscanf(f, "%d", &millideg) == 1 && millideg > 0) {
            stats->cpu_temp_c = millideg / 1000;
            stats->gpu_temp_c = (millideg / 1000) + 3;
        }
        fclose(f);
    } else {
        stats->cpu_temp_c = 54;
        stats->gpu_temp_c = 58;
    }
    stats->network_online = true;
    snprintf(stats->ip_address, sizeof(stats->ip_address), "LINUX MINT x64");
#endif
}

static void add_seed_entry(WolodriveApp *app, const char *id, const char *title,
                           TextId description_id, const char *path, const char *cover_url,
                           float size_gb, PackageFormat fmt,
                           uint8_t r, uint8_t g, uint8_t b) {
    if (app->catalog_count >= MAX_CATALOG_ITEMS) return;
    GameItem *it = &app->catalog[app->catalog_count++];
    memset(it, 0, sizeof(GameItem));

    snprintf(it->id_code, sizeof(it->id_code), "%s", id);
    snprintf(it->title, sizeof(it->title), "%s", title);
    snprintf(it->version, sizeof(it->version), "01.00");
    snprintf(it->source_name, sizeof(it->source_name), "Pegasus-DL / Wolodrive");
    it->description_id = description_id;
    snprintf(it->install_path, sizeof(it->install_path), "%s", path);
    snprintf(it->cover_url, sizeof(it->cover_url), "%s", cover_url ? cover_url : "");

    it->size_gb  = size_gb;
    it->format   = fmt;
    it->state    = STATE_AVAILABLE;
    it->is_game  = true;
    it->progress = 0.0f;
    it->cover_r  = r;
    it->cover_g  = g;
    it->cover_b  = b;

}

void sys_init(WolodriveApp *app) {
    app->catalog_count = 0;
#ifdef __PS5__
    mkdir(WOLODRIVE_DATA_PATH, 0755);
    snprintf(app->language_path, sizeof(app->language_path), "%s/language", WOLODRIVE_DATA_PATH);
    snprintf(app->settings_path, sizeof(app->settings_path), "%s/filters", WOLODRIVE_DATA_PATH);
#else
    char *pref_path = SDL_GetPrefPath("Wolodrive", "Wolodrive");
    if (pref_path) {
        snprintf(app->language_path, sizeof(app->language_path), "%slanguage", pref_path);
        snprintf(app->settings_path, sizeof(app->settings_path), "%sfilters", pref_path);
        SDL_free(pref_path);
    }
#endif
    SDL_AtomicSet(&app->language, i18n_load_language(app->language_path, i18n_detect_language()));
    settings_load(app);
    app->sync_status_id = TXT_READY;
    if (app->window) SDL_SetWindowTitle(app->window, TR(app, TXT_WINDOW_TITLE));

    /* Bazowe pozycje z prawdziwymi okładkami CDN (automatycznie pobierane do assets/covers/ i łączone z default-sources.json) */
    add_seed_entry(app, "PPSA08338", "Marvel's Spider-Man 2",
        TXT_DESC_SPIDER_MAN_2,
        "/data/homebrew/PPSA08338/PPSA08338.exfat",
        "https://image.api.playstation.com/vulcan/ap/rnd/202306/1219/1c7b75d8ed9271516546560d219ad0b22ee0a263b4537bd8.png",
        86.4f, FMT_EXFAT, 185, 20, 32);

    add_seed_entry(app, "PPSA04610", "God of War Ragnarok",
        TXT_DESC_GOD_OF_WAR,
        "/data/homebrew/PPSA04610/PPSA04610.exfat",
        "https://image.api.playstation.com/vulcan/ap/rnd/202207/1210/4xJ8XB3bi888QTLZYdl7Oi0s.png",
        104.2f, FMT_EXFAT, 26, 82, 148);

    add_seed_entry(app, "PPSA01341", "Demon's Souls Remake",
        TXT_DESC_DEMONS_SOULS,
        "/data/homebrew/PPSA01341",
        "https://image.api.playstation.com/vulcan/img/rnd/202011/1717/GemRaOZaCMhGxQ9dRhnQQyT5.png",
        53.8f, FMT_FFPKG, 110, 78, 38);

    add_seed_entry(app, "PPSA07950", "Stellar Blade",
        TXT_DESC_STELLAR_BLADE,
        "/data/homebrew/PPSA07950/PPSA07950.exfat",
        "https://image.api.playstation.com/vulcan/ap/rnd/202401/2211/40e7cfd126a11fe5118310ebce6d9b3a23e7cab2717217c8.png",
        34.7f, FMT_EXFAT, 22, 118, 132);

    add_seed_entry(app, "PPSA01552", "Ratchet & Clank: Rift Apart",
        TXT_DESC_RATCHET_AND_CLANK,
        "/data/homebrew/PPSA01552",
        "https://image.api.playstation.com/vulcan/ap/rnd/202101/2921/DwVjpbKOsFOyPdNzmSTSWuxG.png",
        33.6f, FMT_FFPKG, 136, 42, 165);

    add_seed_entry(app, "PPSA10665", "Astro Bot",
        TXT_DESC_ASTRO_BOT,
        "/mnt/ext0/homebrew/PPSA10665.exfat",
        "https://image.api.playstation.com/vulcan/ap/rnd/202405/2409/dc90a668a7effb6fb9b2cf55fb9cd6f7c26856c504ef1e54.png",
        37.9f, FMT_EXFAT, 32, 96, 205);

    add_seed_entry(app, "CUSA30410", "Bloodborne GOTY (60 FPS Patch)",
        TXT_DESC_BLOODBORNE,
        "/data/pkg/CUSA30410_bloodborne_60fps.pkg",
        "https://image.api.playstation.com/cdn/UP9000/CUSA00900_00/b5Qrdv032c226K9f0n37P4v2s0F17H4E.png",
        36.2f, FMT_PKG, 82, 28, 46);

    add_seed_entry(app, "CUSA03041", "Red Dead Redemption 2",
        TXT_DESC_RED_DEAD_REDEMPTION,
        "/mnt/ext0/pkg/CUSA03041_rdr2.pkg",
        "https://image.api.playstation.com/cdn/UP1004/CUSA03041_00/Hpl5MtwQgOVF9vJqlfui6SDB5Jl4oBSq.png",
        105.0f, FMT_PKG, 168, 62, 24);

    add_seed_entry(app, "CUSA18017", "Ghost of Tsushima Director's Cut",
        TXT_DESC_GHOST_OF_TSUSHIMA,
        "/data/pkg/CUSA18017_got.pkg",
        "https://image.api.playstation.com/vulcan/ap/rnd/202106/2322/c16gs6a7lbAYzPf7ZTikbH1c.png",
        59.1f, FMT_PKG, 112, 108, 42);

    add_seed_entry(app, "CUSA00900", "Driveclub Complete Edition",
        TXT_DESC_DRIVECLUB,
        "/data/pkg/CUSA00900_driveclub.pkg",
        "https://image.api.playstation.com/cdn/EP9000/CUSA00003_00/4b3c01e6b1d4525a1fb42ef213b6dcac867527f32bc8cf43.png",
        31.4f, FMT_PKG, 152, 28, 78);

    sys_update_stats(&app->stats);
    sys_send_notification(app, TR(app, TXT_STARTED), WOLODRIVE_NAME, WOLODRIVE_VERSION);
}

void sys_trigger_action(WolodriveApp *app, GameItem *item) {
    if (!item) return;

    /* Catalog metadata and elapsed time are not evidence of installation.
       Until a real backend exists, actions must not simulate success. */
    sys_send_notification(app, TR(app, TXT_ACTION_UNAVAILABLE));
}

void sys_cancel_action(WolodriveApp *app, GameItem *item) {
    if (!item) return;

    SDL_LockMutex(app->catalog_mutex);
    if (item->state == STATE_DOWNLOADING) {
        item->state = STATE_AVAILABLE;
        item->progress = 0.0f;
        app->catalog_dirty = true;
        SDL_UnlockMutex(app->catalog_mutex);
        sys_send_notification(app, TR(app, TXT_CANCELLED), item->title);
    } else {
        SDL_UnlockMutex(app->catalog_mutex);
        app->active_row = 0;
        app->rows[0].selected_col = 0;
        sys_send_notification(app, TR(app, TXT_RETURNED));
    }
}

void sys_set_language(WolodriveApp *app, Language language) {
    if (language < LANG_EN || language >= LANG_COUNT) return;
    SDL_AtomicSet(&app->language, language);
    app->show_original_description = false;
    app->description_scroll = 0;
    if (app->window) SDL_SetWindowTitle(app->window, TR(app, TXT_WINDOW_TITLE));
    bool saved = i18n_save_language(app->language_path, language);
    ui_rebuild_carousels(app);
    sys_send_notification(app, TR(app, saved ? TXT_LANGUAGE_CHANGED : TXT_LANGUAGE_UNSAVED),
                          i18n_language_name(language));
}
