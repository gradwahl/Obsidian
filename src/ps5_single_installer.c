/* One file to copy: persist/start the quiet service, configure autoload, then
 * register a direct Media URL. No native UI or browser opener is linked here. */
#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>
#include "ps5_install_contract.h"
#ifdef WOLO_FULLSCREEN_WEBAPP
#include "ps5_fullscreen_service_payload.h"
#else
#include "ps5_service_payload.h"
#endif

#ifndef POLDRIVE_INSTALL_LOG
#ifdef WOLO_FULLSCREEN_WEBAPP
#define POLDRIVE_INSTALL_LOG "/data/poldrive-fullscreen-install.log"
#else
#define POLDRIVE_INSTALL_LOG "/data/poldrive-install.log"
#endif
#endif
#ifndef POLDRIVE_TITLE_ROOT
#define POLDRIVE_TITLE_ROOT "/user/app/WOLO00001"
#endif
#ifndef POLDRIVE_LOADER_PORT
#define POLDRIVE_LOADER_PORT 9021
#endif
#ifndef POLDRIVE_HEALTH_PORT
#define POLDRIVE_HEALTH_PORT POLDRIVE_DIRECT_PORT
#endif
#ifndef POLDRIVE_READY_ATTEMPTS
#define POLDRIVE_READY_ATTEMPTS 150
#endif
#ifndef POLDRIVE_READY_DELAY_US
#define POLDRIVE_READY_DELAY_US 100000
#endif
extern int poldrive_install_shortcut(int argc, char **argv);

static int atomic_write(const char *path, const void *data, size_t size, int executable) {
    char temporary[768];
    int n = snprintf(temporary, sizeof(temporary), "%s.tmp.%ld", path, (long)getpid());
    if (n < 0 || (size_t)n >= sizeof(temporary)) return -1;
    FILE *file = fopen(temporary, "wb");
    if (!file) return -1;
    int failed = fwrite(data, 1, size, file) != size;
    if (fflush(file) || fsync(fileno(file))) failed = 1;
    if (fclose(file)) failed = 1;
    if (executable && chmod(temporary, 0755)) failed = 1;
    if (!failed && rename(temporary, path) == 0) return 0;
    remove(temporary);
    return -1;
}

/* Bound config reads, reject binary data and preserve unrelated settings and
 * entries byte-for-byte. Do not truncate an unreadable/oversized user file. */
static char *read_text(const char *path, size_t limit, size_t *size) {
    *size = 0;
    FILE *file = fopen(path, "rb");
    if (!file && errno != ENOENT) return NULL;
    char *text = malloc(limit + 1);
    if (!text) { if (file) fclose(file); return NULL; }
    if (file) {
        *size = fread(text, 1, limit, file);
        int invalid = ferror(file) || fgetc(file) != EOF || memchr(text, 0, *size) != NULL;
        if (fclose(file)) invalid = 1;
        if (invalid) { free(text); return NULL; }
    }
    text[*size] = 0;
    return text;
}

/* Save the original file once, before the first change. Missing originals get
 * an empty backup; repeated installation never replaces that rollback copy. */
static int save_backup(const char *path, const char *text, size_t size) {
    char backup[768];
    int n = snprintf(backup, sizeof(backup), "%s.poldrive-backup", path);
    if (n < 0 || (size_t)n >= sizeof(backup)) return -1;
    struct stat info;
    if (lstat(backup, &info) == 0) return S_ISREG(info.st_mode) ? 0 : -1;
    if (errno != ENOENT) return -1;
    return atomic_write(backup, text, size, 0);
}

static int configure_autoload(FILE *log) {
    size_t list_size, config_size;
    char *list = read_text(POLDRIVE_AUTOLOAD_PATH, 4095, &list_size);
    char *config = read_text(POLDRIVE_CONFIG_PATH, 65536, &config_size);
    if (!list || !config) {
        fprintf(log, "Cannot read Payload Manager startup files; leaving them unchanged.\n");
        free(list); free(config); return -1;
    }
    const size_t path_size = strlen(POLDRIVE_SERVICE_PATH);
    int present = 0;
    for (char *line = list; *line;) {
        char *end = strchr(line, '\n');
        if (!end) end = line + strlen(line);
        char *start = line, *trimmed_end = end;
        while (start < end && isspace((unsigned char)*start)) ++start;
        while (trimmed_end > start && isspace((unsigned char)trimmed_end[-1])) --trimmed_end;
        /* Match Payload Manager's sequence format: !N means a delay in ms,
         * not a disabled payload. Do not publish an unreadable sequence. */
        size_t entry_size = (size_t)(trimmed_end-start);
        int invalid = entry_size > 511;
        for (char *p=start; p<trimmed_end; ++p)
            if (*p == ',' || *p == '\r') invalid = 1;
        if (entry_size && *start == '!') {
            unsigned delay = 0;
            if (entry_size == 1) invalid = 1;
            for (char *p=start+1; p<trimmed_end && !invalid; ++p) {
                if (*p < '0' || *p > '9' || delay > 360000) invalid = 1;
                else { delay = delay*10 + (unsigned)(*p-'0'); if (delay > 3600000) invalid = 1; }
            }
        }
        if (invalid) {
            fprintf(log, "Invalid existing autoload sequence; startup/dashboard unchanged.\n");
            free(list); free(config); return -1;
        }
        if ((size_t)(trimmed_end - start) == path_size &&
            !memcmp(start, POLDRIVE_SERVICE_PATH, path_size)) present = 1;
        line = *end ? end + 1 : end;
    }
    char updated_list[4096];
    int list_length = snprintf(updated_list, sizeof(updated_list), "%s%s%s%s", list,
        !present && list_size && list[list_size-1] != '\n' ? "\n" : "",
        present ? "" : POLDRIVE_SERVICE_PATH, present ? "" : "\n");
    /* Append a final enabled setting after removing previous copies of only
     * that key. Everything else (including unknown settings) stays intact. */
    char *updated_config = malloc(config_size + 32);
    if (!updated_config) { free(list); free(config); return -1; }
    size_t used = 0;
    for (char *line = config; *line;) {
        char *end = strchr(line, '\n');
        if (!end) end = line + strlen(line);
        char *start = line;
        while (start < end && (*start == ' ' || *start == '\t')) ++start;
        const char key[] = "AUTOLOAD_ENABLED";
        char *after = start;
        if ((size_t)(end-start) >= sizeof(key)-1 && !memcmp(start, key, sizeof(key)-1)) {
            after = start + sizeof(key)-1;
            while (after < end && (*after == ' ' || *after == '\t')) ++after;
        }
        int matching = after != start && after < end && *after == '=';
        size_t length = (size_t)(end-line) + (*end == '\n');
        if (!matching) { memcpy(updated_config + used, line, length); used += length; }
        line += length;
    }
    if (used && updated_config[used-1] != '\n') updated_config[used++] = '\n';
    const char enabled[] = "AUTOLOAD_ENABLED=1\n";
    memcpy(updated_config + used, enabled, sizeof(enabled)-1);
    used += sizeof(enabled)-1;
    /* No writes until both inputs and both resulting file sizes are valid. */
    int failed = list_length < 0 || (size_t)list_length >= sizeof(updated_list) || used > 65536;
    if (!failed && (save_backup(POLDRIVE_AUTOLOAD_PATH, list, list_size) ||
                    save_backup(POLDRIVE_CONFIG_PATH, config, config_size))) failed = 1;
    /* Publish the sequence before enabling it. Do not alter browser, delay,
     * USB, favorites, existing entry order, or other Payload Manager settings. */
    if (!failed && ((list_size != (size_t)list_length || memcmp(list, updated_list, list_size)) &&
        atomic_write(POLDRIVE_AUTOLOAD_PATH, updated_list, (size_t)list_length, 0))) failed = 1;
    if (!failed && ((config_size != used || memcmp(config, updated_config, used)) &&
        atomic_write(POLDRIVE_CONFIG_PATH, updated_config, used, 0))) failed = 1;
    free(list); free(config); free(updated_config);
    fprintf(log, failed ? "Could not configure startup; dashboard update cancelled.\n" :
        "Service added to Payload Manager autoload; existing entries/settings preserved.\n");
    return failed ? -1 : 0;
}

static int connect_local(unsigned port, long timeout_us) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return -1;
    struct timeval timeout = {timeout_us / 1000000, timeout_us % 1000000};
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    struct sockaddr_in address = {0};
#ifdef __PS5__
    address.sin_len = sizeof(address);
#endif
    address.sin_family = AF_INET;
    address.sin_port = htons((unsigned short)port);
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (connect(fd, (struct sockaddr *)&address, sizeof(address))) { close(fd); return -1; }
    return fd;
}

static int send_bytes(int fd, const void *data, size_t size) {
    const unsigned char *bytes = data;
    while (size) {
        ssize_t sent = send(fd, bytes, size, 0);
        if (sent < 0 && errno == EINTR) continue;
        if (sent <= 0) return -1;
        bytes += sent; size -= (size_t)sent;
    }
    return 0;
}

/* 1: exact service/build; 0: nothing listening; -1: occupied/untrusted service.
 * Never terminate the previous server or replace an unrelated listener. */
static int service_ready(void) {
    int fd = connect_local(POLDRIVE_HEALTH_PORT, 250000);
    if (fd < 0) return 0;
    const char request[] = "GET /api/wolodrive/health HTTP/1.1\r\nHost: 127.0.0.1\r\nConnection: close\r\n\r\n";
    char response[2048];
    size_t used = 0;
    int failed = send_bytes(fd, request, sizeof(request)-1);
    while (!failed && used < sizeof(response)-1) {
        ssize_t n = recv(fd, response + used, sizeof(response)-1-used, 0);
        if (n < 0 && errno == EINTR) continue;
        if (n < 0) { failed = 1; break; }
        if (!n) break;
        used += (size_t)n;
    }
    close(fd); response[used] = 0;
    char *body = strstr(response, "\r\n\r\n");
    return !failed && !strncmp(response, "HTTP/1.1 200 ", 13) && body &&
        used == (size_t)(body+4-response) + sizeof(POLDRIVE_EXPECTED_HEALTH)-1 &&
        !memcmp(body+4, POLDRIVE_EXPECTED_HEALTH, sizeof(POLDRIVE_EXPECTED_HEALTH)-1) ? 1 : -1;
}

int main(void) {
    signal(SIGPIPE, SIG_IGN);
    FILE *log = fopen(POLDRIVE_INSTALL_LOG, "w");
    if (!log) return 1;
    setvbuf(log, NULL, _IONBF, 0);
    struct stat info;
    if (!stat(POLDRIVE_TITLE_ROOT, &info) &&
        stat(POLDRIVE_TITLE_ROOT "/sce_sys/wolodrive-shortcut.marker", &info)) {
        fprintf(log, "Refusing to change an unowned WOLO00001 title.\n"); fclose(log); return 1;
    }
#ifdef POLDRIVE_CREATE_SERVICE_DIRS
    const char *parents[] = {"/data/pldmgr", "/data/pldmgr/payloads", "/data/pldmgr/payloads/poldrive"};
    for (size_t i=0; i<sizeof(parents)/sizeof(parents[0]); ++i) {
        if (mkdir(parents[i], 0755) && errno != EEXIST) {
            fprintf(log, "Cannot create service directory: %s\n", strerror(errno)); fclose(log); return 1;
        }
    }
#endif
    int ready = service_ready();
    if (ready < 0) {
        fprintf(log, "Port %d has a different service/build. Stop that service or restart the jailbroken environment, then retry. No dashboard/config changes.\n", POLDRIVE_HEALTH_PORT);
        fclose(log); return 1;
    }
    if (atomic_write(POLDRIVE_SERVICE_PATH, service_payload_elf, sizeof(service_payload_elf), 1)) {
        fprintf(log, "Cannot save bundled service: %s\n", strerror(errno)); fclose(log); return 1;
    }
    if (!ready) {
        int fd = connect_local(POLDRIVE_LOADER_PORT, 3000000);
        int failed = fd < 0 || send_bytes(fd, service_payload_elf, sizeof(service_payload_elf));
        if (fd >= 0) close(fd);
        if (failed) { fprintf(log, "Cannot send quiet service to ELF loader.\n"); fclose(log); return 1; }
        for (int i=0; i<POLDRIVE_READY_ATTEMPTS; ++i) {
            ready = service_ready();
            if (ready) break;
            usleep(POLDRIVE_READY_DELAY_US);
        }
        if (ready != 1) { fprintf(log, "Quiet service readiness unconfirmed; dashboard/config unchanged.\n"); fclose(log); return 1; }
    }
    fprintf(log, "Exact quiet service ready at " POLDRIVE_DIRECT_URL "; no UI opened.\n");
    if (configure_autoload(log)) { fclose(log); return 1; }
    int result = poldrive_install_shortcut(0, NULL);
    fprintf(log, result ? "Dashboard registration unconfirmed; see shortcut logs.\n" :
        "Installed. Open PolDrive under Media. No browser/native UI started by installer.\n");
    fclose(log);
    return result;
}
