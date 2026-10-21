#ifndef OBSIDIAN_EMULATOR_DOWNLOAD_H
#define OBSIDIAN_EMULATOR_DOWNLOAD_H
#include "wolodrive.h"
#include "emulator_releases.h"

const EmulatorReleaseAsset *emulator_release_for_console(const LibraryConsole *console);
/* Downloads a verified GitHub asset into the app's emulators folder, but
 * never executes, extracts, or installs it. Returns false if busy/unavailable. */
bool emulator_start_download(WolodriveApp *app, const EmulatorReleaseAsset *asset);
int emulator_download_status(WolodriveApp *app);
int emulator_download_percent(WolodriveApp *app);
void emulator_download_shutdown(WolodriveApp *app);

#endif
