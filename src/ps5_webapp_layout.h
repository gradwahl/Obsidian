#ifndef POLDRIVE_WEBAPP_LAYOUT_H
#define POLDRIVE_WEBAPP_LAYOUT_H

/* Full app-mode titles have a system_ex launch scaffold as well as their user
 * metadata. Never replace an unowned system title or uninstall an application. */
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#ifndef WOLO_SYSTEM_APP_ROOT
#define WOLO_SYSTEM_APP_ROOT "/system_ex/app"
#endif
#define WEBAPP_DIR WOLO_SYSTEM_APP_ROOT "/" TITLE_ID
#define WEBAPP_SYS WEBAPP_DIR "/sce_sys"
#define WEBAPP_OWNER WEBAPP_DIR "/poldrive-webapp.owner"
static const char webapp_owner[] = "poldrive-webapp-layout-v2\n";

#if defined(__PS5__) && !defined(WOLO_WEBAPP_MOUNT_TEST)
#include <sys/mount.h>
#include <sys/uio.h>
static struct statfs webapp_mount;
static int webapp_mount_changed;

static int webapp_remount(int readonly) {
    struct iovec options[] = {
        {(void *)"from", 5}, {webapp_mount.f_mntfromname, strlen(webapp_mount.f_mntfromname)+1},
        {(void *)"fspath", 7}, {webapp_mount.f_mntonname, strlen(webapp_mount.f_mntonname)+1},
        {(void *)"fstype", 7}, {webapp_mount.f_fstypename, strlen(webapp_mount.f_fstypename)+1}
    };
    /* Preserve updateable flags. No ignoreacl option, forced mount, or changes
     * to the device, filesystem type, execute/suid restrictions are requested. */
    int flags = MNT_UPDATE | (int)(webapp_mount.f_flags & MNT_UPDATEMASK);
    if (readonly) flags |= MNT_RDONLY;
    return nmount(options, sizeof(options)/sizeof(options[0]), flags);
}

static int webapp_begin_write(FILE *log) {
    webapp_mount_changed = 0;
    if (statfs("/system_ex", &webapp_mount) ||
        strcmp(webapp_mount.f_mntonname, "/system_ex") ||
        strcmp(webapp_mount.f_mntfromname, "/dev/ssd0.system_ex") ||
        strcmp(webapp_mount.f_fstypename, "exfatfs")) {
        fprintf(log, "Cannot verify system_ex mount identity; no system title writes.\n");
        return -1;
    }
    if (webapp_mount.f_flags & MNT_RDONLY) {
        if (webapp_remount(0)) {
            fprintf(log, "Cannot temporarily make system_ex writable: %s\n", strerror(errno));
            return -1;
        }
        webapp_mount_changed = 1;
        fprintf(log, "system_ex temporarily writable; original read-only state will be restored.\n");
    } else {
        fprintf(log, "system_ex already writable; mount unchanged.\n");
    }
    return 0;
}

static int webapp_end_write(FILE *log) {
    if (webapp_mount_changed && webapp_remount(1)) {
        fprintf(log, "ERROR: failed restoring system_ex read-only state: %s\n", strerror(errno));
        return -1;
    }
    webapp_mount_changed = 0;
    fprintf(log, "system_ex write scope complete.\n");
    return 0;
}
#elif defined(WOLO_WEBAPP_MOUNT_TEST)
extern int webapp_begin_write(FILE *log);
extern int webapp_end_write(FILE *log);
#else
/* Host tests override the root into a disposable directory. */
static int webapp_begin_write(FILE *log) { (void)log; return 0; }
static int webapp_end_write(FILE *log) { (void)log; return 0; }
#endif

static int webapp_regular(const char *path, int empty) {
    struct stat info;
    return !lstat(path, &info) && S_ISREG(info.st_mode) && (!empty || info.st_size == 0);
}

/* Empty eboot.bin is intentional: matches SVT Play's web-app scaffold, not a
 * native executable. Validate every known object before changing any of it. */
static int webapp_owned(FILE *log, int *exists) {
    struct stat info;
    *exists = 0;
    if (lstat(WOLO_SYSTEM_APP_ROOT, &info) || !S_ISDIR(info.st_mode)) return -1;
    if (lstat(WEBAPP_DIR, &info)) return errno == ENOENT ? 0 : -1;
    *exists = 1;
    if (!S_ISDIR(info.st_mode) || !webapp_regular(WEBAPP_OWNER, 0)) goto refuse;
    char owner[sizeof(webapp_owner)] = {0};
    FILE *file = fopen(WEBAPP_OWNER, "rb");
    if (!file) goto refuse;
    size_t n = fread(owner, 1, sizeof(owner), file);
    int valid = !ferror(file) && n == sizeof(webapp_owner)-1 &&
        !memcmp(owner, webapp_owner, sizeof(webapp_owner)-1);
    fclose(file);
    if (!valid) goto refuse;
    if (!lstat(WEBAPP_SYS, &info) && !S_ISDIR(info.st_mode)) goto refuse;
    if (lstat(WEBAPP_SYS, &info) && errno != ENOENT) goto refuse;
    const char *files[] = {WEBAPP_DIR "/eboot.bin", WEBAPP_SYS "/param.json"};
    for (size_t i=0; i<2; ++i) {
        if (!lstat(files[i], &info) && (!S_ISREG(info.st_mode) || (!i && info.st_size))) goto refuse;
        if (lstat(files[i], &info) && errno != ENOENT) goto refuse;
    }
    return 0;
refuse:
    fprintf(log, "Refusing unowned, linked or unexpected system title layout: " WEBAPP_DIR "\n");
    return -1;
}

#ifdef WOLO_FULLSCREEN_WEBAPP
static int webapp_prepare(FILE *log) {
    int exists;
    if (webapp_owned(log, &exists) || webapp_begin_write(log)) return -1;
    int failed = (!exists && mkdir(WEBAPP_DIR, 0755)) ||
        write_asset(WEBAPP_OWNER, webapp_owner, sizeof(webapp_owner)-1) ||
        (mkdir(WEBAPP_SYS, 0755) && errno != EEXIST) ||
        write_asset(WEBAPP_DIR "/eboot.bin", "", 0) ||
        write_asset(WEBAPP_SYS "/param.json", param_json, sizeof(param_json)-1);
    if (failed) fprintf(log, "System web-app scaffold write failed: %s\n", strerror(errno));
    /* Restore the mount even if a file operation failed. */
    if (webapp_end_write(log)) failed = 1;
    if (!failed) fprintf(log, "System web-app scaffold prepared: " WEBAPP_DIR "/eboot.bin + sce_sys/param.json\n");
    return failed ? -1 : 0;
}
#endif

#ifdef WOLO_FULLSCREEN_RESTORE
static int webapp_only_expected(const char *path, int system_dir) {
    DIR *dir = opendir(path);
    if (!dir) return errno == ENOENT ? 0 : -1;
    int failed = 0;
    struct dirent *entry;
    errno = 0;
    while ((entry = readdir(dir))) {
        const char *name = entry->d_name;
        if (!strcmp(name, ".") || !strcmp(name, "..")) continue;
        if (system_dir ? strcmp(name, "param.json") :
            (strcmp(name, "eboot.bin") && strcmp(name, "sce_sys") && strcmp(name, "poldrive-webapp.owner"))) {
            failed = 1; break;
        }
        errno = 0;
    }
    if (errno) failed = 1;
    if (closedir(dir)) failed = 1;
    return failed ? -1 : 0;
}

static int webapp_remove_file(const char *path) { return unlink(path) && errno != ENOENT ? -1 : 0; }
static int webapp_remove_dir(const char *path) { return rmdir(path) && errno != ENOENT ? -1 : 0; }

static int webapp_restore(FILE *log) {
    int exists;
    if (webapp_owned(log, &exists)) return -1;
    if (!exists) return 0;
    if (webapp_only_expected(WEBAPP_DIR, 0) || webapp_only_expected(WEBAPP_SYS, 1)) {
        fprintf(log, "Unexpected files in system title; refusing cleanup.\n"); return -1;
    }
    if (webapp_begin_write(log)) return -1;
    /* Exact known files only. Never recurse, uninstall, or remove user data. */
    int failed = webapp_remove_file(WEBAPP_SYS "/param.json") ||
        webapp_remove_file(WEBAPP_DIR "/eboot.bin") || webapp_remove_dir(WEBAPP_SYS) ||
        webapp_remove_file(WEBAPP_OWNER) || webapp_remove_dir(WEBAPP_DIR);
    if (failed) fprintf(log, "System scaffold cleanup failed: %s\n", strerror(errno));
    if (webapp_end_write(log)) failed = 1;
    if (!failed) fprintf(log, "Owned system web-app scaffold removed; restoring browser-card tile.\n");
    return failed ? -1 : 0;
}
#endif
#endif
