/* Runs in a separate payload process: browser IPC must not suspend the server. */
#include <stdio.h>
#include <string.h>

/* Patched inside the embedded ELF before it is sent to the console's loader. */
char wolodrive_browser_url[96] = "WOLODRIVE_BROWSER_URL_PATCH_V1";
extern int sceUserServiceInitialize(void *params);
extern int sceUserServiceTerminate(void);
extern int sceSystemServiceLaunchWebBrowser(const char *uri, void *params);
#ifdef WOLO_BUNDLED_SHORTCUT
extern int wolodrive_install_shortcut(void);
#endif

int main(void) {
    if (strncmp(wolodrive_browser_url, "http://127.0.0.1:", 17) != 0) return 1;
#ifdef WOLO_BUNDLED_SHORTCUT
    /* Keep app-service dependencies isolated from the HTTP server. */
    wolodrive_install_shortcut();
#endif
    FILE *log = fopen("/data/wolodrive-browser.log", "w");
    int initialized = sceUserServiceInitialize(NULL);
    if (log) {
        fprintf(log, "[OBSIDIAN BROWSER] UserService initialize returned 0x%x\n", (unsigned)initialized);
        fprintf(log, "[OBSIDIAN BROWSER] Requesting %s\n", wolodrive_browser_url);
        fclose(log);
    }
    int result = sceSystemServiceLaunchWebBrowser(wolodrive_browser_url, NULL);
    log = fopen("/data/wolodrive-browser.log", "a");
    if (log) {
        fprintf(log, "[OBSIDIAN BROWSER] Browser API returned 0x%x\n", (unsigned)result);
        fclose(log);
    }
    if (!initialized) sceUserServiceTerminate();
    return result ? 1 : 0;
}
