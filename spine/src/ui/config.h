/*
 * config.h — Persistent UI configuration.
 *
 * Stores ui_enabled and ui_port in ~/.cache/logan-spine-mcp/config.json.
 * Thread-safe: load/save are independent operations on the filesystem.
 */
#ifndef LSM_UI_CONFIG_H
#define LSM_UI_CONFIG_H

#include <stdbool.h>

/* Default values */
#define LSM_UI_DEFAULT_PORT 9749
#define LSM_UI_DEFAULT_ENABLED false

typedef struct {
    bool ui_enabled;
    int ui_port;
} lsm_ui_config_t;

/* Load config from disk. Missing/corrupt file → defaults. */
void lsm_ui_config_load(lsm_ui_config_t *cfg);

/* Atomically save one complete config generation. Creates the directory if
 * needed and reports write/sync/replace failures. */
bool lsm_ui_config_save(const lsm_ui_config_t *cfg);

/* Get the config file path. Writes to buf (up to bufsz bytes).
 * Exposed for testing. */
void lsm_ui_config_path(char *buf, int bufsz);

#endif /* LSM_UI_CONFIG_H */
