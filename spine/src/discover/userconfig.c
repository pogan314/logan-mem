/*
 * userconfig.c — User-defined extension→language mappings.
 *
 * Reads extra_extensions from:
 *   Global:  $XDG_CONFIG_HOME/logan-spine-mcp/config.json
 *            (falls back to ~/.config/logan-spine-mcp/config.json)
 *   Project: {repo_root}/.logan-spine.json
 *
 * Project config wins over global. Unknown language values warn and are
 * skipped (fail-open). Missing files are silently ignored.
 */
#include "discover/userconfig.h"
#include "lsm.h" /* LSMLanguage, LSM_LANG_* */
#include "foundation/constants.h"
#include "foundation/platform.h" /* lsm_safe_getenv */
#include "foundation/compat_fs.h"
#include "foundation/sha256.h"

enum { MAX_CONFIG_SIZE = 65536 };
#include "foundation/log.h"

#include <yyjson/yyjson.h>

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── Process-global user config pointer ──────────────────────────── */

static const lsm_userconfig_t *g_userconfig = NULL;

static void userconfig_source_digest(const char *state, const void *bytes, size_t len,
                                     char out[LSM_SHA256_HEX_LEN + 1]) {
    static const char domain[] = "lsm-userconfig-source-v1";
    lsm_sha256_ctx sha;
    lsm_sha256_init(&sha);
    lsm_sha256_update(&sha, domain, sizeof(domain));
    lsm_sha256_update(&sha, state, strlen(state) + 1);
    if (bytes && len > 0) {
        lsm_sha256_update(&sha, bytes, len);
    }
    uint8_t digest[LSM_SHA256_DIGEST_LEN];
    lsm_sha256_final(&sha, digest);
    static const char hex[] = "0123456789abcdef";
    for (int i = 0; i < LSM_SHA256_DIGEST_LEN; i++) {
        out[i * 2] = hex[digest[i] >> 4];
        out[i * 2 + 1] = hex[digest[i] & 0x0f];
    }
    out[LSM_SHA256_HEX_LEN] = '\0';
}

void lsm_set_user_lang_config(const lsm_userconfig_t *cfg) {
    g_userconfig = cfg;
}

const lsm_userconfig_t *lsm_get_user_lang_config(void) {
    return g_userconfig;
}

/* ── Language name → enum table ──────────────────────────────────── */

/*
 * Reverse-mapping from lowercase language name strings to LSMLanguage.
 * Covers all names exposed by lsm_language_name() plus common aliases.
 */
typedef struct {
    const char *name; /* lowercase */
    LSMLanguage lang;
} lang_name_entry_t;

static const lang_name_entry_t LANG_NAME_TABLE[] = {
    {"go", LSM_LANG_GO},
    {"python", LSM_LANG_PYTHON},
    {"javascript", LSM_LANG_JAVASCRIPT},
    {"typescript", LSM_LANG_TYPESCRIPT},
    {"tsx", LSM_LANG_TSX},
    {"rust", LSM_LANG_RUST},
    {"java", LSM_LANG_JAVA},
    {"c++", LSM_LANG_CPP},
    {"cpp", LSM_LANG_CPP},
    {"c#", LSM_LANG_CSHARP},
    {"csharp", LSM_LANG_CSHARP},
    {"php", LSM_LANG_PHP},
    {"lua", LSM_LANG_LUA},
    {"scala", LSM_LANG_SCALA},
    {"kotlin", LSM_LANG_KOTLIN},
    {"ruby", LSM_LANG_RUBY},
    {"c", LSM_LANG_C},
    {"bash", LSM_LANG_BASH},
    {"sh", LSM_LANG_BASH},
    {"zig", LSM_LANG_ZIG},
    {"elixir", LSM_LANG_ELIXIR},
    {"haskell", LSM_LANG_HASKELL},
    {"ocaml", LSM_LANG_OCAML},
    {"objective-c", LSM_LANG_OBJC},
    {"objc", LSM_LANG_OBJC},
    {"swift", LSM_LANG_SWIFT},
    {"dart", LSM_LANG_DART},
    {"perl", LSM_LANG_PERL},
    {"groovy", LSM_LANG_GROOVY},
    {"erlang", LSM_LANG_ERLANG},
    {"r", LSM_LANG_R},
    {"html", LSM_LANG_HTML},
    {"css", LSM_LANG_CSS},
    {"scss", LSM_LANG_SCSS},
    {"yaml", LSM_LANG_YAML},
    {"toml", LSM_LANG_TOML},
    {"hcl", LSM_LANG_HCL},
    {"terraform", LSM_LANG_HCL},
    {"sql", LSM_LANG_SQL},
    {"dockerfile", LSM_LANG_DOCKERFILE},
    {"clojure", LSM_LANG_CLOJURE},
    {"f#", LSM_LANG_FSHARP},
    {"fsharp", LSM_LANG_FSHARP},
    {"julia", LSM_LANG_JULIA},
    {"vimscript", LSM_LANG_VIMSCRIPT},
    {"nix", LSM_LANG_NIX},
    {"common lisp", LSM_LANG_COMMONLISP},
    {"commonlisp", LSM_LANG_COMMONLISP},
    {"lisp", LSM_LANG_COMMONLISP},
    {"elm", LSM_LANG_ELM},
    {"fortran", LSM_LANG_FORTRAN},
    {"cuda", LSM_LANG_CUDA},
    {"cobol", LSM_LANG_COBOL},
    {"verilog", LSM_LANG_VERILOG},
    {"emacs lisp", LSM_LANG_EMACSLISP},
    {"emacslisp", LSM_LANG_EMACSLISP},
    {"json", LSM_LANG_JSON},
    {"xml", LSM_LANG_XML},
    {"markdown", LSM_LANG_MARKDOWN},
    {"makefile", LSM_LANG_MAKEFILE},
    {"cmake", LSM_LANG_CMAKE},
    {"protobuf", LSM_LANG_PROTOBUF},
    {"graphql", LSM_LANG_GRAPHQL},
    {"vue", LSM_LANG_VUE},
    {"svelte", LSM_LANG_SVELTE},
    {"meson", LSM_LANG_MESON},
    {"glsl", LSM_LANG_GLSL},
    {"ini", LSM_LANG_INI},
    {"matlab", LSM_LANG_MATLAB},
    {"mojo", LSM_LANG_MOJO},
    {"lean", LSM_LANG_LEAN},
    {"form", LSM_LANG_FORM},
    {"magma", LSM_LANG_MAGMA},
    {"wolfram", LSM_LANG_WOLFRAM},
};

#define LANG_NAME_TABLE_SIZE (sizeof(LANG_NAME_TABLE) / sizeof(LANG_NAME_TABLE[0]))

/*
 * Parse a language string (case-insensitive) to a LSMLanguage enum.
 * Returns LSM_LANG_COUNT if the string is not recognized.
 */
static LSMLanguage lang_from_string(const char *s) {
    if (!s || !s[0]) {
        return LSM_LANG_COUNT;
    }

    /* Build a lowercase copy for comparison */
    char lower[LSM_SZ_64];
    size_t i;
    for (i = 0; i < sizeof(lower) - SKIP_ONE && s[i]; i++) {
        lower[i] = (char)tolower((unsigned char)s[i]);
    }
    lower[i] = '\0';

    for (size_t j = 0; j < LANG_NAME_TABLE_SIZE; j++) {
        if (strcmp(LANG_NAME_TABLE[j].name, lower) == 0) {
            return LANG_NAME_TABLE[j].lang;
        }
    }
    return LSM_LANG_COUNT;
}

/* ── Config directory helper ─────────────────────────────────────── */

/* lsm_app_config_dir() is now in platform.c (cross-platform). */

/* ── JSON parsing ────────────────────────────────────────────────── */

/*
 * Parse extra_extensions from a yyjson object root.
 * Appends valid entries to *entries / *count (growing via realloc).
 * Project-level entries (from_project=true) are appended after global
 * entries so that a later dedup pass can prefer project values.
 *
 * Returns 0 on success, -1 on alloc failure.
 */
static int parse_extra_extensions(yyjson_val *root, lsm_userext_t **entries, int *count,
                                  const char *source_label) {
    if (!yyjson_is_obj(root)) {
        lsm_log_warn("userconfig.bad_root", "file", source_label);
        return 0;
    }

    yyjson_val *extra = yyjson_obj_get(root, "extra_extensions");
    if (!extra) {
        return 0; /* key absent — fine */
    }
    if (!yyjson_is_obj(extra)) {
        lsm_log_warn("userconfig.bad_extra_extensions", "file", source_label);
        return 0;
    }

    yyjson_obj_iter iter;
    yyjson_obj_iter_init(extra, &iter);
    yyjson_val *key;
    while ((key = yyjson_obj_iter_next(&iter)) != NULL) {
        yyjson_val *val = yyjson_obj_iter_get_val(key);

        const char *ext_str = yyjson_get_str(key);
        const char *lang_str = yyjson_get_str(val);

        if (!ext_str || !lang_str) {
            lsm_log_warn("userconfig.skip_non_string", "file", source_label);
            continue;
        }

        /* Extension must start with '.' */
        if (ext_str[0] != '.') {
            lsm_log_warn("userconfig.skip_bad_ext", "file", source_label, "ext", ext_str);
            continue;
        }

        LSMLanguage lang = lang_from_string(lang_str);
        if (lang == LSM_LANG_COUNT) {
            lsm_log_warn("userconfig.unknown_lang", "file", source_label, "lang", lang_str);
            continue; /* fail-open: skip unknown languages */
        }

        /* Grow the array */
        lsm_userext_t *tmp = realloc(*entries, (size_t)(*count + SKIP_ONE) * sizeof(lsm_userext_t));
        if (!tmp) {
            return LSM_NOT_FOUND;
        }
        *entries = tmp;

        char *ext_copy = strdup(ext_str);
        if (!ext_copy) {
            return LSM_NOT_FOUND;
        }

        (*entries)[*count].ext = ext_copy;
        (*entries)[*count].lang = lang;
        (*count)++;
    }
    return 0;
}

/*
 * Read a JSON file and parse extra_extensions from it.
 * Silently ignores missing files. Logs warnings for corrupt JSON.
 * Returns 0 on success (or absent file), -1 on alloc failure.
 */
static int load_config_file(const char *path, lsm_userext_t **entries, int *count,
                            char source_sha256[LSM_SHA256_HEX_LEN + 1]) {
    userconfig_source_digest("missing-or-unreadable", NULL, 0, source_sha256);
    FILE *f = lsm_fopen(path, "rb");
    if (!f) {
        return 0; /* file absent — silently ignore */
    }

    if (fseek(f, 0, SEEK_END) != 0) {
        (void)fclose(f);
        userconfig_source_digest("seek-error", NULL, 0, source_sha256);
        return 0;
    }
    long len = ftell(f);
    if (fseek(f, 0, SEEK_SET) != 0) {
        (void)fclose(f);
        userconfig_source_digest("seek-error", NULL, 0, source_sha256);
        return 0;
    }

    if (len <= 0 || len > MAX_CONFIG_SIZE) {
        (void)fclose(f);
        if (len > MAX_CONFIG_SIZE) {
            lsm_log_warn("userconfig.file_too_large", "path", path);
            userconfig_source_digest("oversized", NULL, 0, source_sha256);
        } else {
            userconfig_source_digest("empty", NULL, 0, source_sha256);
        }
        return 0;
    }

    char *buf = malloc((size_t)len + SKIP_ONE);
    if (!buf) {
        (void)fclose(f);
        return LSM_NOT_FOUND;
    }

    size_t nread = fread(buf, SKIP_ONE, (size_t)len, f);
    (void)fclose(f);
    if (nread > (size_t)len) {
        nread = (size_t)len;
    }
    buf[nread] = '\0';
    userconfig_source_digest("present", buf, nread, source_sha256);

    yyjson_doc *doc = yyjson_read(buf, nread, 0);
    free(buf);

    if (!doc) {
        lsm_log_warn("userconfig.corrupt_json", "path", path);
        return 0; /* corrupt JSON — silently ignore (fail-open) */
    }

    yyjson_val *root = yyjson_doc_get_root(doc);
    int rc = parse_extra_extensions(root, entries, count, path);
    yyjson_doc_free(doc);
    return rc;
}

/* ── Public API ──────────────────────────────────────────────────── */

lsm_userconfig_t *lsm_userconfig_load(const char *repo_path) {
    lsm_userconfig_t *cfg = calloc(LSM_ALLOC_ONE, sizeof(lsm_userconfig_t));
    if (!cfg) {
        return NULL;
    }

    lsm_userext_t *entries = NULL;
    int count = 0;

    /* ── Step 1: Load global config ── */
    enum { PATH_BUF_SZ = 1280 };
    const char *cfg_base = lsm_app_config_dir();
    const char *cfg_fallback = cfg_base ? cfg_base : "/tmp";
    char global_path[PATH_BUF_SZ];
    snprintf(global_path, sizeof(global_path), "%s/logan-spine-mcp/config.json", cfg_fallback);

    if (load_config_file(global_path, &entries, &count, cfg->global_source_sha256) != 0) {
        for (int i = 0; i < count; i++) {
            free(entries[i].ext);
        }
        free(entries);
        free(cfg);
        return NULL;
    }

    int global_count = count; /* entries[0..global_count) are from global */

    /* ── Step 2: Load project config ── */
    userconfig_source_digest("not-applicable", NULL, 0, cfg->project_source_sha256);
    if (repo_path && repo_path[0]) {
        char project_path[PATH_BUF_SZ];
        snprintf(project_path, sizeof(project_path), "%s/.logan-spine.json", repo_path);

        if (load_config_file(project_path, &entries, &count, cfg->project_source_sha256) != 0) {
            /* Free already-allocated entries */
            for (int i = 0; i < count; i++) {
                free(entries[i].ext);
            }
            free(entries);
            free(cfg);
            return NULL;
        }
    }

    /*
     * ── Step 3: Dedup — project entries win over global ──
     *
     * For any extension that appears in both global (indices 0..global_count)
     * and project (indices global_count..count), remove the global entry by
     * replacing it with the last global entry (order-insensitive dedup).
     */
    for (int p = global_count; p < count; p++) {
        for (int g = 0; g < global_count; g++) {
            if (entries[g].ext && strcmp(entries[g].ext, entries[p].ext) == 0) {
                /* Remove global entry: overwrite with last global entry */
                free(entries[g].ext);
                entries[g] = entries[global_count - SKIP_ONE];
                entries[global_count - SKIP_ONE].ext = NULL; /* mark as consumed */
                global_count--;
                break;
            }
        }
    }

    /*
     * Compact: remove any NULL-ext slots left by the dedup step.
     * (Those are the consumed "last global" entries.)
     */
    int write_idx = 0;
    for (int i = 0; i < count; i++) {
        if (entries[i].ext != NULL) {
            entries[write_idx++] = entries[i];
        }
    }
    count = write_idx;

    cfg->entries = entries;
    cfg->count = count;
    return cfg;
}

LSMLanguage lsm_userconfig_lookup(const lsm_userconfig_t *cfg, const char *ext) {
    if (!cfg || !ext || !ext[0]) {
        return LSM_LANG_COUNT;
    }
    for (int i = 0; i < cfg->count; i++) {
        if (cfg->entries[i].ext && strcmp(cfg->entries[i].ext, ext) == 0) {
            return cfg->entries[i].lang;
        }
    }
    return LSM_LANG_COUNT;
}

void lsm_userconfig_free(lsm_userconfig_t *cfg) {
    if (!cfg) {
        return;
    }
    for (int i = 0; i < cfg->count; i++) {
        free(cfg->entries[i].ext);
    }
    free(cfg->entries);
    free(cfg);
}
