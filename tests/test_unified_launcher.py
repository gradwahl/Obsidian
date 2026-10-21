"""Exercise launch ordering, fallback and helper embedding without a PS5."""
import importlib.util
import os
import http.client
from pathlib import Path
import subprocess
import tempfile
import time
import unittest
from urllib.parse import urlsplit

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('shortcut_embed', ROOT/'scripts/embed_shortcut_helper.py')
embedding = importlib.util.module_from_spec(spec)
spec.loader.exec_module(embedding)


class UnifiedLauncher(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory(prefix='obsidian-unified-')
        cls.folder = Path(cls.temp.name)
        cls.log = cls.folder/'launcher.log'
        driver = cls.folder/'driver.c'
        driver.write_text(r'''
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
static int stage;
int obsidian_shortcut_registered(void) {
    const char *existing=getenv("ALREADY_REGISTERED");
    const char *completed=getenv("REGISTER_AFTER_DISPATCH");
    return (existing && *existing) ||
        (stage>=1 && completed && *completed && !getenv("FAIL_SHORTCUT"));
}
int obsidian_shortcut_prepare(void) {
    return getenv("STATUS_PREPARE_FAILED") ? 0 : 1;
}
int obsidian_shortcut_status(void) {
    if (getenv("FAIL_SHORTCUT")) return 0;
    if (getenv("HELPER_ALREADY_CURRENT")) return 2;
    if (stage>=1 && getenv("HELPER_CURRENT_AFTER_DISPATCH")) return 2;
    if (stage>=1 && getenv("HELPER_JUST_INSTALLED")) return 1;
    return 0;
}
int obsidian_dispatch_shortcut(void) {
    assert(stage==0); stage=1; puts("SHORTCUT"); errno=ECONNREFUSED;
    return getenv("FAIL_SHORTCUT") ? -1 : 0;
}
int wolodrive_native_main(int argc,char **argv) {
    assert((stage==0 || stage==1) && argc==2 && argv[1]); stage=2; puts("NATIVE");
    return getenv("FAIL_NATIVE") ? 1 : 0;
}
int wolodrive_web_main(void) {
    assert(stage==2); stage=3; puts("WEB");
    return getenv("FAIL_WEB") ? 7 : 0;
}
''')
        cls.app_executable = cls.folder/'app'
        cls.executable = cls.folder/'launcher'
        subprocess.run(['gcc', '-Wall', '-Wextra', '-Werror', '-DWOLO_UNIFIED_TEST',
                        f'-DOBSIDIAN_LAUNCH_LOG="{cls.log}"', str(ROOT/'src/ps5_unified_launcher.c'),
                        str(driver), '-o', str(cls.executable)], check=True)
        subprocess.run(['gcc', '-Wall', '-Wextra', '-Werror', '-DWOLO_UNIFIED_TEST',
                        '-DOBSIDIAN_APP_ENTRY', f'-DOBSIDIAN_LAUNCH_LOG="{cls.log}"',
                        str(ROOT/'src/ps5_unified_launcher.c'), str(driver),
                        '-o', str(cls.app_executable)], check=True)

    @classmethod
    def tearDownClass(cls):
        cls.temp.cleanup()

    def launch(self, already_installed=True, app=False, **environment):
        env = {k:v for k,v in os.environ.items() if k not in
               ['FAIL_SHORTCUT', 'FAIL_NATIVE', 'FAIL_WEB', 'ALREADY_REGISTERED',
                'REGISTER_AFTER_DISPATCH', 'HELPER_ALREADY_CURRENT',
                'HELPER_JUST_INSTALLED', 'HELPER_CURRENT_AFTER_DISPATCH',
                'STATUS_PREPARE_FAILED']}
        if already_installed:
            env['ALREADY_REGISTERED']='1'
        return subprocess.run([str(self.app_executable if app else self.executable), 'test'], env={**env, **environment},
                              capture_output=True, text=True)

    def test_installer_never_opens_ui_in_any_registration_state(self):
        for state in ({'HELPER_JUST_INSTALLED':'1'},
                      {'HELPER_CURRENT_AFTER_DISPATCH':'1'},
                      {'HELPER_ALREADY_CURRENT':'1'}):
            for existing in (False, True):
                with self.subTest(state=state, existing=existing):
                    result=self.launch(already_installed=existing, **state)
                    self.assertEqual(result.returncode, 0)
                    self.assertEqual(result.stdout.splitlines(), ['SHORTCUT'])
                    self.assertIn('Installer-only payload', self.log.read_text())

    def test_old_marker_does_not_count_as_dashboard_registration(self):
        result=self.launch(REGISTER_AFTER_DISPATCH='1')
        self.assertEqual(result.returncode, 1)
        self.assertEqual(result.stdout.splitlines(), ['SHORTCUT'])
        self.assertIn('registration unconfirmed', self.log.read_text())

    def test_uncleared_status_cancels_before_dispatch(self):
        result=self.launch(STATUS_PREPARE_FAILED='1', HELPER_ALREADY_CURRENT='1')
        self.assertEqual(result.returncode, 1)
        self.assertEqual(result.stdout, '')

    def test_failed_dispatch_never_opens_ui(self):
        result=self.launch(FAIL_SHORTCUT='1')
        self.assertEqual(result.returncode, 1)
        self.assertEqual(result.stdout.splitlines(), ['SHORTCUT'])
        self.assertIn('transfer failed', self.log.read_text())

    def test_dashboard_opens_native_without_installer(self):
        result=self.launch(app=True, already_installed=False)
        self.assertEqual(result.returncode, 0)
        self.assertEqual(result.stdout.splitlines(), ['NATIVE'])
        self.assertIn('Normal exit; no browser fallback', self.log.read_text())

    def test_dashboard_native_failure_runs_web_fallback(self):
        result=self.launch(app=True, FAIL_NATIVE='1')
        self.assertEqual(result.returncode, 0)
        self.assertEqual(result.stdout.splitlines(), ['NATIVE', 'WEB'])
        self.assertEqual(self.launch(app=True, FAIL_NATIVE='1', FAIL_WEB='1').returncode, 7)

    def test_embeds_exact_helper_bytes_and_refuses_non_elf(self):
        source=self.folder/'helper.elf'; target=self.folder/'helper.h'
        payload=b'\x7fELF'+bytes(range(256))
        source.write_bytes(payload); embedding.embed(source,target)
        body=target.read_text().split('{',1)[1].split('}',1)[0]
        self.assertEqual(bytes(int(value) for value in body.split(',') if value.strip()),payload)
        source.write_bytes(b'not ELF')
        with self.assertRaises(ValueError): embedding.embed(source,target)

    def test_default_build_and_shortcut_alias_are_unified(self):
        make=(ROOT/'Makefile').read_text()
        self.assertIn('ps5-shortcut: ps5',make)
        self.assertIn('-o build/obsidian.elf',make)
        self.assertNotIn('cp build/wolodrive-web.elf build/obsidian.elf',make)
        self.assertIn('src/ps5_unified_launcher.c build/obsidian-native-main.o build/obsidian-web-fallback.o',make)
        self.assertIn('-DOBSIDIAN_APP_ENTRY',make)
        self.assertIn('build/ps5_app_payload.h app_payload_elf',make)
        self.assertIn('-DWOLO_EMBED_APP',make)
        self.assertIn('src/web_shortcut_installer.c -lSceIpmi -lSceAppInstUtil -o build/.internal/obsidian-shortcut-helper.elf',make)
        default_recipe=make.split('ps5: ps5-web-assets', 1)[1].split('ps5-native:', 1)[0]
        self.assertIn('src/ps5_single_installer.c', default_recipe)
        self.assertIn('-DWOLO_WEB_NO_BROWSER', default_recipe)
        self.assertIn('-DWOLO_SINGLE_SHORTCUT', default_recipe)
        self.assertIn('embed_ps5_service.py', default_recipe)
        self.assertNotIn('wolodrive_native_main', default_recipe)
        fullscreen_recipe=make.split('ps5-fullscreen-test: ps5-web-assets ps5-fullscreen-restore',1)[1].split('ps5-native:',1)[0]
        self.assertIn('-DWOLO_FULLSCREEN_WEBAPP', fullscreen_recipe)
        self.assertIn('-o build/obsidian-fullscreen-v2.elf', fullscreen_recipe)
        self.assertNotIn('-o build/obsidian.elf', fullscreen_recipe)
        self.assertNotIn('build/ps5_shortcut_helper.h', fullscreen_recipe)

    def test_fallback_serves_root_and_preserves_notice_on_reuse(self):
        with tempfile.TemporaryDirectory(prefix='obsidian-fallback-') as temporary:
            folder=Path(temporary); preview=folder/'preview'; preview.mkdir()
            (preview/'index.html').write_text('<html>fallback fixture</html>')
            subprocess.run(['python3', str(ROOT/'scripts/embed_web_preview.py'), str(preview),
                            str(folder/'ps5_web_assets.h')],check=True,capture_output=True)
            stub=folder/'browser.c'
            stub.write_text('#include <stdio.h>\nint sceSystemServiceLaunchWebBrowser(const char *url) { '
                            'FILE *f=fopen("opened.url","w"); if(!f)return 1; fputs(url,f); fclose(f); return 0; }\n')
            binary=folder/'web'
            subprocess.run(['gcc','-D_GNU_SOURCE','-DWOLO_NATIVE_FALLBACK','-DWOLO_WEB_TEST_BROWSER',
                            '-DWOLO_WEB_PORT=0','-Wall','-Wextra','-Werror',f'-I{folder}',
                            str(ROOT/'src/web_payload.c'),str(stub),'-pthread','-lz','-o',str(binary)],check=True)
            process=subprocess.Popen([str(binary)],cwd=folder,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
            try:
                opened=folder/'opened.url'
                deadline=time.monotonic()+5
                while not opened.exists() and time.monotonic()<deadline: time.sleep(.02)
                url=opened.read_text(); self.assertTrue(url.endswith('/#native-unavailable'))
                parsed=urlsplit(url)
                connection=http.client.HTTPConnection(parsed.hostname,parsed.port,timeout=2)
                connection.request('GET','/'); response=connection.getresponse()
                self.assertEqual(response.status,200)
                self.assertIn(b'fallback fixture',response.read()); connection.close()
                opened.unlink()
                again=subprocess.run([str(binary)],cwd=folder,capture_output=True,timeout=5)
                self.assertEqual(again.returncode,0)
                self.assertEqual(opened.read_text(),url)
            finally:
                process.terminate(); process.wait(timeout=5)


if __name__ == '__main__':
    unittest.main()
