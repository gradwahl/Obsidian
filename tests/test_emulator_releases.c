/* Offline SHA-256 and release matching regression tests.
   No GitHub binaries are fetched and no emulator is executed. */
#include <stdio.h>
#include <string.h>
#include "../src/emulator_download.c"

static int test_sha(const char *payload, const char *expected) {
    EmuSHA256 context;
    char actual[65];
    sha_init(&context);
    size_t len = strlen(payload);
    sha_update(&context, payload, len / 2);
    sha_update(&context, payload + len / 2, len - len / 2);
    sha_final(&context, actual);
    if (strcmp(actual, expected) != 0) {
        fprintf(stderr, "SHA-256 mismatch: %s != %s\n", actual, expected);
        return 1;
    }
    return 0;
}
int main(void) {
    if (test_sha("", "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"))
        return 1;
    if (test_sha("abc", "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"))
        return 1;
    const LibraryConsole ps2 = {.name="PlayStation 2"};
    const LibraryConsole n64 = {.name="Nintendo 64"};
    const EmulatorReleaseAsset *match=emulator_release_for_console(&ps2);
    if(!match || strcmp(match->filename,"PS5SX2Installer.elf")!=0)return 2;
    const EmulatorReleaseAsset *retro = emulator_release_for_console(&n64);
    if(retro != &EMULATOR_RETROARCH_ASSET ||
       strcmp(retro->filename, "PS5_RetroArch-v4.0.0-beta-4.zip") != 0)return 3;
    if(EMULATOR_RELEASE_ASSET_COUNT!=9 || EMULATOR_RETROARCH_SYSTEM_COUNT!=34)return 4;
    for(size_t i=0;i<EMULATOR_RETROARCH_SYSTEM_COUNT;i++){
        const LibraryConsole system={.name=EMULATOR_RETROARCH_SYSTEM_NAMES[i]};
        if(emulator_release_for_console(&system) != &EMULATOR_RETROARCH_ASSET)return 5;
    }
    const LibraryConsole missing={.name="Panasonic 3DO"};
    if(emulator_release_for_console(&missing)!=NULL)return 6;
    puts("PASS: SHA-256 official vectors, 9 standalone and 34 RetroArch mappings");
    return 0;
}
