#ifndef WOLODRIVE_TEXT_H
#define WOLODRIVE_TEXT_H

#include <stdbool.h>
#include <stddef.h>

/* start points just after the opening JSON quote; end bounds the source. */
bool text_decode_json_string(const char *start, const char *end, char *out, size_t capacity);
void text_plain_description(const char *html, char *out, size_t capacity);

#endif
