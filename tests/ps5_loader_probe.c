/* A minimal SDK/runtime probe, deliberately independent of SDL and app assets. */
#include <stdio.h>

int main(void) {
    FILE *log = fopen("/data/wolodrive-loader-probe.log", "w");
    if (!log) {
        perror("wolodrive loader probe: open log");
        return 1;
    }
    fputs("Wolodrive loader probe v1 reached main.\n"
          "SDK C runtime started and /data is writable.\n"
          "This does not test SDL, app imports, video or firmware support.\n", log);
    return fclose(log) ? 1 : 0;
}
