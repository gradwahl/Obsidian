/* PS5 browser launcher: embedded UI plus an optional console-local file bridge. */
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <signal.h>
#include <pthread.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#include "ps5_web_assets.h"

#ifndef WOLO_WEB_PORT
#define WOLO_WEB_PORT 8097
#endif
#ifdef WOLO_NATIVE_FALLBACK
#define WOLO_WEB_SUFFIX "#native-unavailable"
#elif defined(WOLO_FILE_BRIDGE)
#define WOLO_WEB_SUFFIX "#ps5-files"
#else
#define WOLO_WEB_SUFFIX ""
#endif
#ifdef WOLO_FILE_BRIDGE
#define WOLO_SERVER_ID "wolodrive-server-v10-files-lan-open"
#else
#define WOLO_SERVER_ID "wolodrive-server-v7"
#endif
static char web_url[64]; /* Set once from getsockname, before starting the worker. */
#ifdef __PS5__
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

static void respond(int fd, int status, const char *reason, const char *mime,
                    const void *body, size_t length, int head) {
    char header[512];
    int size = snprintf(header, sizeof(header),
        "HTTP/1.1 %d %s\r\nContent-Type: %s\r\nContent-Length: %zu\r\n"
        "Connection: close\r\nCache-Control: no-store\r\n"
        "X-Content-Type-Options: nosniff\r\n"
#ifdef WOLO_FILE_BRIDGE
        "X-PolDrive-Files: 3\r\n"
#endif
        "%s\r\n",
        status, reason, mime, length,
        status == 405 ? "Allow: GET, HEAD\r\n" : "");
    if (size < 0 || (size_t)size >= sizeof(header)) return;
    if (send_all(fd, header, (size_t)size) == 0 && !head)
        send_all(fd, body, length);
}

#ifdef WOLO_FILE_BRIDGE
#include "file_bridge.h"
#endif

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
    for (size_t i = 0; i < web_asset_count; i++) {
        const WoloWebAsset *asset = &web_assets[i];
        if (!strcmp(lookup, asset->path)) {
            if (!strcmp(lookup, "/index.html"))
                fprintf(stderr, "[POLDRIVE WEB] Serving index.html\n");
            respond(fd, 200, "OK", asset->mime, asset->data, asset->size, head);
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
    /* Serve the same catalogue to the console browser and other LAN devices. */
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    int bound = bind(listener, (struct sockaddr *)&address, sizeof(address));
    if (bound < 0 && errno == EADDRINUSE) {
        fprintf(stderr, "[POLDRIVE WEB] Port %d occupied; requesting a free port\n", WOLO_WEB_PORT);
        address.sin_port = 0;
        bound = bind(listener, (struct sockaddr *)&address, sizeof(address));
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
    fprintf(stderr, "[POLDRIVE WEB] LAN catalogue: http://<PS5-IP>:%u/ (Files accessible to trusted LAN clients; package installation unavailable)\n",
            (unsigned)ntohs(address.sin_port));
    request_browser();
    fprintf(stderr, "[POLDRIVE WEB] HTTP request loop active\n");
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
        int client = accept(listener, NULL, NULL);
        if (client < 0) {
            if (errno == EINTR) continue;
            perror("accept");
            continue;
        }
        handle_client(client);
        close(client);
    }
    close(listener);
    fprintf(stderr, "[POLDRIVE WEB] Stopped\n");
    return 0;
}
