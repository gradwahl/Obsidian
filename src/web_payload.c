/* PS5 browser launcher: embedded UI plus an optional same-origin LAN file bridge. */
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <signal.h>
#include <pthread.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#include "ps5_web_assets.h"
#include <zlib.h>
#ifdef WOLO_SINGLE_SERVICE
#include "ps5_install_contract.h"
#endif

#ifndef WOLO_WEB_PORT
#ifdef WOLO_SINGLE_SERVICE
#define WOLO_WEB_PORT POLDRIVE_DIRECT_PORT
#else
#define WOLO_WEB_PORT 8097
#endif
#endif
#ifdef WOLO_NATIVE_FALLBACK
#define WOLO_WEB_SUFFIX "#native-unavailable"
#elif defined(WOLO_WEB_AUTOSTART)
/* Direct Media shortcut contains no hash; never show the native-failure toast. */
#define WOLO_WEB_SUFFIX ""
#elif defined(WOLO_FILE_BRIDGE)
#define WOLO_WEB_SUFFIX "#ps5-files"
#else
#define WOLO_WEB_SUFFIX ""
#endif
#ifdef WOLO_WEB_AUTOSTART
#define WOLO_SERVER_ID "wolodrive-webfirst-v2-files-lan"
#elif defined(WOLO_FILE_BRIDGE)
#define WOLO_SERVER_ID "wolodrive-server-v11-files-lan-open"
#else
#define WOLO_SERVER_ID "wolodrive-server-v7"
#endif
static char web_url[64]; /* Set once from getsockname, before starting the worker. */
#if defined(__PS5__) && defined(WOLO_SINGLE_SERVICE)
#define ENDPOINT_RECORD "/data/poldrive-service.endpoint"
#elif defined(__PS5__)
#define ENDPOINT_RECORD "/data/wolodrive-web.endpoint"
#else
#define ENDPOINT_RECORD "wolodrive-web.endpoint"
#endif

#if defined(__PS5__) && !defined(WOLO_WEB_NO_BROWSER)
#include "ps5_browser_helper.h"
#endif

static int send_all(int fd, const void *data, size_t length);

#if defined(WOLO_WEB_TEST_BROWSER)
extern int sceSystemServiceLaunchWebBrowser(const char *uri);
#endif

#if (defined(__PS5__) || defined(WOLO_WEB_TEST_BROWSER)) && !defined(WOLO_WEB_NO_BROWSER)
static void *launch_browser(void *unused) {
    (void)unused;
    fprintf(stderr, "[POLDRIVE WEB] Requesting isolated browser helper at %s\n", web_url);
#ifdef WOLO_WEB_TEST_BROWSER
    int result = sceSystemServiceLaunchWebBrowser(web_url);
    fprintf(stderr, "[POLDRIVE WEB] Browser launch returned 0x%x\n", (unsigned)result);
    if (result)
        fprintf(stderr, "[POLDRIVE WEB] Browser launch failed; server remains available at %s\n", web_url);
#else
    unsigned char *payload = malloc(sizeof(browser_helper_elf));
    if (!payload) { fprintf(stderr, "[POLDRIVE WEB] Browser helper allocation failed\n"); return NULL; }
    memcpy(payload, browser_helper_elf, sizeof(browser_helper_elf));
    memset(payload + BROWSER_URL_OFFSET, 0, BROWSER_URL_SIZE);
    memcpy(payload + BROWSER_URL_OFFSET, web_url, strlen(web_url));
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd >= 0) {
        struct timeval timeout = {3, 0};
        setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
        struct sockaddr_in loader;
        memset(&loader, 0, sizeof(loader));
        loader.sin_len = sizeof(loader);
        loader.sin_family = AF_INET;
        loader.sin_port = htons(9021);
        loader.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        if (connect(fd, (struct sockaddr *)&loader, sizeof(loader)) ||
            send_all(fd, payload, sizeof(browser_helper_elf))) {
            perror("[POLDRIVE WEB] Browser helper transfer");
        } else fprintf(stderr, "[POLDRIVE WEB] Browser helper sent; see /data/wolodrive-browser.log\n");
        close(fd);
    } else perror("[POLDRIVE WEB] Browser helper socket");
    free(payload);
#endif
    return NULL;
}
#endif

#ifndef WOLO_WEB_AUTOSTART
static void request_browser(void) {
#if (defined(__PS5__) || defined(WOLO_WEB_TEST_BROWSER)) && !defined(WOLO_WEB_NO_BROWSER)
    pthread_t browser_thread;
    int error = pthread_create(&browser_thread, NULL, launch_browser, NULL);
    if (error) {
        fprintf(stderr, "[POLDRIVE WEB] Browser thread failed: %s; open %s manually\n",
                strerror(error), web_url);
    } else {
        error = pthread_detach(browser_thread);
        if (error) fprintf(stderr, "[POLDRIVE WEB] Browser thread detach: %s\n", strerror(error));
    }
#endif
}
#endif /* !WOLO_WEB_AUTOSTART */

static volatile sig_atomic_t stopping;
static void stop_server(int sig) { (void)sig; stopping = 1; }

static int send_all(int fd, const void *data, size_t length) {
    const unsigned char *bytes = data;
    while (length) {
        ssize_t sent = send(fd, bytes, length, 0);
        if (sent < 0 && errno == EINTR) continue;
        if (sent <= 0) return -1;
        bytes += sent;
        length -= (size_t)sent;
    }
    return 0;
}

static int reuse_running_server(void) {
    FILE *record = fopen(ENDPOINT_RECORD, "r");
    unsigned port = 0;
    if (!record) return 0;
    int parsed = fscanf(record, "%u", &port);
    fclose(record);
    if (parsed != 1 || !port || port > 65535) return 0;
#ifdef WOLO_WEB_FIXED_PORT
    /* A matching build on another port cannot satisfy this tile's URL. */
    if (port != WOLO_WEB_PORT) return 0;
#endif
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return 0;
    struct timeval timeout = {2, 0};
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));
#ifdef __PS5__
    address.sin_len = sizeof(address);
#endif
    address.sin_family = AF_INET;
    address.sin_port = htons((unsigned short)port);
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    const char request[] = "GET /api/wolodrive/health HTTP/1.0\r\n\r\n";
    int found = 0;
    if (!connect(fd, (struct sockaddr *)&address, sizeof(address)) &&
        !send_all(fd, request, sizeof(request) - 1)) {
        char response[1024], expected[128];
        size_t used = 0;
        snprintf(expected, sizeof(expected), WOLO_SERVER_ID " %s\n", web_build_id);
        while (used < sizeof(response) - 1) {
            ssize_t got = recv(fd, response + used, sizeof(response) - 1 - used, 0);
            if (got <= 0) break;
            used += (size_t)got;
            response[used] = 0;
            if (strstr(response, expected)) { found = 1; break; }
        }
    }
    close(fd);
    if (found) snprintf(web_url, sizeof(web_url), "http://127.0.0.1:%u/" WOLO_WEB_SUFFIX, port);
    return found;
}

static void respond_policy(int fd, int status, const char *reason, const char *mime,
                           const void *body, size_t length, int head,
                           const char *cache_policy) {
    char header[512];
    int size = snprintf(header, sizeof(header),
        "HTTP/1.1 %d %s\r\nContent-Type: %s\r\nContent-Length: %zu\r\n"
        "Connection: close\r\nCache-Control: %s\r\n"
        "X-Content-Type-Options: nosniff\r\n"
#ifdef WOLO_FILE_BRIDGE
        "X-PolDrive-Files: 3\r\n"
#endif
        "%s\r\n",
        status, reason, mime, length, cache_policy,
        status == 405 ? "Allow: GET, HEAD\r\n" : "");
    if (size < 0 || (size_t)size >= sizeof(header)) return;
    if (send_all(fd, header, (size_t)size) == 0 && !head)
        send_all(fd, body, length);
}

static void respond(int fd, int status, const char *reason, const char *mime,
                    const void *body, size_t length, int head) {
    respond_policy(fd, status, reason, mime, body, length, head, "no-store");
}

/* Store only compressed multi-megabyte text bundles in the ELF. Modern clients
   get gzip directly; clients without Accept-Encoding receive streaming inflate.
   Neither route creates another multi-megabyte buffer in the payload. */
static void respond_asset(int fd, const WoloWebAsset *asset, int head, int accepts_gzip,
                          const char *policy) {
    if (!asset->is_gzip) {
        respond_policy(fd, 200, "OK", asset->mime, asset->data, asset->size, head, policy);
        return;
    }
    char header[512];
    int length = snprintf(header, sizeof(header),
        "HTTP/1.1 200 OK\r\nContent-Type: %s\r\nContent-Length: %zu\r\n"
        "Connection: close\r\nCache-Control: %s\r\n"
        "X-Content-Type-Options: nosniff\r\nVary: Accept-Encoding\r\n"
#ifdef WOLO_FILE_BRIDGE
        "X-PolDrive-Files: 3\r\n"
#endif
        "%s\r\n", asset->mime, accepts_gzip ? asset->size : asset->raw_size,
        policy, accepts_gzip ? "Content-Encoding: gzip\r\n" : "");
    if (length < 0 || (size_t)length >= sizeof(header) ||
        send_all(fd, header, (size_t)length) != 0 || head) return;
    if (accepts_gzip) {
        send_all(fd, asset->data, asset->size);
        return;
    }
    z_stream stream = {0};
    if (inflateInit2(&stream, MAX_WBITS + 16) != Z_OK) return;
    stream.next_in = (Bytef *)asset->data;
    stream.avail_in = (uInt)asset->size;
    unsigned char output[16384];
    int rc;
    do {
        stream.next_out = output;
        stream.avail_out = sizeof(output);
        rc = inflate(&stream, Z_NO_FLUSH);
        if (rc != Z_OK && rc != Z_STREAM_END) break;
        size_t written = sizeof(output) - stream.avail_out;
        if (written && send_all(fd, output, written) != 0) break;
    } while (rc != Z_STREAM_END);
    inflateEnd(&stream);
}

#ifdef WOLO_FILE_BRIDGE
#include "file_bridge.h"
#endif

/* Inspect only the Accept-Encoding field. Looking for "gzip" anywhere in
   the request (e.g. inside User-Agent or an unrelated header) can select
   compressed JavaScript for a browser that never agreed to decompress it. */
static int accepts_gzip_encoding(const char *headers) {
    const char *line = headers;
    const char key[] = "Accept-Encoding:";
    const size_t key_length = sizeof(key) - 1;
    while (line && *line) {
        const char *end = strstr(line, "\r\n");
        if (!end || end == line) break;
        if ((size_t)(end - line) >= key_length &&
            strncasecmp(line, key, key_length) == 0) {
            const char *p = line + key_length;
            while (p < end) {
                while (p < end && (*p == ' ' || *p == '\t' || *p == ',')) ++p;
                const char *token = p;
                while (p < end && *p != ',' && *p != ';') ++p;
                const char *token_end = p;
                while (token_end > token && (token_end[-1] == ' ' || token_end[-1] == '\t'))
                    --token_end;
                int gzip = token_end - token == 4 && strncasecmp(token, "gzip", 4) == 0;
                int allowed = 1;
                if (p < end && *p == ';') {
                    const char *options_end = memchr(p, ',', (size_t)(end - p));
                    if (!options_end) options_end = end;
                    const char *q = p + 1;
                    while (q < options_end && (*q == ' ' || *q == '\t')) ++q;
                    if (options_end - q >= 3 && (q[0] == 'q' || q[0] == 'Q') && q[1] == '=') {
                        /* q=0, q=0.0 etc are explicitly disallowed. */
                        if (q[2] == '0') {
                            const char *value_end = q + 3;
                            while (value_end < options_end && *value_end == '0') ++value_end;
                            if (value_end < options_end && *value_end == '.') {
                                ++value_end;
                                while (value_end < options_end && *value_end == '0') ++value_end;
                            }
                            if (value_end == options_end) allowed = 0;
                        }
                    }
                    p = options_end;
                }
                if (gzip && allowed) return 1;
                while (p < end && *p != ',') ++p;
                if (p < end) ++p;
            }
            return 0;
        }
        line = end + 2;
    }
    return 0;
}

static void handle_client(int fd) {
    struct timeval timeout = {2, 0};
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
    char request[32768];
    size_t used = 0;
    int complete = 0;
    while (used < sizeof(request) - 1 && !stopping) {
        ssize_t got = recv(fd, request + used, sizeof(request) - 1 - used, 0);
        if (got < 0 && errno == EINTR) continue;
        if (got <= 0) break;
        used += (size_t)got;
        request[used] = 0;
        if (strstr(request, "\r\n\r\n")) { complete = 1; break; }
    }
    char method[16], path[16384], version[16], extra;
    char *body = complete ? strstr(request, "\r\n\r\n") + 4 : NULL;
    char *line_end = complete ? strstr(request, "\r\n") : NULL;
    int accepts_gzip = line_end && accepts_gzip_encoding(line_end + 2);
    if (line_end) *line_end = 0;
    if (!line_end || sscanf(request, "%15s %16383s %15s %c", method, path, version, &extra) != 3 ||
        (strcmp(version, "HTTP/1.0") && strcmp(version, "HTTP/1.1")) || path[0] != '/') {
        respond(fd, 400, "Bad Request", "text/plain", "Bad request\n", 12, 0);
        return;
    }
    int head = strcmp(method, "HEAD") == 0;
#ifdef WOLO_FILE_BRIDGE
    if (!strncmp(path, "/api/wolodrive/files", 20)) {
        char *headers = line_end + 2;
        char *query = strchr(path, '?');
        if (query) *query++ = 0;
        int is_content=!strcmp(path,"/api/wolodrive/files/content");
        int is_session=!strcmp(path,"/api/wolodrive/files/session"), local=0;
        if (!files_request_allowed(fd,headers,&local) || (!is_session && !files_authorized(fd,headers))) {
            files_failure(fd,403,"File access requires a same-origin LAN session",head); return;
        }
        char length[24], transfer[80];
        int has_length = files_header(headers, "Content-Length", length, sizeof(length));
        size_t wanted = 0, maximum=is_content ? FM_TRANSFER_LIMIT : is_session ? 1024 : 30000;
        if (has_length == 1 && length[0]) {
            for (const char *p = length; *p; ++p) {
                if (!isdigit((unsigned char)*p) || wanted > maximum) { has_length = -1; break; }
                wanted = wanted * 10 + (size_t)(*p-'0');
            }
        } else if (has_length == 1) has_length = -1;
        if (has_length < 0 || wanted > maximum || files_header(headers, "Transfer-Encoding", transfer, sizeof(transfer)) != 0) {
            files_failure(fd, 400, "Invalid request length", head); return;
        }
        size_t offset = (size_t)(body-request);
        char *payload=malloc(wanted+1);
        if (!payload) { files_failure(fd,500,"Out of memory",head); return; }
        size_t received=used-offset;
        if (received>wanted) received=wanted;
        memcpy(payload,body,received);
        while (received < wanted) {
            ssize_t got = recv(fd, payload+received, wanted-received, 0);
            if (got < 0 && errno == EINTR) continue;
            if (got <= 0) { free(payload); files_failure(fd, 400, "Incomplete request", head); return; }
            received += (size_t)got;
        }
        if (!is_content && memchr(payload, 0, wanted)) { free(payload); files_failure(fd, 400, "Invalid request body", head); return; }
        payload[wanted] = 0;
        files_handle(fd, method, path, query, headers, payload, wanted, head);
        free(payload);
        return;
    }
#else
    (void)body;
#endif
    if (!head && strcmp(method, "GET")) {
        respond(fd, 405, "Method Not Allowed", "text/plain", "GET or HEAD required\n", 21, 0);
        return;
    }
    char *query = strchr(path, '?');
    if (query) *query = 0;
    const char *lookup = strcmp(path, "/") ? path : "/index.html";
    if (!strcmp(lookup, "/api/wolodrive/health")) {
        char identity[128];
        int size = snprintf(identity, sizeof(identity), WOLO_SERVER_ID " %s\n", web_build_id);
        respond(fd, 200, "OK", "text/plain", identity, (size_t)size, head);
        return;
    }
    /* Trace a bounded number of static requests to distinguish an HTML-only
       boot from a JavaScript/CSS load failure on the actual PS5 browser.
       Never log file bridge paths, headers, or session tokens. */
    static unsigned static_request_log_count;
    for (size_t i = 0; i < web_asset_count; i++) {
        const WoloWebAsset *asset = &web_assets[i];
        if (!strcmp(lookup, asset->path)) {
            if (static_request_log_count < 96) {
                fprintf(stderr, "[POLDRIVE WEB] Static %s %s | type=%s | bytes=%zu | gzip=%d\n",
                        head ? "HEAD" : "GET", lookup, asset->mime, asset->size,
                        asset->is_gzip && accepts_gzip);
                if (++static_request_log_count == 96)
                    fprintf(stderr, "[POLDRIVE WEB] Static request tracing limit reached\n");
            }
            /* Cache immutable-in-this-build static files, not API responses. */
            /* Content-addressed generated pages can be cached; always
               revalidate manifest.json to discover a new build's page hashes. */
            const int cached_pages = strncmp(lookup, "/poldrive-pages/", 16) == 0 &&
                strcmp(lookup, "/poldrive-pages/manifest.json") != 0;
            const char *policy = strncmp(lookup, "/assets/", 8) == 0 || cached_pages ?
                "public, max-age=86400" : "no-store";
            respond_asset(fd, asset, head, accepts_gzip, policy);
            return;
        }
    }
    /* Unknown URLs never fall through to the console filesystem. */
    respond(fd, 404, "Not Found", "text/plain", "Not found\n", 10, head);
}

int main(void) {
    signal(SIGPIPE, SIG_IGN);
    if (reuse_running_server()) {
        fprintf(stderr, "[POLDRIVE WEB] Reusing running server at %s\n", web_url);
#if (defined(__PS5__) || defined(WOLO_WEB_TEST_BROWSER)) && !defined(WOLO_WEB_NO_BROWSER)
        /* Wait for helper transfer before this short-lived launcher exits. */
        launch_browser(NULL);
#endif
        return 0;
    }
#ifdef __PS5__
    /* Separate from SDL diagnostics so each executable retains its own errors. */
    FILE *log = fopen("/data/wolodrive-web.log", "w");
    if (log) {
        if (dup2(fileno(log), STDERR_FILENO) < 0) perror("redirect web log");
        fclose(log);
    } else perror("open /data/wolodrive-web.log");
#endif
    setvbuf(stderr, NULL, _IONBF, 0);
    fprintf(stderr, "[POLDRIVE WEB] Entered main (%s); %zu embedded files\n", WOLO_SERVER_ID, web_asset_count);
    fprintf(stderr, "[POLDRIVE WEB] Process ID: %ld\n", (long)getpid());
    signal(SIGPIPE, SIG_IGN);
    signal(SIGINT, stop_server);
    signal(SIGTERM, stop_server);
    int listener = socket(AF_INET, SOCK_STREAM, 0);
    if (listener < 0) { perror("socket"); return 1; }
    int reuse = 1;
    setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));
#ifdef __PS5__
    address.sin_len = sizeof(address);
#endif
    address.sin_family = AF_INET;
    address.sin_port = htons(WOLO_WEB_PORT);
#ifdef WOLO_WEB_LOCAL_ONLY
    /* Autoloaded file bridge must not expose itself to the whole LAN. */
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
#else
    /* Serve both the console's Media tile and same-origin LAN clients. */
    address.sin_addr.s_addr = htonl(INADDR_ANY);
#endif
    int bound = bind(listener, (struct sockaddr *)&address, sizeof(address));
    if (bound < 0 && errno == EADDRINUSE) {
#ifdef WOLO_WEB_FIXED_PORT
        /* The Media tile points to exactly this port. A random fallback port
           would silently send it to an old/wrong server. Fail clearly. */
        fprintf(stderr, "[POLDRIVE WEB] Web-first port %d is busy. Stop the old server and retry; no random port fallback.\n",
                WOLO_WEB_PORT);
#else
        fprintf(stderr, "[POLDRIVE WEB] Port %d occupied; requesting a free port\n", WOLO_WEB_PORT);
        address.sin_port = 0;
        bound = bind(listener, (struct sockaddr *)&address, sizeof(address));
#endif
    }
    if (bound < 0 || listen(listener, 16)) {
        perror("[POLDRIVE WEB] bind/listen");
        close(listener);
        return 1;
    }
    socklen_t address_size = sizeof(address);
    if (getsockname(listener, (struct sockaddr *)&address, &address_size)) {
        perror("[POLDRIVE WEB] getsockname");
        close(listener);
        return 1;
    }
#ifdef WOLO_FILE_BRIDGE
    if (files_bridge_init((unsigned)ntohs(address.sin_port)))
        fprintf(stderr, "[POLDRIVE FILES] Session randomness unavailable; bridge disabled\n");
    else fprintf(stderr, "[POLDRIVE FILES] Filesystem bridge ready; code-free LAN access enabled; initial path /data\n");
#endif
    snprintf(web_url, sizeof(web_url), "http://127.0.0.1:%u/" WOLO_WEB_SUFFIX, (unsigned)ntohs(address.sin_port));
    FILE *record = fopen(ENDPOINT_RECORD, "w");
    if (record) {
        fprintf(record, "%u %ld\n", (unsigned)ntohs(address.sin_port), (long)getpid());
        fclose(record);
    } else perror("[POLDRIVE WEB] Save endpoint record");
    fprintf(stderr, "[POLDRIVE WEB] Listening at %s\n", web_url);
    fprintf(stderr, "[POLDRIVE WEB] Listener descriptor: %d\n", listener);
#ifdef WOLO_WEB_LOCAL_ONLY
    fprintf(stderr, "[POLDRIVE WEB] Loopback-only: http://127.0.0.1:%u/ (no LAN file access)\n",
            (unsigned)ntohs(address.sin_port));
#else
    fprintf(stderr, "[POLDRIVE WEB] LAN catalogue: http://<PS5-IP>:%u/ (Files accessible to trusted LAN clients; package installation unavailable)\n",
            (unsigned)ntohs(address.sin_port));
#endif
#ifdef WOLO_WEB_AUTOSTART
    fprintf(stderr, "[POLDRIVE WEB] Web-first background service ready; browser launch disabled.\n");
#else
    request_browser();
#endif
    fprintf(stderr, "[POLDRIVE WEB] HTTP request loop active\n");
    unsigned consecutive_accept_errors = 0;
    while (!stopping) {
        struct pollfd readable = {listener, POLLIN, 0};
        int ready = poll(&readable, 1, 1000);
        if (ready < 0) {
            if (errno == EINTR) continue;
            perror("poll");
            close(listener);
            return 1;
        }
        if (!ready) continue;
        if (!(readable.revents & POLLIN)) {
            fprintf(stderr, "[POLDRIVE WEB] Listener poll error: 0x%x\n", readable.revents);
            close(listener);
            return 1;
        }
#ifdef WOLO_WEB_TEST_ACCEPT_FAILURE
        /* Deterministic regression hook; not present in shipping builds. */
        static int test_accepted_once;
        int client;
        if (test_accepted_once) {
            errno = EINVAL;
            client = -1;
        } else {
            client = accept(listener, NULL, NULL);
            if (client >= 0) test_accepted_once = 1;
        }
#else
        int client = accept(listener, NULL, NULL);
#endif
        if (client < 0) {
            if (errno == EINTR) continue;
            int error = errno;
            /* On PS5 a repeatedly failing listener previously busy-spun,
               filling /data/wolodrive-web.log with millions of identical
               lines. A persistent error cannot be repaired by retrying
               accept() on the same descriptor forever. */
            if (++consecutive_accept_errors == 1 || consecutive_accept_errors == 4)
                fprintf(stderr, "[POLDRIVE WEB] accept failed: errno=0x%x (%d), attempt=%u\n",
                        (unsigned)error, error, consecutive_accept_errors);
            if (consecutive_accept_errors >= 8) {
                fprintf(stderr, "[POLDRIVE WEB] Fatal: listener repeatedly failed; stopping instead of spinning. Restart the payload.\n");
                close(listener);
                return 1;
            }
            /* Delay outside the I/O loop; do not burn CPU or disk bandwidth. */
            poll(NULL, 0, 200);
            continue;
        }
        consecutive_accept_errors = 0;
        handle_client(client);
        close(client);
    }
    close(listener);
    fprintf(stderr, "[POLDRIVE WEB] Stopped\n");
    return 0;
}
