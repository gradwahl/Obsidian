#ifndef OBSIDIAN_INSTALL_CONTRACT_H
#define OBSIDIAN_INSTALL_CONTRACT_H

/* Shared by the single-file installer, its quiet service and Media metadata.
 * The older native/web fallback uses 8097; leave that process untouched. */
#define OBSIDIAN_DIRECT_PORT 8098
#define OBSIDIAN_DIRECT_URL "http://127.0.0.1:8098/"
#ifndef OBSIDIAN_SERVICE_PATH
#define OBSIDIAN_SERVICE_PATH "/data/pldmgr/payloads/obsidian/obsidian-web-service.elf"
#define OBSIDIAN_CREATE_SERVICE_DIRS
#endif
#ifndef OBSIDIAN_AUTOLOAD_PATH
#define OBSIDIAN_AUTOLOAD_PATH "/data/pldmgr/autoload.txt"
#endif
#ifndef OBSIDIAN_CONFIG_PATH
#define OBSIDIAN_CONFIG_PATH "/data/pldmgr/pldmgr_config.txt"
#endif

#endif
