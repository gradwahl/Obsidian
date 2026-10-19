#include "text.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

static size_t encode_utf8(unsigned code, char bytes[4]) {
    if (!code || code > 0x10ffff || (code >= 0xd800 && code <= 0xdfff)) code = 0xfffd;
    if (code < 0x80) { bytes[0] = (char)code; return 1; }
    if (code < 0x800) {
        bytes[0] = (char)(0xc0 | (code >> 6)); bytes[1] = (char)(0x80 | (code & 63)); return 2;
    }
    if (code < 0x10000) {
        bytes[0] = (char)(0xe0 | (code >> 12)); bytes[1] = (char)(0x80 | ((code >> 6) & 63));
        bytes[2] = (char)(0x80 | (code & 63)); return 3;
    }
    bytes[0] = (char)(0xf0 | (code >> 18)); bytes[1] = (char)(0x80 | ((code >> 12) & 63));
    bytes[2] = (char)(0x80 | ((code >> 6) & 63)); bytes[3] = (char)(0x80 | (code & 63)); return 4;
}

static size_t utf8_length(const char *text, const char *end) {
    unsigned char lead = (unsigned char)*text;
    size_t length = lead < 0x80 ? 1 : lead < 0xe0 ? 2 : lead < 0xf0 ? 3 : 4;
    if ((size_t)(end - text) < length) return 0;
    for (size_t i = 1; i < length; i++) {
        if (((unsigned char)text[i] & 0xc0) != 0x80) return 0;
    }
    return length;
}

static int hex_digit(char digit) {
    if (digit >= '0' && digit <= '9') return digit - '0';
    if (digit >= 'a' && digit <= 'f') return digit - 'a' + 10;
    if (digit >= 'A' && digit <= 'F') return digit - 'A' + 10;
    return -1;
}

static bool read_hex4(const char **cursor, const char *end, unsigned *code) {
    if (end - *cursor < 4) return false;
    *code = 0;
    for (int i = 0; i < 4; i++) {
        int digit = hex_digit((*cursor)[i]);
        if (digit < 0) return false;
        *code = (*code << 4) | (unsigned)digit;
    }
    *cursor += 4;
    return true;
}

bool text_decode_json_string(const char *start, const char *end, char *out, size_t capacity) {
    if (!out || !capacity) return false;
    out[0] = '\0';
    size_t used = 0;
    bool full = false;
    const char *p = start;
    while (p < end) {
        if (*p == '"') { out[used] = '\0'; return used > 0; }
        char bytes[4];
        size_t length;
        if (*p == '\\') {
            if (++p == end) break;
            char escaped = *p++;
            if (escaped == 'u') {
                unsigned code;
                if (!read_hex4(&p, end, &code)) break;
                if (code >= 0xd800 && code <= 0xdbff) {
                    unsigned low;
                    if (end - p < 6 || p[0] != '\\' || p[1] != 'u') break;
                    p += 2;
                    if (!read_hex4(&p, end, &low) || low < 0xdc00 || low > 0xdfff) break;
                    code = 0x10000 + ((code - 0xd800) << 10) + low - 0xdc00;
                }
                length = encode_utf8(code, bytes);
            } else {
                switch (escaped) {
                    case 'n': bytes[0] = '\n'; break;
                    case 'r': bytes[0] = '\r'; break;
                    case 't': bytes[0] = '\t'; break;
                    case 'b': bytes[0] = '\b'; break;
                    case 'f': bytes[0] = '\f'; break;
                    case '"': case '\\': case '/': bytes[0] = escaped; break;
                    default: out[0] = '\0'; return false;
                }
                length = 1;
            }
        } else {
            length = utf8_length(p, end);
            if (!length) break;
            memcpy(bytes, p, length);
            p += length;
        }
        if (!full && used + length < capacity) {
            memcpy(out + used, bytes, length);
            used += length;
        } else full = true; /* Truncate once, always at a UTF-8 boundary. */
    }
    out[0] = '\0';
    return false;
}

static size_t decode_entity(const char *text, unsigned *code) {
    const char *semicolon = strchr(text, ';');
    if (!semicolon || semicolon - text > 16) return 0;
    size_t length = (size_t)(semicolon - text + 1);
    static const struct { const char *name; unsigned code; } entities[] = {
        {"&amp;", '&'}, {"&lt;", '<'}, {"&gt;", '>'}, {"&quot;", '"'}, {"&apos;", '\''},
        {"&nbsp;", ' '}, {"&ndash;", 0x2013}, {"&mdash;", 0x2014}, {"&hellip;", 0x2026},
        {"&lsquo;", 0x2018}, {"&rsquo;", 0x2019}, {"&ldquo;", 0x201c}, {"&rdquo;", 0x201d},
        {"&copy;", 0xa9}, {"&reg;", 0xae}, {"&trade;", 0x2122}, {"&bull;", 0x2022}
    };
    for (size_t i = 0; i < sizeof(entities) / sizeof(entities[0]); i++) {
        if (strlen(entities[i].name) == length && strncmp(text, entities[i].name, length) == 0) {
            *code = entities[i].code;
            return length;
        }
    }
    if (text[1] == '#') {
        unsigned value = 0;
        size_t i = 2;
        bool hex = text[i] == 'x' || text[i] == 'X';
        if (hex) i++;
        size_t first = i;
        for (; text + i < semicolon; i++) {
            int digit = hex ? hex_digit(text[i]) : (text[i] >= '0' && text[i] <= '9' ? text[i] - '0' : -1);
            if (digit < 0 || value > 0x10ffff / (hex ? 16u : 10u)) return 0;
            value = value * (hex ? 16u : 10u) + (unsigned)digit;
        }
        if (i == first || value > 0x10ffff) return 0;
        *code = value == 0xa0 ? ' ' : value;
        return length;
    }
    return 0;
}

static bool starts_ignore_case(const char *text, const char *prefix) {
    while (*prefix) {
        if (tolower((unsigned char)*text++) != (unsigned char)*prefix++) return false;
    }
    return true;
}

static const char *tag_end(const char *text) {
    char quote = 0;
    for (; *text; text++) {
        if (quote) { if (*text == quote) quote = 0; }
        else if (*text == '\'' || *text == '"') quote = *text;
        else if (*text == '>') return text + 1;
    }
    return text;
}

void text_plain_description(const char *html, char *out, size_t capacity) {
    if (!out || !capacity) return;
    out[0] = '\0';
    if (!html) return;
    size_t source_size = strlen(html);
    char *decoded = malloc(source_size + 1);
    if (!decoded) return;
    size_t length = 0;
    for (const char *p = html; *p;) {
        unsigned code;
        size_t entity = *p == '&' ? decode_entity(p, &code) : 0;
        if (entity) { length += encode_utf8(code, decoded + length); p += entity; }
        else decoded[length++] = *p++;
    }
    decoded[length] = '\0';
    size_t used = 0;
    bool space = false;
    const char *end = decoded + length;
    for (const char *p = decoded; *p;) {
        if (strncmp(p, "<!--", 4) == 0) {
            const char *close = strstr(p + 4, "-->");
            p = close ? close + 3 : end;
            continue;
        }
        if (*p == '<' && (isalpha((unsigned char)p[1]) || p[1] == '/' || p[1] == '!')) {
            const char *name = p + 1 + (p[1] == '/');
            char tag[16];
            size_t i = 0;
            while (isalnum((unsigned char)name[i]) && i + 1 < sizeof(tag)) {
                tag[i] = (char)tolower((unsigned char)name[i]); i++;
            }
            tag[i] = '\0';
            bool block = !strcmp(tag, "p") || !strcmp(tag, "br") || !strcmp(tag, "div") ||
                         !strcmp(tag, "li") || !strcmp(tag, "tr") || !strcmp(tag, "hr") ||
                         (tag[0] == 'h' && tag[1] >= '1' && tag[1] <= '6' && !tag[2]);
            bool hidden = p[1] != '/' && (!strcmp(tag, "script") || !strcmp(tag, "style"));
            p = tag_end(p + 1);
            if (hidden) {
                for (; *p; p++) {
                    if (p[0] == '<' && p[1] == '/' && starts_ignore_case(p + 2, tag)) break;
                }
                if (*p) p = tag_end(p + 2);
            }
            if (block || hidden) space = used > 0;
            continue;
        }
        if (isspace((unsigned char)*p) || ((unsigned char)p[0] == 0xc2 && (unsigned char)p[1] == 0xa0)) {
            space = used > 0;
            p += (unsigned char)p[0] == 0xc2 ? 2 : 1;
            continue;
        }
        size_t bytes = utf8_length(p, end);
        if (!bytes || used + bytes + (space ? 1 : 0) >= capacity) break;
        if (space) out[used++] = ' ';
        memcpy(out + used, p, bytes);
        used += bytes;
        p += bytes;
        space = false;
    }
    out[used] = '\0';
    free(decoded);
}
