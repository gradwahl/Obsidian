#include "../src/library.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    const LibraryCompany *sony = library_company_at(0);
    assert(sony && strcmp(sony->name, "Sony") == 0);
    assert(library_console_visible_count(0, false) == 6);
    assert(library_console_visible_count(0, true) == 4);
    static const char *visible[] = {
        "PlayStation 1", "PlayStation 2", "PlayStation 3 (experimental)",
        "PlayStation Portable (PSP)"
    };
    for (int i = 0; i < 4; i++) {
        const LibraryConsole *console = library_console_visible_at(0, i, true);
        assert(console && strcmp(console->name, visible[i]) == 0);
    }
    assert(library_console_visible_at(0, 4, true) == NULL);
    assert(library_console_visible_at(0, -1, true) == NULL);
    assert(library_console_visible_at(-1, 0, true) == NULL);
    const LibraryConsole *ps4 = library_console_visible_at(0, 3, false);
    const LibraryConsole *ps5 = library_console_visible_at(0, 4, false);
    assert(ps4 && strcmp(ps4->title_id_prefix, "CUSA") == 0);
    assert(ps5 && strcmp(ps5->title_id_prefix, "PPSA") == 0);
    /* Other brands retain their original console counts and ordering. */
    for (int company = 1; company < library_company_count(); company++) {
        const LibraryCompany *brand = library_company_at(company);
        assert(brand);
        assert(library_console_visible_count(company, true) == brand->console_count);
        for (int n = 0; n < brand->console_count; n++)
            assert(library_console_visible_at(company, n, true) ==
                   library_console_at(company, n));
    }
    puts("PASS: Emulators omits PS4/PS5; Games/Roms and navigation unchanged");
    return 0;
}
