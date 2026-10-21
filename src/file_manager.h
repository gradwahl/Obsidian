#ifndef WOLO_FILE_MANAGER_H
#define WOLO_FILE_MANAGER_H
#include <stddef.h>
#include <stdint.h>
#define FM_PATH 4096
#define FM_NAME 256
#define FM_LIMIT 4096
#define FM_TRANSFER_LIMIT (8 * 1024 * 1024)
#define FM_REVISION 160
typedef struct { char name[FM_NAME]; int directory; uint64_t size; } FileEntry;
/* All paths are relative to an existing root. Links and traversal are refused.
 * Operations never merge/overwrite destinations. errno describes failures. */
int fm_list(const char *root, const char *relative, FileEntry *entries, int capacity);
int fm_copy(const char *root, const char *source, const char *destination, int move);
int fm_delete(const char *root, const char *relative);
int fm_unzip(const char *root, const char *source, const char *destination);
/* Bounded file transfer. Reads allocate *bytes (caller frees). "new" writes
 * create exclusively; existing files require the revision returned by fm_read.
 * Existing-file saves stage beside the target and rename after conflict checks. */
int fm_read(const char *root, const char *relative, unsigned char **bytes, size_t *length, char revision[FM_REVISION]);
int fm_write(const char *root, const char *relative, const unsigned char *bytes, size_t length, const char *revision);
#endif
