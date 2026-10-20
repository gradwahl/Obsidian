#ifndef WOLODRIVE_STARTUP_LOG_H
#define WOLODRIVE_STARTUP_LOG_H

/* Console-only diagnostics: no UI, browser, or desktop behaviour changes. */
#ifdef __PS5__
#include <stdio.h>
#include <unistd.h>

static void startup_log_init(void) {
    FILE *log = fopen("/data/wolodrive-startup.log", "w");
    if (log) {
        fflush(stdout);
        fflush(stderr);
        if (dup2(fileno(log), STDERR_FILENO) < 0)
            perror("[POLDRIVE STARTUP] redirect stderr");
        if (dup2(fileno(log), STDOUT_FILENO) < 0)
            perror("[POLDRIVE STARTUP] redirect stdout");
        fclose(log);
        setvbuf(stdout, NULL, _IONBF, 0);
        setvbuf(stderr, NULL, _IONBF, 0);
    } else {
        perror("[POLDRIVE STARTUP] open /data/wolodrive-startup.log");
    }
}

static void startup_log_stage(const char *stage) {
    fprintf(stderr, "[POLDRIVE STARTUP] %s\n", stage);
    fflush(stderr);
}
#else
#define startup_log_init() ((void)0)
#define startup_log_stage(stage) ((void)0)
#endif

#endif
