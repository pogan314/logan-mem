/*
 * userconfig.h — User-defined file extension → language mappings.
 *
 * Reads extra_extensions from two optional JSON config files:
 *   Global:  $XDG_CONFIG_HOME/logan-spine-mcp/config.json
 *            (falls back to ~/.config/logan-spine-mcp/config.json)
 *   Project: {repo_root}/.logan-spine.json
 *
 * Project config wins over global. Unknown language values warn and are
 * skipped (fail-open). Missing files are silently ignored.
 *
 * Format:
 *   {"extra_extensions": {".blade.php": "php", ".mjs": "javascript"}}
 *
 * The language string matching is case-insensitive.
 */
#ifndef LSM_USERCONFIG_H
#define LSM_USERCONFIG_H

#include "lsm.h" /* LSMLanguage */
#include "foundation/sha256.h"

/* ── Types ──────────────────────────────────────────────────────── */

typedef struct {
    char *ext;        /* file extension including dot, e.g. ".blade.php" */
    LSMLanguage lang; /* resolved language enum */
} lsm_userext_t;

typedef struct {
    lsm_userext_t *entries; /* heap-allocated array */
    int count;              /* number of entries */
    /* Digests of the exact bytes/state consumed by lsm_userconfig_load(). */
    char global_source_sha256[LSM_SHA256_HEX_LEN + 1];
    char project_source_sha256[LSM_SHA256_HEX_LEN + 1];
} lsm_userconfig_t;

/* ── API ────────────────────────────────────────────────────────── */

/*
 * Load user config from global + project files, merge (project wins).
 * repo_path: absolute path to the repository root (for project config).
 * Returns a heap-allocated lsm_userconfig_t (caller must free via
 * lsm_userconfig_free). Returns NULL only on allocation failure.
 * Missing config files are silently ignored.
 */
lsm_userconfig_t *lsm_userconfig_load(const char *repo_path);

/*
 * Look up a file extension in the user config.
 * ext: extension including dot, e.g. ".blade.php"
 * Returns the mapped LSMLanguage, or LSM_LANG_COUNT if not found.
 */
LSMLanguage lsm_userconfig_lookup(const lsm_userconfig_t *cfg, const char *ext);

/* Free a lsm_userconfig_t returned by lsm_userconfig_load. NULL-safe. */
void lsm_userconfig_free(lsm_userconfig_t *cfg);

/* ── Integration hook ───────────────────────────────────────────── */

/*
 * Set the process-global user config that lsm_language_for_extension()
 * will consult before the built-in table.
 * cfg may be NULL to clear the override.
 * Not thread-safe — call before spawning worker threads.
 */
void lsm_set_user_lang_config(const lsm_userconfig_t *cfg);

/*
 * Get the currently active process-global user config.
 * Returns NULL if none has been set.
 * Called internally by lsm_language_for_extension().
 */
const lsm_userconfig_t *lsm_get_user_lang_config(void);

#endif /* LSM_USERCONFIG_H */
