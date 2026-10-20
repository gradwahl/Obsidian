#ifndef WOLODRIVE_H
#define WOLODRIVE_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_image.h>
#include <stdbool.h>
#include <stdint.h>
#include "i18n.h"
#include "translation.h"
#include "library.h"

#define TR(app, id) i18n_text((Language)SDL_AtomicGet(&(app)->language), (id))

#define WOLODRIVE_NAME          "Poldrive"
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
#define HERO_HEIGHT             552   /* Ponad 50% wysokości ekranu (1080p) */
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

#define MAX_CATALOG_ITEMS       26000 /* Full filtered Axekin index plus other sources. */
#define MAX_ITEMS_PER_ROW       128
typedef enum {
    VIEW_HOME,
    VIEW_GAMES,
    VIEW_EMULATORS,
    VIEW_FILES,
    VIEW_PAYLOADS,
    VIEW_DOWNLOADS,
    VIEW_SEARCH,
    VIEW_SETTINGS,
    VIEW_COUNT
} SidebarView;
#define TOTAL_ROWS              VIEW_COUNT

typedef enum {
    HOME_CAT_PS5 = 0,
    HOME_CAT_PS4 = 1,
    HOME_CAT_POPULAR = 2,
    HOME_CAT_NEWEST = 3,
    HOME_CAT_TOP_RATED = 4,
    HOME_CAT_COUNT = 5
} HomeCategory;

#define MAX_DESCRIPTION_BYTES   4096
#define MAX_GAME_SCREENSHOTS    6

typedef enum {
    FMT_EXFAT = 0,
    FMT_FFPKG = 1,
    FMT_PKG   = 2,
    FMT_CATALOG = 3  /* Website metadata only; not an installable package. */
} PackageFormat;

typedef enum {
    STATE_AVAILABLE   = 0,
    STATE_DOWNLOADING = 1,
    STATE_INSTALLED   = 2
} PackageState;

typedef struct {
    char id_code[32];           /* np. PPSA08338 lub CUSA30410 */
    char platform[48];          /* Optional metadata-only platform identifier. */
    char title[128];            /* Tytuł gry */
    char version[32];           /* Wersja np. 01.00 */
    char source_name[64];       /* Nazwa katalogu źródłowego Pegasus-DL */
    bool is_game;              /* Explicit category=game; utilities/DLC stay off Home. */
    bool is_dlc;
    char parent_title_id[32];
    char screenshot_urls[MAX_GAME_SCREENSHOTS][512];
    int screenshot_count;
    char launchbox_url[512], playstation_url[512];
    uint64_t catalog_added_at;  /* Validated UTC YYYYMMDDhhmmss, zero means unknown. */
    char release_date[24];      /* Data premiery YYYY-MM-DD (sortowanie "Najnowsze") */
    int download_count;         /* Ilość pobrań (sortowanie "Najpopularniejsze") */
    float rating_score;         /* Ocena IMDb/Metacritic/IGDB 0.0f - 10.0f ("Najlepiej oceniane") */
    TextId description_id;      /* TEXT_COUNT means source-provided description. */
    char description[MAX_DESCRIPTION_BYTES]; /* Original plain-text catalog description. */
    char install_path[256];     /* Docelowa ścieżka na PS5 */
    char download_url[512];     /* Bezpośredni link do pakietu z katalogu JSON */
    char boxart_url[512];      /* Complete portrait front cover, including platform strip. */
    char cover_url[512];       /* Portrait cover/icon fallback. */
    char cover_fallback_url[512]; /* Independent provider if supplied art fails. */
    char icon_url[512];        /* Square game icon. */
    char landscape_url[512];   /* Wide card artwork, kept separate from the cover. */
    char backdrop_url[512];    /* Wide hero/background artwork. */

    float size_gb;              /* Rozmiar w GB */
    PackageFormat format;       /* .exfat / .ffpkg / .pkg */
    PackageState state;         /* Dostępny / W trakcie pobierania / Zainstalowany */
    float progress;             /* 0.0f - 100.0f */

    uint8_t cover_r;            /* Kolor akcentu tła i placeholdera */
    uint8_t cover_g;
    uint8_t cover_b;

} GameItem;

typedef struct {
    char title[64];
    GameItem *items[MAX_ITEMS_PER_ROW];
    int item_count;
    int selected_col;
    float scroll_x;
    float target_scroll_x;
    float scroll_velocity;
} CarouselRow;

#define MAX_LIBRARY_ARTWORK 128
typedef struct {
    const char *path;
    SDL_Texture *texture;
    int width, height;
    SDL_Rect source_bounds;
} LibraryArtwork;

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
    bool sources_limited; /* false includes current and future sources. */
    int source_count;
    char sources[MAX_CATALOG_ITEMS][64];
    unsigned disabled_formats; /* PackageFormat bits; zero includes all. */
} CatalogFilters;

typedef struct {
    SDL_atomic_t language;
    char language_path[512];
    char settings_path[512];
    CatalogFilters filters;
    int settings_selection;
    int settings_column;
    int settings_scroll;
    bool language_menu_open;
    int language_menu_index;
    struct ScreenshotService *screenshots;
    bool screenshot_viewer;
    struct ArtworkCache *artwork;
    SDL_Thread *catalog_thread;
    TranslationService *translations;
    GameItem *description_item;
    bool show_original_description;
    bool description_hovered;
    int description_scroll;
    SDL_Window *window;
    SDL_Renderer *renderer;
    bool vsync_enabled;
    struct UiTextCache *text_cache;
    TTF_Font *font_logo;
    TTF_Font *font_hero_title;
    TTF_Font *font_row_title;
    TTF_Font *font_body;
    TTF_Font *font_small;
    TTF_Font *font_micro;

    SDL_mutex *catalog_mutex;
    GameItem catalog[MAX_CATALOG_ITEMS];
    int catalog_count;
    char catalog_source_names[40][64]; /* Include sources with empty/offline indexes. */
    int catalog_source_count;
    bool additional_catalog_loaded; /* Avoid re-reading immutable local snapshots. */
    bool axekin_catalog_loaded; /* Prevent duplicate bulk imports on refresh. */
    bool catalog_dirty;         /* Sygnał od wątku tła do przebudowania karuzel */
    bool sync_in_progress;      /* Czy trwa synchronizacja z Pegasus-DL */
    TextId sync_status_id;
    int sync_current, sync_total, sync_sources;
    char sync_title[128];

    CarouselRow rows[TOTAL_ROWS];
    CarouselRow home_categories[HOME_CAT_COUNT];
    int home_cat_index;
    int last_synced_home_col;
    bool home_grid_expanded;
    float home_grid_anim;
    int home_grid_scroll_row;
    float home_vertical_scroll;
    int active_row;
    LibraryDepth library_depth;
    int library_company_selection;
    int library_company;
    int library_console_selection;
    int library_game_offset; /* 128-title pages within a Games/Roms console. */
    bool sidebar_focus;
    bool sidebar_hovered;
    float sidebar_expand;
    float sidebar_width;
    float sidebar_dim_alpha;
    GameItem *hero_current_item;
    GameItem *hero_prev_item;
    float hero_crossfade;
    float card_focus_scale;
    char search_query[128];
    GameItem *search_results[MAX_CATALOG_ITEMS];
    int search_count, search_selected, search_key;
    bool search_results_focus;
    bool detail_open, detail_content_focus;
    GameItem *detail_item, *detail_parent;
    int detail_tab, detail_selection, detail_scroll;
    LibraryArtwork library_artwork[MAX_LIBRARY_ARTWORK];
    int library_artwork_count;
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
int sys_import_native_snapshot(WolodriveApp *app);
bool settings_accepts_item(const WolodriveApp *app, const GameItem *item);
bool settings_source_enabled(const WolodriveApp *app, const char *source);
bool settings_filters_active(const WolodriveApp *app);
bool settings_load(WolodriveApp *app);
bool settings_save(const WolodriveApp *app);
const char *sys_format_to_string(WolodriveApp *app, PackageFormat fmt);
const char *sys_format_short_badge(PackageFormat fmt);

/* Warstwa katalogu Pegasus-DL i cache'u okładek (src/catalog_pegasus.c) */
void catalog_start_async_sync(WolodriveApp *app);
void catalog_stop_sync(WolodriveApp *app);
void catalog_fetch_ratings_curl(WolodriveApp *app);
/* Caller holds catalog_mutex; returns newest dated games up to capacity. */
int catalog_latest_games_locked(WolodriveApp *app, GameItem **items, int capacity);

/* Warstwa interfejsu Premium Netflix/PS5 UI (src/ui_netflix.c) */
bool ui_init(WolodriveApp *app);
bool ui_create_renderer(WolodriveApp *app);
void ui_cleanup(WolodriveApp *app);
void ui_rebuild_carousels(WolodriveApp *app);
void ui_handle_input(WolodriveApp *app, const SDL_Event *ev);
void ui_update_animations(WolodriveApp *app, float dt);
void ui_render(WolodriveApp *app);

#endif /* WOLODRIVE_H */
