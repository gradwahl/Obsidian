"""Prove mount flag preservation without mounting or changing any filesystem."""
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class WebAppMount(unittest.TestCase):
    def test_readonly_scope_preserves_security_flags_and_reports_failures(self):
        with tempfile.TemporaryDirectory(prefix="poldrive-mount-") as temporary:
            folder = Path(temporary)
            (folder / "sys").mkdir()
            (folder / "sys/mount.h").write_text('''
#define MNT_RDONLY 1
#define MNT_NOEXEC 4
#define MNT_NOSUID 8
#define MNT_UPDATE 0x10000
#define MNT_UPDATEMASK (MNT_NOEXEC | MNT_NOSUID)
struct statfs { unsigned long f_flags; char f_mntfromname[64], f_mntonname[64], f_fstypename[64]; };
struct iovec;
int statfs(const char *, struct statfs *);
int nmount(struct iovec *, unsigned int, int);
''')
            driver = folder / "mount.c"
            driver.write_text(r'''
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define TITLE_ID "WOLO00001"
static const char param_json[]="{}";
static int write_asset(const char *p, const void *d, size_t s) { (void)p; (void)d; (void)s; return 0; }
#include "ps5_webapp_layout.h"
static const char *mode;
static int calls;
int statfs(const char *path, struct statfs *info) {
    assert(!strcmp(path,"/system_ex"));
    memset(info,0,sizeof(*info));
    info->f_flags=MNT_NOEXEC|MNT_NOSUID|(!strcmp(mode,"rw")?0:MNT_RDONLY);
    strcpy(info->f_mntfromname, !strcmp(mode,"bad-device")?"/dev/unrelated":"/dev/ssd0.system_ex");
    strcpy(info->f_mntonname,"/system_ex"); strcpy(info->f_fstypename,"exfatfs");
    return !strcmp(mode,"fail-stat")?-1:0;
}
int nmount(struct iovec *iov, unsigned int count, int flags) {
    assert(count==6);
    assert(!strcmp(iov[0].iov_base,"from") && !strcmp(iov[1].iov_base,"/dev/ssd0.system_ex"));
    assert(!strcmp(iov[2].iov_base,"fspath") && !strcmp(iov[3].iov_base,"/system_ex"));
    assert(!strcmp(iov[4].iov_base,"fstype") && !strcmp(iov[5].iov_base,"exfatfs"));
    ++calls;
    int expected=MNT_UPDATE|MNT_NOEXEC|MNT_NOSUID|(calls==2?MNT_RDONLY:0);
    assert(flags==expected);
    return (!strcmp(mode,"fail-open") && calls==1) || (!strcmp(mode,"fail-close") && calls==2)?-1:0;
}
int main(int argc,char **argv) {
    assert(argc==2); mode=argv[1];
    int begin=webapp_begin_write(stderr);
    if (!strcmp(mode,"fail-stat") || !strcmp(mode,"bad-device")) { assert(begin==-1 && calls==0); return 0; }
    if (!strcmp(mode,"fail-open")) { assert(begin==-1 && calls==1); return 0; }
    assert(begin==0);
    int end=webapp_end_write(stderr);
    if (!strcmp(mode,"fail-close")) assert(end==-1 && calls==2);
    else assert(end==0 && calls==(!strcmp(mode,"rw")?0:2));
    return 0;
}
''')
            binary = folder / "mount-test"
            subprocess.run(["gcc", "-D__PS5__", "-DWOLO_FULLSCREEN_WEBAPP", "-Wall", "-Wextra",
                            "-Werror", "-Wno-unused-function", "-I", str(folder), "-I", str(ROOT / "src"),
                            str(driver), "-o", str(binary)], check=True)
            for mode in ("ro", "rw", "fail-open", "fail-close", "bad-device", "fail-stat"):
                with self.subTest(mode=mode):
                    subprocess.run([str(binary), mode], check=True, capture_output=True)


if __name__ == "__main__":
    unittest.main()
