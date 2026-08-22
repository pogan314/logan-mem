/*
 * embedded_stub.c — Empty asset table when built without frontend.
 *
 * Used by the standard `lsm` target (no Node.js required).
 * The `lsm-with-ui` target replaces this with generated embedded_assets.c.
 */
#include "ui/embedded_assets.h"

#include <stddef.h>
#include <string.h>

lsm_embedded_file_t LSM_EMBEDDED_FILES[] = {{NULL, NULL, 0, NULL}};
const int LSM_EMBEDDED_FILE_COUNT = 0;

const lsm_embedded_file_t *lsm_embedded_lookup(const char *path) {
    for (int i = 0; i < LSM_EMBEDDED_FILE_COUNT; i++) {
        if (strcmp(LSM_EMBEDDED_FILES[i].path, path) == 0) {
            return &LSM_EMBEDDED_FILES[i];
        }
    }
    return NULL;
}
