/* Included by the payload HTTP server after respond(). No browser/SDL APIs. */
#include "file_manager.h"
#include <ctype.h>
#include <strings.h>
#include <sys/stat.h>
#include <time.h>

#ifndef WOLO_FILES_ROOT
#define WOLO_FILES_ROOT "/"
#endif
static char files_token[65];
static unsigned files_port;

static int files_bridge_init(unsigned port) {
    unsigned char random[32];
    FILE *file = fopen("/dev/urandom", "rb");
    if (!file) file = fopen("/dev/random", "rb");
    if (!file) return -1;
    size_t got = fread(random, 1, sizeof(random), file);
    fclose(file);
    if (got != sizeof(random)) return -1;
    for (size_t i = 0; i < 32; ++i) sprintf(files_token + 2*i, "%02x", random[i]);
    files_port = port;
    return 0;
}
static void files_json(int fd, int status, const char *json, int head) {
    respond(fd, status, status == 200 ? "OK" : "File bridge error", "application/json; charset=utf-8", json, strlen(json), head);
}
static void files_failure(int fd, int status, const char *message, int head) {
    /* Error messages come only from constants/strerror, never user paths. */
    char json[1024];
    snprintf(json, sizeof(json), "{\"error\":\"%s\"}", message);
    files_json(fd, status, json, head);
}
/* Return one header, rejecting duplicates and oversized values. */
static int files_header(const char *headers, const char *name, char *out, size_t capacity) {
    int found = 0;
    size_t len = strlen(name);
    while (*headers && strncmp(headers, "\r\n", 2)) {
        const char *end = strstr(headers, "\r\n");
        if (!end) return -1;
        if ((size_t)(end-headers) > len && !strncasecmp(headers, name, len) && headers[len] == ':') {
            if (found++) return -1;
            const char *start = headers + len + 1;
            while (start < end && (*start == ' ' || *start == '\t')) ++start;
            while (end > start && (end[-1] == ' ' || end[-1] == '\t')) --end;
            size_t size = (size_t)(end-start);
            if (size >= capacity) return -1;
            memcpy(out, start, size); out[size] = 0;
            end = strstr(headers, "\r\n");
        }
        headers = end + 2;
    }
    return found;
}
static int files_request_allowed(int fd, const char *headers, int *local) {
    struct sockaddr_in peer, address;
    socklen_t size = sizeof(peer), address_size = sizeof(address);
    if (getpeername(fd, (struct sockaddr *)&peer, &size) || peer.sin_family != AF_INET ||
        getsockname(fd, (struct sockaddr *)&address, &address_size)) return 0;
    uint32_t ip=ntohl(peer.sin_addr.s_addr);
    if ((ip>>24)!=127 && (ip>>24)!=10 && (ip>>20)!=0xac1 && (ip>>16)!=0xc0a8 && (ip>>16)!=0xa9fe) return 0;
    *local=peer.sin_addr.s_addr==htonl(INADDR_LOOPBACK) && address.sin_addr.s_addr==htonl(INADDR_LOOPBACK);
    char host[80], expected[80], origin[100], site[32], marker[16];
    char address_text[INET_ADDRSTRLEN];
    if (!inet_ntop(AF_INET,&address.sin_addr,address_text,sizeof(address_text))) return 0;
    snprintf(expected, sizeof(expected), "%s:%u", address_text, files_port);
    if (files_header(headers, "Host", host, sizeof(host)) != 1 || strcmp(host, expected)) return 0;
    snprintf(expected, sizeof(expected), "http://%s:%u", address_text, files_port);
    int present = files_header(headers, "Origin", origin, sizeof(origin));
    if (present < 0 || (present && strcmp(origin, expected))) return 0;
    present = files_header(headers, "Sec-Fetch-Site", site, sizeof(site));
    if (present < 0 || (present && strcmp(site, "same-origin") && strcmp(site, "none"))) return 0;
    return files_header(headers, "X-PolDrive-Bridge", marker, sizeof(marker)) == 1 && !strcmp(marker, "1");
}
static int files_authorized(int fd, const char *headers) {
    int local; char token[80];
    return files_token[0] && files_request_allowed(fd,headers,&local) &&
        files_header(headers,"X-PolDrive-Token",token,sizeof(token))==1 && !strcmp(token,files_token);
}
static int files_hex(char c) {
    if (c >= '0' && c <= '9') return c-'0';
    if (c >= 'a' && c <= 'f') return c-'a'+10;
    if (c >= 'A' && c <= 'F') return c-'A'+10;
    return -1;
}
static int files_decode(const char *text, size_t length, char *out, size_t capacity) {
    size_t used = 0;
    for (size_t i = 0; i < length; ++i) {
        unsigned char c = (unsigned char)text[i];
        if (c == '%') {
            if (i+2 >= length || files_hex(text[i+1]) < 0 || files_hex(text[i+2]) < 0) return -1;
            c = (unsigned char)(files_hex(text[i+1])*16 + files_hex(text[i+2])); i += 2;
        } else if (c == '+') c = ' ';
        if (!c || used+1 >= capacity) return -1;
        out[used++] = (char)c;
    }
    out[used] = 0;
    return 0;
}
static int files_field(const char *form, const char *key, char *out, size_t capacity) {
    int found = 0;
    size_t keylen = strlen(key);
    out[0] = 0;
    while (*form) {
        const char *end = strchr(form, '&');
        if (!end) end = form+strlen(form);
        const char *equal = memchr(form, '=', (size_t)(end-form));
        if (!equal) return -1;
        if ((size_t)(equal-form) == keylen && !strncmp(form, key, keylen)) {
            if (found++ || files_decode(equal+1, (size_t)(end-equal-1), out, capacity)) return -1;
        }
        form = *end ? end+1 : end;
    }
    return found;
}
/* Bound JSON encoding to six bytes per filename byte, including controls. */
static char *files_quote(char *out, const char *text) {
    static const char hex[] = "0123456789abcdef";
    *out++ = '"';
    for (const unsigned char *p = (const unsigned char *)text; *p; ++p) {
        if (*p == '"' || *p == '\\') { *out++ = '\\'; *out++ = (char)*p; }
        else if (*p < 32) { memcpy(out, "\\u00", 4); out+=4; *out++=hex[*p>>4]; *out++=hex[*p&15]; }
        else *out++ = (char)*p;
    }
    *out++ = '"'; *out = 0; return out;
}
static void files_handle(int fd, const char *method, const char *path, const char *query,
                         const char *headers, const char *body, size_t body_length, int head) {
    int local=0;
    if (!files_token[0] || !files_request_allowed(fd, headers, &local)) {
        files_failure(fd, 403, "File access requires a same-origin console or LAN connection", head); return;
    }
    if (!strcmp(path, "/api/wolodrive/files/session")) {
        /* Any allowed same-origin private-LAN peer may request a session.
           No access code is required. All file operations still need the
           random session token and the same-origin/Host/request checks. */
        if (!strcmp(method, "GET")) {
            char json[160];
            snprintf(json, sizeof(json), "{\"version\":3,\"token\":\"%s\"}", files_token);
            files_json(fd, 200, json, head);
        } else files_failure(fd, 405, "GET required", head);
        return;
    }
    if (!files_authorized(fd,headers)) {
        files_failure(fd, 403, "File session expired; reload PolDrive", head); return;
    }
    if (!strcmp(path,"/api/wolodrive/files/content")) {
        char relative[FM_PATH], revision[FM_REVISION];
        if (files_field(query ? query : "","path",relative,sizeof(relative))!=1 || !relative[0]) {
            files_failure(fd,400,"Invalid file path",head); return;
        }
        if (!strcmp(method,"GET") || head) {
            unsigned char *bytes=NULL; size_t length=0;
            if (fm_read(WOLO_FILES_ROOT,relative,&bytes,&length,revision)) { files_failure(fd,400,strerror(errno),head); return; }
            char header[512];
            int n=snprintf(header,sizeof(header),"HTTP/1.1 200 OK\r\nContent-Type: application/octet-stream\r\n"
                "Content-Length: %zu\r\nConnection: close\r\nCache-Control: no-store\r\nX-Content-Type-Options: nosniff\r\n"
                "Content-Disposition: attachment\r\nX-PolDrive-Revision: %s\r\n\r\n",length,revision);
            if (send_all(fd,header,(size_t)n)==0 && !head) send_all(fd,bytes,length);
            free(bytes); return;
        }
        if (!strcmp(method,"POST")) {
            char type[80];
            if (files_header(headers,"Content-Type",type,sizeof(type))!=1 || strcmp(type,"application/octet-stream") ||
                files_header(headers,"X-PolDrive-Revision",revision,sizeof(revision))!=1) {
                files_failure(fd,400,"Missing file revision or content type",head); return;
            }
            if (fm_write(WOLO_FILES_ROOT,relative,(const unsigned char *)body,body_length,revision)) {
                int error=errno;
                files_failure(fd,error==ESTALE || error==EEXIST ? 409 : 400,
                    error==ESTALE ? "File changed on console; reload before saving" : strerror(error),head);
            } else files_json(fd,200,"{\"ok\":true}",head);
            return;
        }
        files_failure(fd,405,"GET, HEAD or POST required",head); return;
    }
    if (strcmp(path, "/api/wolodrive/files")) { files_failure(fd, 404, "Unknown file endpoint", head); return; }
    char source[FM_PATH], destination[FM_PATH], action[16];
    if (!strcmp(method, "GET")) {
        if (files_field(query ? query : "", "path", source, sizeof(source)) != 1) {
            files_failure(fd, 400, "Invalid folder path", head); return;
        }
        FileEntry *entries = calloc(FM_LIMIT, sizeof(*entries));
        if (!entries) { files_failure(fd, 500, "Out of memory", head); return; }
        int count = fm_list(WOLO_FILES_ROOT, source, entries, FM_LIMIT), error = errno;
        if (count < 0) { free(entries); files_failure(fd, 400, strerror(error), head); return; }
        size_t capacity = 32 + (size_t)count * (FM_NAME*6+100);
        char *json = malloc(capacity);
        if (!json) { free(entries); files_failure(fd, 500, "Out of memory", head); return; }
        char *cursor = json; cursor += sprintf(cursor, "{\"entries\":[");
        for (int i = 0; i < count; ++i) {
            if (i) *cursor++ = ',';
            cursor += sprintf(cursor, "{\"name\":"); cursor = files_quote(cursor, entries[i].name);
            cursor += sprintf(cursor, ",\"kind\":\"%s\",\"size\":%llu}",
                entries[i].directory ? "directory" : "file", (unsigned long long)entries[i].size);
        }
        strcpy(cursor, "]}"); files_json(fd, 200, json, head); free(json); free(entries); return;
    }
    if (strcmp(method, "POST")) { files_failure(fd, 405, "GET or POST required", head); return; }
    char type[80];
    if (files_header(headers, "Content-Type", type, sizeof(type)) != 1 ||
        strcmp(type, "application/x-www-form-urlencoded") ||
        files_field(body, "action", action, sizeof(action)) != 1 ||
        files_field(body, "source", source, sizeof(source)) != 1 || !source[0] ||
        files_field(body, "destination", destination, sizeof(destination)) < 0) {
        files_failure(fd, 400, "Invalid file operation", head); return;
    }
    int result;
    if (!strcmp(action, "delete")) result = fm_delete(WOLO_FILES_ROOT, source);
    else if (!strcmp(action, "copy") || !strcmp(action, "move"))
        result = fm_copy(WOLO_FILES_ROOT, source, destination, !strcmp(action, "move"));
    else if (!strcmp(action, "unzip")) {
        if (snprintf(destination, sizeof(destination), "%s.extracted", source) >= (int)sizeof(destination)) {
            files_failure(fd, 400, "Archive path too long", head); return;
        }
        result = fm_unzip(WOLO_FILES_ROOT, source, destination);
    } else { files_failure(fd, 400, "Unknown file operation", head); return; }
    if (result) files_failure(fd, 400, strerror(errno), head);
    else files_json(fd, 200, "{\"ok\":true}", head);
}
