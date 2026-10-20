"""Guard the PS5 SDK browser ABI and helper URL embedding without platform calls."""
import importlib.util
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("helper_embedding", ROOT / "scripts/embed_browser_helper.py")
embedding = importlib.util.module_from_spec(spec)
spec.loader.exec_module(embedding)


class BrowserHelperTest(unittest.TestCase):
    def test_unique_fixed_width_url_slot(self):
        with tempfile.TemporaryDirectory() as directory:
            elf = Path(directory) / "helper.elf"
            header = Path(directory) / "helper.h"
            slot = embedding.MARKER.ljust(embedding.PATCH_SIZE, b"\0")
            elf.write_bytes(b"prefix" + slot + b"suffix")
            embedding.embed_helper(elf, header)
            self.assertIn("#define BROWSER_URL_OFFSET 6", header.read_text())
            for bad in (slot + slot, embedding.MARKER + b"bad padding", b"no marker"):
                elf.write_bytes(bad)
                with self.assertRaises(ValueError):
                    embedding.embed_helper(elf, header)

    def test_sdk_user_service_lifecycle_and_null_browser_parameter(self):
        with tempfile.TemporaryDirectory() as directory:
            directory = Path(directory)
            hooks = directory / "hooks.h"
            hooks.write_text('#include <stdio.h>\nFILE *helper_test_fopen(const char *, const char *);\n'
                             '#define fopen helper_test_fopen\n')
            driver = directory / "driver.c"
            driver.write_text(r'''
#include <assert.h>
#include <stdio.h>
#include <string.h>
extern char wolodrive_browser_url[96];
int browser_helper_main(void);
static int stage;
int wolodrive_install_shortcut(void) { assert(stage == 0); stage = -1; return 0; }
FILE *helper_test_fopen(const char *p, const char *m) { (void)p; (void)m; return NULL; }
int sceUserServiceInitialize(void *params) { assert(params == NULL && stage == -1); stage = 1; return 0; }
int sceSystemServiceLaunchWebBrowser(const char *url, void *params) {
    assert(stage == 1 && params == NULL);
    assert(strcmp(url, "http://127.0.0.1:12345/") == 0); stage = 2; return 0;
}
int sceUserServiceTerminate(void) { assert(stage == 2); stage = 3; return 0; }
int main(void) {
    assert(browser_helper_main() == 1 && stage == 0);
    strcpy(wolodrive_browser_url, "http://127.0.0.1:12345/");
    assert(browser_helper_main() == 0 && stage == 3);
    return 0;
}
''')
            obj = directory / "helper.o"
            executable = directory / "test-helper"
            subprocess.run(["gcc", "-Wall", "-Wextra", "-Werror", "-include", str(hooks),
                            "-DWOLO_BUNDLED_SHORTCUT", "-Dmain=browser_helper_main", "-c", str(ROOT / "src/web_browser_helper.c"),
                            "-o", str(obj)], check=True)
            subprocess.run(["gcc", "-Wall", "-Wextra", "-Werror", str(driver), str(obj),
                            "-o", str(executable)], check=True)
            subprocess.run([str(executable)], check=True)


if __name__ == "__main__":
    unittest.main()
