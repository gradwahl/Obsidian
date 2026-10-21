/* The distributed payload installs only. The dashboard loads a separate app
 * entry point, so registration state can never accidentally launch the UI. */
#include <stdio.h>
#include <errno.h>
#include <string.h>
#ifndef WOLO_UNIFIED_TEST
#include <signal.h>
#endif
#ifndef OBSIDIAN_APP_ENTRY
#ifndef WOLO_UNIFIED_TEST
#include <arpa/inet.h>
#include <netinet/in.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#ifdef WOLO_FULLSCREEN_WEBAPP
#include "ps5_fullscreen_shortcut_helper.h"
#elif defined(WOLO_FULLSCREEN_RESTORE)
#include "ps5_fullscreen_restore_helper.h"
#else
#include "ps5_shortcut_helper.h"
#endif
#ifndef OBSIDIAN_SHORTCUT_STATUS
#define OBSIDIAN_SHORTCUT_STATUS "/data/wolodrive-shortcut.status"
#endif
#ifndef OBSIDIAN_LOADER_PORT
#define OBSIDIAN_LOADER_PORT 9021
#endif
/* Remove any previous helper reply before launching a fresh helper. Never
   treat an old status file as confirmation of the current request. */
static int obsidian_shortcut_prepare(void) {
    return remove(OBSIDIAN_SHORTCUT_STATUS) == 0 || errno == ENOENT;
}
/* 1 = helper installed now; 2 = it verified an existing installation. */
static int obsidian_shortcut_status(void) {
    FILE *file = fopen(OBSIDIAN_SHORTCUT_STATUS, "rb");
    if (!file) return 0;
    char text[32] = {0};
    int valid = fgets(text, sizeof(text), file) != NULL;
    fclose(file);
    if (!valid) return 0;
    if (!strcmp(text, "INSTALLED\n")) return 1;
    if (!strcmp(text, "CURRENT\n")) return 2;
    return 0;
}
static int obsidian_dispatch_shortcut(void) {
    int fd=socket(AF_INET,SOCK_STREAM,0);
    if(fd<0) return -1;
    struct timeval timeout={3,0};
    setsockopt(fd,SOL_SOCKET,SO_SNDTIMEO,&timeout,sizeof(timeout));
    struct sockaddr_in loader={0};
#ifdef __PS5__
    loader.sin_len=sizeof(loader);
#endif
    loader.sin_family=AF_INET; loader.sin_port=htons(OBSIDIAN_LOADER_PORT);
    loader.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
    int result=connect(fd,(struct sockaddr *)&loader,sizeof(loader));
    size_t sent=0;
    while(!result && sent<sizeof(shortcut_helper_elf)) {
        ssize_t n=send(fd,shortcut_helper_elf+sent,sizeof(shortcut_helper_elf)-sent,0);
        if(n<0 && errno==EINTR) continue;
        if(n<=0) { result=-1; break; }
        sent+=(size_t)n;
    }
    int error=errno; close(fd); errno=error;
    return result;
}
#else
extern int obsidian_dispatch_shortcut(void);
extern int obsidian_shortcut_prepare(void);
extern int obsidian_shortcut_status(void);
#endif
/* The installer runs as a separate payload. Successful helper transfer does
 * not guarantee PS5 registration, so wait for its success marker. */
static int obsidian_wait_for_shortcut(int status_prepared) {
#ifndef OBSIDIAN_SHORTCUT_WAIT_ATTEMPTS
#define OBSIDIAN_SHORTCUT_WAIT_ATTEMPTS 150
#endif
    for (int attempt=0; attempt<OBSIDIAN_SHORTCUT_WAIT_ATTEMPTS; ++attempt) {
        if (status_prepared) {
            int result = obsidian_shortcut_status();
            if (result) return result;
        }
#ifndef WOLO_UNIFIED_TEST
        usleep(100000);
#else
        /* Unit tests emulate helper completion without a 15s wait. */
        break;
#endif
    }
    return 0;
}
#endif /* installer */
extern int wolodrive_native_main(int argc,char **argv);
extern int wolodrive_web_main(void);
#ifndef OBSIDIAN_LAUNCH_LOG
#ifdef WOLO_FULLSCREEN_WEBAPP
#define OBSIDIAN_LAUNCH_LOG "/data/obsidian-fullscreen-launcher.log"
#elif defined(WOLO_FULLSCREEN_RESTORE)
#define OBSIDIAN_LAUNCH_LOG "/data/obsidian-fullscreen-restore-launcher.log"
#else
#define OBSIDIAN_LAUNCH_LOG "/data/obsidian-launcher.log"
#endif
#endif
int main(int argc,char **argv) {
#ifndef WOLO_UNIFIED_TEST
    signal(SIGPIPE,SIG_IGN);
#endif
    FILE *log=fopen(OBSIDIAN_LAUNCH_LOG,"w");
#ifndef OBSIDIAN_APP_ENTRY
    (void)argc;
    (void)argv;
    int status_prepared=obsidian_shortcut_prepare();
    if (!status_prepared) {
        if(log) { fprintf(log,"Cannot clear previous helper status; installation cancelled.\n"); fclose(log); }
        return 1;
    }
    int sent=obsidian_dispatch_shortcut();
    int installed=sent ? 0 : obsidian_wait_for_shortcut(status_prepared);
    if(log) {
        if(sent) fprintf(log,"Shortcut helper transfer failed: %s.\n",strerror(errno));
        fprintf(log, installed ?
            "Dashboard shortcut registered. Installer-only payload; open Obsidian under Media.\n" :
            "Dashboard shortcut registration unconfirmed; see /data/wolodrive-shortcut.log, then retry.\n");
        fclose(log);
    }
    return installed ? 0 : 1;
#else
    int result=wolodrive_native_main(argc,argv);
    if(log) {
        fprintf(log,"Native returned %d.%s\n",result,result ? " Starting web fallback with console-local file bridge." : " Normal exit; no browser fallback.");
        fclose(log);
    }
    return result ? wolodrive_web_main() : 0;
#endif
}
