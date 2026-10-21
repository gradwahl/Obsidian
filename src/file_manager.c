#include "file_manager.h"
#include <sys/stat.h>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

static int fail(int error) { errno = error; return -1; }
static int join(char *out, const char *a, const char *b) {
    return snprintf(out, FM_PATH, "%s/%s", a, b) >= FM_PATH ? fail(ENAMETOOLONG) : 0;
}
/* Walk every existing component, refusing symlinks even within the root. */
static int resolve(const char *root, const char *rel, char *out, int missing) {
    if (!root || !rel || *rel == '/' || strchr(rel, '\\')) return fail(EINVAL);
    if (!realpath(root, out)) return -1;
    if (strlen(rel) >= FM_PATH) return fail(ENAMETOOLONG);
    char copy[FM_PATH]; strcpy(copy, rel);
    char *save = NULL, *part = strtok_r(copy, "/", &save);
    while (part) {
        if (!strcmp(part, ".") || !strcmp(part, "..")) return fail(EINVAL);
        char next[FM_PATH]; if (join(next, out, part)) return -1;
        strcpy(out, next);
        char *following = strtok_r(NULL, "/", &save);
        struct stat st;
        if (lstat(out, &st)) {
            if (!(missing && !following && errno == ENOENT)) return -1;
        } else if (S_ISLNK(st.st_mode)) return fail(ELOOP);
        part = following;
    }
    return 0;
}
static int compare(const void *a, const void *b) {
    const FileEntry *x = a, *y = b;
    return x->directory != y->directory ? y->directory - x->directory : strcmp(x->name, y->name);
}
int fm_read(const char *root, const char *rel, unsigned char **bytes, size_t *length, char revision[FM_REVISION]) {
    char path[FM_PATH];
    *bytes = NULL; *length = 0;
    if (!rel || !*rel) return fail(EINVAL);
    if (resolve(root, rel, path, 0)) return -1;
    int fd = open(path, O_RDONLY | O_NOFOLLOW | O_NONBLOCK);
    if (fd < 0) return -1;
    struct stat before, after;
    int error = 0;
    unsigned char *data = NULL;
    if (fstat(fd, &before)) { error=errno; goto done; }
    if (!S_ISREG(before.st_mode)) { error=EINVAL; goto done; }
    if (before.st_size < 0 || before.st_size > FM_TRANSFER_LIMIT) { error=EFBIG; goto done; }
    size_t size=(size_t)before.st_size, used=0;
    data=malloc(size+1);
    if (!data) { error=ENOMEM; goto done; }
    while (used<size) {
        ssize_t got=read(fd,data+used,size-used);
        if (got<0 && errno==EINTR) continue;
        if (got<=0) { error=got<0 ? errno : ESTALE; goto done; }
        used+=(size_t)got;
    }
    if (fstat(fd,&after)) { error=errno; goto done; }
    if (after.st_size!=before.st_size || after.st_mtime!=before.st_mtime || after.st_ctime!=before.st_ctime) {
        error=ESTALE; goto done;
    }
    data[size]=0;
    snprintf(revision,FM_REVISION,"%llx-%llx-%llx-%llx-%llx-%lx",
        (unsigned long long)after.st_dev,(unsigned long long)after.st_ino,
        (unsigned long long)after.st_size,(unsigned long long)after.st_mtime,
        (unsigned long long)after.st_ctime,(unsigned long)crc32(0,data,(uInt)size));
    *bytes=data; *length=size; data=NULL;
done:
    free(data); close(fd);
    return error ? fail(error) : 0;
}
static int write_bytes(int fd, const unsigned char *bytes, size_t length) {
    while (length) {
        ssize_t written=write(fd,bytes,length);
        if (written<0 && errno==EINTR) continue;
        if (written<=0) return fail(written<0 ? errno : EIO);
        bytes+=written; length-=(size_t)written;
    }
    return fsync(fd);
}
int fm_write(const char *root, const char *rel, const unsigned char *bytes, size_t length, const char *revision) {
    if (!rel || !*rel || !revision || !*revision || length>FM_TRANSFER_LIMIT) return fail(EINVAL);
    char path[FM_PATH];
    if (resolve(root,rel,path,!strcmp(revision,"new"))) return -1;
    if (!strcmp(revision,"new")) {
        int fd=open(path,O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW,0600);
        if (fd<0) return -1;
        int result=write_bytes(fd,bytes,length), error=errno;
        if (close(fd) && !result) { result=-1; error=errno; }
        if (result) { unlink(path); return fail(error); }
        return 0;
    }
    unsigned char *old=NULL; size_t old_size; char current[FM_REVISION]; struct stat original;
    if (fm_read(root,rel,&old,&old_size,current)) return -1;
    free(old);
    if (strcmp(current,revision)) return fail(ESTALE);
    if (lstat(path,&original)) return -1;
    if (!S_ISREG(original.st_mode)) return fail(EINVAL);
    char temporary[FM_PATH];
    strcpy(temporary,path); char *slash=strrchr(temporary,'/');
    if (!slash || snprintf(slash+1,FM_PATH-(size_t)(slash+1-temporary),".obsidian-save-XXXXXX") >=
        (int)(FM_PATH-(size_t)(slash+1-temporary))) return fail(ENAMETOOLONG);
    int fd=mkstemp(temporary);
    if (fd<0) return -1;
    int result=write_bytes(fd,bytes,length), error=errno;
    if (!result && (fchown(fd,original.st_uid,original.st_gid) || fchmod(fd,original.st_mode & 07777) || fsync(fd))) {
        result=-1; error=errno;
    }
    if (close(fd) && !result) { result=-1; error=errno; }
    if (!result) {
        old=NULL;
        if (fm_read(root,rel,&old,&old_size,current)) { result=-1; error=errno; }
        else if (strcmp(current,revision)) { result=-1; error=ESTALE; }
        free(old);
    }
    if (!result && rename(temporary,path)) { result=-1; error=errno; }
    if (result) unlink(temporary);
    return result ? fail(error) : 0;
}
int fm_list(const char *root, const char *rel, FileEntry *entries, int capacity) {
    char path[FM_PATH]; if (resolve(root, rel, path, 0)) return -1;
    DIR *dir = opendir(path); if (!dir) return -1;
    int count = 0, error = 0; struct dirent *entry;
    while ((entry = readdir(dir))) {
        if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, "..")) continue;
        char child[FM_PATH]; struct stat st;
        if (join(child, path, entry->d_name) || lstat(child, &st)) { error = errno; break; }
        if (!S_ISREG(st.st_mode) && !S_ISDIR(st.st_mode)) continue;
        if (count == capacity || strlen(entry->d_name) >= FM_NAME) { error = EOVERFLOW; break; }
        FileEntry *item = &entries[count++]; strcpy(item->name, entry->d_name);
        item->directory = S_ISDIR(st.st_mode); item->size = (uint64_t)st.st_size;
    }
    closedir(dir);
    if (error) return fail(error);
    qsort(entries, count, sizeof(*entries), compare); return count;
}
static int remove_tree(const char *path, int depth) {
    if (depth > 64) return fail(ELOOP);
    struct stat st; if (lstat(path, &st)) return -1;
    if (S_ISLNK(st.st_mode)) return fail(ELOOP);
    if (!S_ISDIR(st.st_mode)) return unlink(path);
    DIR *dir = opendir(path); if (!dir) return -1;
    int result = 0; struct dirent *entry;
    while ((entry = readdir(dir))) {
        if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, "..")) continue;
        char child[FM_PATH];
        if (join(child, path, entry->d_name) || remove_tree(child, depth + 1)) { result = -1; break; }
    }
    int error = errno; closedir(dir);
    if (result) return fail(error);
    return rmdir(path);
}
static int copy_tree(const char *source, const char *dest, int depth) {
    if (depth > 64) return fail(ELOOP);
    struct stat st; if (lstat(source, &st)) return -1;
    if (S_ISDIR(st.st_mode)) {
        if (mkdir(dest, 0700)) return -1;
        DIR *dir = opendir(source);
        if (!dir) { int error = errno; rmdir(dest); return fail(error); }
        int result = 0; struct dirent *entry;
        while ((entry = readdir(dir))) {
            if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, "..")) continue;
            char from[FM_PATH], to[FM_PATH];
            if (join(from, source, entry->d_name) || join(to, dest, entry->d_name) || copy_tree(from, to, depth + 1)) { result = -1; break; }
        }
        int error = errno; closedir(dir);
        if (result) { remove_tree(dest, 0); return fail(error); }
        return 0;
    }
    if (!S_ISREG(st.st_mode)) return fail(ELOOP);
    int in = open(source, O_RDONLY | O_NOFOLLOW); if (in < 0) return -1;
    int out = open(dest, O_WRONLY | O_CREAT | O_EXCL, 0600);
    if (out < 0) { int error = errno; close(in); return fail(error); }
    char buf[65536]; ssize_t n; int result = 0;
    while ((n = read(in, buf, sizeof(buf))) > 0) {
        ssize_t offset = 0;
        while (offset < n) {
            ssize_t written = write(out, buf + offset, n - offset);
            if (written <= 0) { result = -1; break; } offset += written;
        }
        if (result) break;
    }
    if (n < 0) result = -1;
    int error = errno;
    if (close(out) && !result) { result = -1; error = errno; }
    close(in);
    if (result) { unlink(dest); return fail(error); }
    return 0;
}
int fm_delete(const char *root, const char *rel) {
    if (!rel || !*rel) return fail(EINVAL);
    char path[FM_PATH]; if (resolve(root, rel, path, 0)) return -1;
    return remove_tree(path, 0);
}
int fm_copy(const char *root, const char *source, const char *destination, int move) {
    if (!source || !*source || !destination || !*destination) return fail(EINVAL);
    char from[FM_PATH], to[FM_PATH]; struct stat st, original;
    if (resolve(root, source, from, 0) || resolve(root, destination, to, 1)) return -1;
    size_t len = strlen(from);
    if (!strcmp(from, to) || (!strncmp(from, to, len) && to[len] == '/')) return fail(EINVAL);
    if (!lstat(to, &st)) return fail(EEXIST);
    if (errno != ENOENT) return -1;
    if (lstat(from, &original)) return -1;
    /* Use exclusive copy creation even on the same volume. Portable rename()
     * may replace a destination that appeared after our absence check. */
    if (copy_tree(from, to, 0)) return -1;
    if (move && (lstat(from, &st) || st.st_dev != original.st_dev || st.st_ino != original.st_ino)) return fail(ESTALE);
    /* A failed source removal keeps the completed destination; report failure. */
    return move ? remove_tree(from, 0) : 0;
}

static unsigned u16(const unsigned char *p) { return p[0] | (p[1] << 8); }
static uint32_t u32(const unsigned char *p) { return u16(p) | ((uint32_t)u16(p+2) << 16); }
static int safe_zip_name(const char *name) {
    if (!*name || *name == '/' || strchr(name, '\\') || strchr(name, ':')) return 0;
    if (strstr(name,"//")) return 0;
    for (const unsigned char *p=(const unsigned char *)name; *p; p++) if (*p<32) return 0;
    char copy[FM_PATH]; if (strlen(name) >= sizeof(copy)) return 0; strcpy(copy, name);
    int depth=0;
    char *save, *part = strtok_r(copy, "/", &save);
    while (part) {
        if (!strcmp(part, ".") || !strcmp(part, "..") || ++depth>64) return 0;
        part = strtok_r(NULL, "/", &save);
    }
    return 1;
}
static int valid_utf8(const unsigned char *text) {
    while(*text) {
        unsigned c=*text++, more=0, minimum=0;
        if(c<128) continue;
        if(c>=194 && c<=223) { more=1; c&=31; minimum=128; }
        else if(c>=224 && c<=239) { more=2; c&=15; minimum=2048; }
        else if(c>=240 && c<=244) { more=3; c&=7; minimum=65536; }
        else return 0;
        while(more--) { if((*text&192)!=128) return 0; c=(c<<6)|(*text++&63); }
        if(c<minimum || c>0x10ffff || (c>=0xd800 && c<=0xdfff)) return 0;
    }
    return 1;
}
/* Classic ZIP store/deflate, max 128 MiB input / 512 MiB output / 4096 entries.
 * A fresh destination isolates extraction; CRC and sizes are verified. */
int fm_unzip(const char *root, const char *source, const char *destination) {
    char path[FM_PATH], dest[FM_PATH];
    if (!*destination || resolve(root, source, path, 0) || resolve(root, destination, dest, 1)) return -1;
    FILE *file = fopen(path, "rb"); if (!file) return -1;
    if (fseek(file, 0, SEEK_END)) { fclose(file); return -1; }
    long size = ftell(file);
    if (size < 22 || size > 128*1024*1024) { fclose(file); return fail(EFBIG); }
    unsigned char *data = malloc((size_t)size); if (!data) { fclose(file); return fail(ENOMEM); }
    rewind(file); int read_ok = fread(data, 1, size, file) == (size_t)size; fclose(file);
    int error = EINVAL, created = 0, result = -1;
    char **seen=NULL; unsigned seen_count=0;
    if (!read_ok) { error = EIO; goto done; }
    long end = size - 22;
    while (end >= 0 && end >= size-65557 && (u32(data+end) != 0x06054b50 || end+22+u16(data+end+20) != size)) end--;
    if (end < 0 || end < size-65557) goto done;
    unsigned count = u16(data+end+10);
    if (u16(data+end+4) || u16(data+end+6) || u16(data+end+8) != count || count > FM_LIMIT) goto done;
    size_t position = u32(data+end+16); uint64_t total = 0;
    if (position > (size_t)end || u32(data+end+12) != (size_t)end-position) goto done;
    seen=calloc(count ? count : 1,sizeof(*seen)); if(!seen) { error=ENOMEM; goto done; }
    if (mkdir(dest, 0700)) { error = errno; goto done; } created = 1;
    for (unsigned i = 0; i < count; i++) {
        if (position+46 > (size_t)end || u32(data+position) != 0x02014b50) goto done;
        const unsigned char *header = data+position;
        unsigned namesize = u16(header+28), method = u16(header+10);
        uint32_t packed = u32(header+20), unpacked = u32(header+24), offset = u32(header+42);
        size_t next = position+46+namesize+u16(header+30)+u16(header+32);
        if (!namesize || namesize >= FM_PATH || next > (size_t)end || (u16(header+8)&1) ||
            (method != 0 && method != 8) || ((u32(header+38)>>16)&0170000) == 0120000) goto done;
        char name[FM_PATH]; memcpy(name, header+46, namesize); name[namesize] = 0;
        if (strlen(name) != namesize || !safe_zip_name(name)) goto done;
        if(!valid_utf8((const unsigned char *)name)) goto done;
        if (!(u16(header+8)&2048)) for (unsigned j=0;j<namesize;j++) if((unsigned char)name[j]>127) goto done;
        for(unsigned j=0;j<seen_count;j++) if(!strcmp(seen[j],name)) goto done;
        seen[seen_count]=strdup(name); if(!seen[seen_count]) { error=ENOMEM; goto done; } seen_count++;
        total += unpacked; if (total > 512*1024*1024) { error = EFBIG; goto done; }
        int directory = name[namesize-1] == '/'; if (directory) name[namesize-1] = 0;
        /* Create only parents inside our fresh destination. */
        char relative[FM_PATH], target[FM_PATH];
        if (join(relative, destination, name)) goto done;
        for (char *p = relative + strlen(destination)+1; *p; p++) {
            if (*p != '/') continue;
            *p = 0;
            if (resolve(root, relative, target, 1) || (mkdir(target,0700) && errno != EEXIST)) { *p='/'; error=errno; goto done; }
            *p = '/';
        }
        if (resolve(root, relative, target, 1)) { error=errno; goto done; }
        if (directory) {
            if(unpacked || u32(header+16)) goto done;
            if (mkdir(target,0700) && errno != EEXIST) { error=errno; goto done; }
        }
        {
            if ((uint64_t)offset+30 > (uint64_t)size || u32(data+offset) != 0x04034b50) goto done;
            uint64_t start = (uint64_t)offset+30+u16(data+offset+26)+u16(data+offset+28);
            if (start+packed > (uint64_t)size) goto done;
            int fd = directory ? -1 : open(target, O_WRONLY|O_CREAT|O_EXCL,0600);
            if (fd < 0 && !directory) { error=errno; goto done; }
            unsigned char buf[65536]; uint64_t written = 0; uLong crc = crc32(0, Z_NULL, 0);
            z_stream stream = {0}; stream.next_in = data+start; stream.avail_in=packed;
            int code = method == 8 ? inflateInit2(&stream,-MAX_WBITS) : Z_OK;
            if (code != Z_OK) { if(fd>=0) close(fd); goto done; }
            do {
                size_t n;
                if (method == 8) {
                    stream.next_out=buf; stream.avail_out=sizeof(buf); code=inflate(&stream,Z_NO_FLUSH); n=sizeof(buf)-stream.avail_out;
                } else { n=packed-written; if(n>sizeof(buf)) n=sizeof(buf); memcpy(buf,data+start+written,n); code=written+n==packed ? Z_STREAM_END : Z_OK; }
                if (written+n > unpacked || (code != Z_OK && code != Z_STREAM_END) || (!n && code != Z_STREAM_END)) { error=EINVAL; code=Z_DATA_ERROR; break; }
                size_t sent=0;
                while(sent<n) { ssize_t amount=write(fd,buf+sent,n-sent); if(amount<=0) { error=errno; code=Z_ERRNO; break; } sent+=amount; }
                if(code==Z_ERRNO) break;
                crc=crc32(crc,buf,n); written+=n;
            } while(code != Z_STREAM_END);
            int valid=code==Z_STREAM_END && written==unpacked && crc==u32(header+16) && (method==0 || stream.total_in==packed);
            if(method==8) inflateEnd(&stream);
            if(fd>=0 && close(fd)) { valid=0; error=errno; }
            if(!valid) goto done;
        }
        position = next;
    }
    if (position != (size_t)end) goto done;
    result=0;
done:
    for(unsigned i=0;i<seen_count;i++) free(seen[i]);
    free(seen);
    free(data);
    if(result && created) remove_tree(dest,0);
    return result ? fail(error) : 0;
}
