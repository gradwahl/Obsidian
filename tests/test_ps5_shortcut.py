"""Test shortcut metadata, title ownership and registration error handling."""
from pathlib import Path
import json
import os
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class ShortcutTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="wolodrive-shortcut-")
        self.directory = Path(self.temp.name)
        self.app_root = self.directory / "app"
        self.app_root.mkdir()
        self.log = self.directory / "install.log"
        (self.directory / "ps5").mkdir()
        (self.directory / "ps5/kernel.h").write_text(
            '#include <stdint.h>\n#include <sys/types.h>\n'
            'int kernel_dynlib_handle(pid_t, const char *, uint32_t *);\n'
            'intptr_t kernel_dynlib_resolve(pid_t, uint32_t, const char *);\n')
        subprocess.run([sys.executable, str(ROOT / "scripts/generate_ps5_shortcut_icon.py"),
                        str(self.directory / "icon.png"), str(self.directory / "ps5_shortcut_icon.h")], check=True)
        stub = self.directory / "stub.c"
        stub.write_text(r'''
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/types.h>
int sceAppInstUtilInitialize(void) { puts("INITIALIZE"); return 0; }
int sceAppInstUtilTerminate(void) { puts("TERMINATE"); return 0; }
int sceAppInstUtilAppInstallTitleDir(const char *title, const char *root, void *params) {
    assert(strcmp(title, "WOLO00001") == 0 && root[0] == '/' && params == NULL);
    puts("REGISTER"); return getenv("WOLO_FAIL_REGISTER") ? -5 : 0;
}
int kernel_dynlib_handle(pid_t pid, const char *name, uint32_t *handle) {
    assert(pid > 0 && strcmp(name, "libSceAppInstUtil.sprx") == 0);
    *handle = 19; return 0;
}
intptr_t kernel_dynlib_resolve(pid_t pid, uint32_t handle, const char *nid) {
    assert(pid > 0 && handle == 19 && strcmp(nid, "Wudg3Xe3heE") == 0);
    return getenv("WOLO_NO_TITLE_DIR") ? 0 : (intptr_t)sceAppInstUtilAppInstallTitleDir;
}
int sceAppInstUtilAppInstallAll(void *params) {
    assert(params == NULL); puts("REGISTRATION_SCAN");
    return getenv("WOLO_FAIL_REGISTER") ? -5 : 0;
}
''')
        self.executable = self.directory / "installer"
        subprocess.run(["gcc", "-D__PS5__", "-Wall", "-Wextra", "-Werror", "-I", str(self.directory),
                        f'-DWOLO_APP_ROOT="{self.app_root}"', f'-DWOLO_INSTALL_LOG="{self.log}"',
                        str(ROOT / "src/web_shortcut_installer.c"), str(stub), "-o", str(self.executable)], check=True)

    def tearDown(self):
        self.temp.cleanup()

    def run_installer(self, **env):
        return subprocess.run([str(self.executable)], capture_output=True,
                              env={**os.environ, **env}, timeout=5)

    def test_install_and_owned_update(self):
        for attempt in range(2):
            result = self.run_installer()
            self.assertEqual(result.returncode, 0)
            if attempt == 0:
                self.assertIn(b"REGISTER", result.stdout)
            else:
                self.assertNotIn(b"INITIALIZE", result.stdout)
                self.assertIn("already current", self.log.read_text())
        system = self.app_root / "WOLO00001/sce_sys"
        param = json.loads((system / "param.json").read_text())
        self.assertEqual(param["titleId"], "WOLO00001")
        self.assertEqual(param["applicationCategoryType"], 65536)
        self.assertEqual(param["localizedParameters"]["en-US"]["titleName"], "PolDrive")
        self.assertTrue(param["deeplinkUri"].endswith("/Poldrive/Poldrive.elf"))
        self.assertEqual((system / "icon0.png").read_bytes(), (self.directory / "icon.png").read_bytes())

    def test_refuses_unowned_existing_title(self):
        system = self.app_root / "WOLO00001/sce_sys"
        system.mkdir(parents=True)
        original = b"existing unrelated title"
        (system / "param.json").write_bytes(original)
        result = self.run_installer()
        self.assertEqual(result.returncode, 1)
        self.assertNotIn(b"INITIALIZE", result.stdout)
        self.assertEqual((system / "param.json").read_bytes(), original)

    def test_changed_metadata_retries_registration_after_failure(self):
        self.assertEqual(self.run_installer().returncode, 0)
        system = self.app_root / "WOLO00001/sce_sys"
        (system / "param.json").write_text('{}')
        self.assertEqual(self.run_installer(WOLO_FAIL_REGISTER="1").returncode, 1)
        self.assertFalse((system / "wolodrive-shortcut.registered").exists())
        retry = self.run_installer()
        self.assertEqual(retry.returncode, 0)
        self.assertIn(b"REGISTER", retry.stdout)

    def test_registration_failure_is_not_success(self):
        result = self.run_installer(WOLO_FAIL_REGISTER="1")
        self.assertEqual(result.returncode, 1)
        self.assertIn(b"TERMINATE", result.stdout)
        self.assertNotIn("Shortcut registered", self.log.read_text())

    def test_missing_private_api_uses_registration_scan(self):
        result = self.run_installer(WOLO_NO_TITLE_DIR="1")
        self.assertEqual(result.returncode, 0)
        self.assertIn(b"REGISTRATION_SCAN", result.stdout)
        self.assertNotIn(b"REGISTER\n", result.stdout)
        self.assertIn(b"TERMINATE", result.stdout)
        (self.app_root / "WOLO00001/sce_sys/wolodrive-shortcut.registered").unlink()
        result = self.run_installer(WOLO_NO_TITLE_DIR="1", WOLO_FAIL_REGISTER="1")
        self.assertEqual(result.returncode, 1)
        self.assertNotIn("Shortcut registered", self.log.read_text())


if __name__ == "__main__":
    unittest.main()
