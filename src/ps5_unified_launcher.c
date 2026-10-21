/* One distributable: Media registration, native UI, web startup fallback.
 * App-service dependencies stay in an isolated helper, as on the tested web
 * launcher, rather than becoming dependencies of the SDL/HTTP process. */
#include <stdio.h>
#include <errno.h>
#include <string.h>
#ifndef WOLO_UNIFIED_TEST
#include <arpa/inet.h>
#include <netinet/in.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#include "ps5_shortcut_helper.h"
#define POLDRIVE_REGISTERED_MARKER "/user/app/WOLO00001/sce_sys/wolodrive-shortcut.registered"
static int poldrive_shortcut_registered(void) {
    return access(POLDRIVE_REGISTERED_MARKER, F_OK) == 0;
}
static int poldrive_dispatch_shortcut(void) {
    int fd=socket(AF_INET,SOCK_STREAM,0);
    if(fd<0) return -1;
    struct timeval timeout={3,0};
    setsockopt(fd,SOL_SOCKET,SO_SNDTIMEO,&timeout,sizeof(timeout));
    struct sockaddr_in loader={0};
#ifdef __PS5__
    loader.sin_len=sizeof(loader);
#endif
    loader.sin_family=AF_INET; loader.sin_port=htons(9021);
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
extern int poldrive_dispatch_shortcut(void);
extern int poldrive_shortcut_registered(void);
#endif
/* The installer runs as a separate payload. Successful helper transfer does
 * not guarantee PS5 registration, so wait for its success marker. */
static int poldrive_wait_for_shortcut(void) {
    for (int attempt=0; attempt<150; ++attempt) {
        if (poldrive_shortcut_registered()) return 1;
#ifndef WOLO_UNIFIED_TEST
        usleep(100000);
#else
        /* Unit tests emulate helper completion without a 15s wait. */
        break;
#endif
    }
    return 0;
}
extern int wolodrive_native_main(int argc,char **argv);
extern int wolodrive_web_main(void);
#ifndef POLDRIVE_LAUNCH_LOG
#define POLDRIVE_LAUNCH_LOG "/data/poldrive-launcher.log"
#endif
int main(int argc,char **argv) {
#ifndef WOLO_UNIFIED_TEST
    signal(SIGPIPE,SIG_IGN);
#endif
    FILE *log=fopen(POLDRIVE_LAUNCH_LOG,"w");
    /* A single ELF: first run registers the Media tile and exits; later runs
     * (including launches from the Media tile) open the application UI. */
    int already_installed=poldrive_shortcut_registered();
    int sent=poldrive_dispatch_shortcut();
    int dispatch_error=errno;
    if(log) {
        fprintf(log,"PolDrive unified launcher\n");
        if(sent) fprintf(log,"Shortcut helper transfer failed: %s.\n",strerror(dispatch_error));
        else fprintf(log,"Shortcut helper sent; registration result is in /data/wolodrive-shortcut.log.\n");
        fflush(log);
    }
    if(!already_installed) {
        int installed=!sent && poldrive_wait_for_shortcut();
        if(log) {
            fprintf(log,installed ?
                "Dashboard shortcut registered. Installer-only first launch; open PolDrive under Media.\n" :
                "Dashboard shortcut registration unconfirmed; see /data/wolodrive-shortcut.log, then retry.\n");
            fclose(log);
        }
        return installed ? 0 : 1;
    }
    if(log) {
        fprintf(log,"Shortcut already installed; starting native UI.\n"); fflush(log);
    }
    int result=wolodrive_native_main(argc,argv);
    if(log) {
        fprintf(log,"Native returned %d.%s\n",result,result ? " Starting web fallback with console-local file bridge." : " Normal exit; no browser fallback.");
        fclose(log);
    }
    return result ? wolodrive_web_main() : 0;
}
