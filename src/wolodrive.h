#ifndef WOLODRIVE_H
#define WOLODRIVE_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_image.h>
#include <stdbool.h>
#include <stdint.h>
#include "i18n.h"

#define TR(app, id) i18n_text((Language)SDL_AtomicGet(&(app)->language), (id))

#define WOLODRIVE_NAME          "WOLODRIVE"
#define WOLODRIVE_VERSION       "2.0.0-PRO (PS5 13.60)"
#define WOLODRIVE_DATA_PATH     "/data/homebrew/wolodrive"
#define SHADOWMOUNT_PATH        "/data/homebrew"

/* Stały adres katalogu źródeł Pegasus-DL */
#define PEGASUS_DEFAULT_SOURCES_URL \
    "https://raw.githubusercontent.com/pegasus-ps5/pegasus-dl/refs/heads/main/catalogs/default-sources.json"

#ifdef __PS5__
#define COVERS_CACHE_DIR        "/data/homebrew/wolodrive/covers"
#else
#define COVERS_CACHE_DIR        "assets/covers"
#endif

#define SCREEN_WIDTH            1920
#define SCREEN_HEIGHT           1080
#define HERO_HEIGHT             486   /* Górne 45% ekranu (1080p) */
#define TOPBAR_HEIGHT           72

/* Paleta kolorów Premium Netflix / PS5 OS */
#define COLOR_BG_TOP_R          20    /* #141414 */
#define COLOR_BG_TOP_G          20
#define COLOR_BG_TOP_B          22

#define COLOR_BG_BOT_R          8     /* #080808 */
#define COLOR_BG_BOT_G          8
#define COLOR_BG_BOT_B          10

#define COLOR_ACCENT_R          229   /* #E50914 (Netflix Red) */
#define COLOR_ACCENT_G          9
#define COLOR_ACCENT_B          20

#define COLOR_TEXT_R            255   /* #FFFFFF */
#define COLOR_TEXT_G            255
#define COLOR_TEXT_B            255

#define COLOR_MUTED_R           148   /* #94949C */
#define COLOR_MUTED_G           148
#define COLOR_MUTED_B           156

#define MAX_CATALOG_ITEMS       256
#define MAX_ITEMS_PER_ROW       128
#define TOTAL_ROWS              4

typedef enum {
    FMT_EXFAT = 0,
    FMT_FFPKG = 1,
    FMT_PKG   = 2
} PackageFormat;

typedef enum {
    STATE_AVAILABLE   = 0,
    STATE_DOWNLOADING = 1,
    STATE_INSTALLED   = 2
} PackageState;

typedef struct {
    char id_code[32];           /* np. PPSA08338 lub CUSA30410 */
    char title[128];            /* Tytuł gry */
    char version[32];           /* Wersja np. 01.00 */
    char source_name[64];       /* Nazwa katalogu źródłowego Pegasus-DL */
    TextId description_id;      /* TEXT_COUNT means source-provided description. */
    char description[384];      /* Opis w Hero Bannerze */
    char install_path[256];     /* Docelowa ścieżka na PS5 */
    char download_url[512];     /* Bezpośredni link do pakietu z katalogu JSON */
    char cover_url[512];        /* URL okładki (posterUrl / coverUrl / iconUrl) */
    char cover_local_path[256]; /* Lokalna ścieżka w assets/covers/ */

    float size_gb;              /* Rozmiar w GB */
    PackageFormat format;       /* .exfat / .ffpkg / .pkg */
    PackageState state;         /* Dostępny / W trakcie pobierania / Zainstalowany */
    float progress;             /* 0.0f - 100.0f */

    uint8_t cover_r;            /* Kolor akcentu tła i placeholdera */
    uint8_t cover_g;
    uint8_t cover_b;

    bool cover_ready_on_disk;   /* Flaga ustawiana przez wątek pobierający po zapisie pliku */
    bool cover_load_attempted;  /* Czy wątek UI próbował już wczytać teksturę */
    SDL_Texture *cover_tex;     /* Tekstura okładki w VRAM (SDL2_image) */
    int cover_w;
    int cover_h;
} GameItem;

typedef struct {
    char title[64];
    GameItem *items[MAX_ITEMS_PER_ROW];
    int item_count;
    int selected_col;
    float scroll_x;
    float target_scroll_x;
} CarouselRow;

typedef struct {
    float data_free_gb;
    float data_total_gb;
    float ext0_free_gb;
    float ext0_total_gb;
    int cpu_temp_c;
    int gpu_temp_c;
    bool network_online;
    char ip_address[32];
} SystemStats;

typedef struct {
    SDL_atomic_t language;
    char language_path[512];
    bool language_menu_open;
    int language_menu_index;
    SDL_Window *window;
    SDL_Renderer *renderer;
    TTF_Font *font_logo;
    TTF_Font *font_hero_title;
    TTF_Font *font_row_title;
    TTF_Font *font_body;
    TTF_Font *font_small;
    TTF_Font *font_micro;

    SDL_mutex *catalog_mutex;
    GameItem catalog[MAX_CATALOG_ITEMS];
    int catalog_count;
    bool catalog_dirty;         /* Sygnał od wątku tła do przebudowania karuzel */
    bool sync_in_progress;      /* Czy trwa synchronizacja z Pegasus-DL */
    TextId sync_status_id;
    int sync_current, sync_total, sync_sources;
    char sync_title[128];
    int covers_cached_count;

    CarouselRow rows[TOTAL_ROWS];
    int active_row;
    float vertical_scroll_y;
    float target_vertical_scroll_y;
    float focus_pulse;          /* Animacja pulsowania ramki i zoomu aktywnego kafelka */

    SystemStats stats;
    uint32_t last_stats_update;

    char toast_message[512];
    uint32_t toast_expire_tick;
    bool running;
} WolodriveApp;

/* Warstwa systemowa (src/sys_bridge.c) */
void sys_init(WolodriveApp *app);
void sys_set_language(WolodriveApp *app, Language language);
void sys_send_notification(WolodriveApp *app, const char *fmt, ...);
void sys_update_stats(SystemStats *stats);
void sys_trigger_action(WolodriveApp *app, GameItem *item);
void sys_cancel_action(WolodriveApp *app, GameItem *item);
const char *sys_format_to_string(PackageFormat fmt);
const char *sys_format_short_badge(PackageFormat fmt);

/* Warstwa katalogu Pegasus-DL i cache'u okładek (src/catalog_pegasus.c) */
void catalog_start_async_sync(WolodriveApp *app);
void catalog_free_textures(WolodriveApp *app);

/* Warstwa interfejsu Premium Netflix/PS5 UI (src/ui_netflix.c) */
bool ui_init(WolodriveApp *app);
bool ui_create_renderer(WolodriveApp *app);
void ui_cleanup(WolodriveApp *app);
void ui_rebuild_carousels(WolodriveApp *app);
void ui_handle_input(WolodriveApp *app, const SDL_Event *ev);
void ui_update_animations(WolodriveApp *app, float dt);
void ui_render(WolodriveApp *app);

#endif /* WOLODRIVE_H */
