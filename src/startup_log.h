#ifndef WOLODRIVE_STARTUP_LOG_H
#define WOLODRIVE_STARTUP_LOG_H

/* Console-only diagnostics: no UI, browser, or desktop behaviour changes. */
#ifdef __PS5__
#include <stdio.h>
#include <unistd.h>
#include <sys/time.h>

static struct timeval startup_boot_clock;

static void startup_log_init(void) {
    gettimeofday(&startup_boot_clock, NULL);
    FILE *log = fopen("/data/wolodrive-startup.log", "w");
    if (log) {
        fflush(stdout);
        fflush(stderr);
        if (dup2(fileno(log), STDERR_FILENO) < 0)
            perror("[OBSIDIAN STARTUP] redirect stderr");
        if (dup2(fileno(log), STDOUT_FILENO) < 0)
            perror("[OBSIDIAN STARTUP] redirect stdout");
        fclose(log);
        setvbuf(stdout, NULL, _IONBF, 0);
        setvbuf(stderr, NULL, _IONBF, 0);
    } else {
        perror("[OBSIDIAN STARTUP] open /data/wolodrive-startup.log");
    }
}

static void startup_log_stage(const char *stage) {
    struct timeval now;
    gettimeofday(&now, NULL);
    long long ms = (long long)(now.tv_sec - startup_boot_clock.tv_sec) * 1000 +
                   (now.tv_usec - startup_boot_clock.tv_usec) / 1000;
    fprintf(stderr, "[OBSIDIAN STARTUP] %s | +%lld ms since native entry\n", stage, ms);
    fflush(stderr);
}
#else
#define startup_log_init() ((void)0)
#define startup_log_stage(stage) ((void)0)
#endif

#endif
