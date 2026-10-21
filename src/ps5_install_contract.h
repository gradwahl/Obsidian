#ifndef POLDRIVE_INSTALL_CONTRACT_H
#define POLDRIVE_INSTALL_CONTRACT_H

/* Shared by the single-file installer, its quiet service and Media metadata.
 * The older native/web fallback uses 8097; leave that process untouched. */
#define POLDRIVE_DIRECT_PORT 8098
#define POLDRIVE_DIRECT_URL "http://127.0.0.1:8098/"
#ifndef POLDRIVE_SERVICE_PATH
#define POLDRIVE_SERVICE_PATH "/data/pldmgr/payloads/poldrive/poldrive-web-service.elf"
#define POLDRIVE_CREATE_SERVICE_DIRS
#endif
#ifndef POLDRIVE_AUTOLOAD_PATH
#define POLDRIVE_AUTOLOAD_PATH "/data/pldmgr/autoload.txt"
#endif
#ifndef POLDRIVE_CONFIG_PATH
#define POLDRIVE_CONFIG_PATH "/data/pldmgr/pldmgr_config.txt"
#endif

#endif
