/*
 * discover.h — File discovery, language detection, and gitignore matching.
 *
 * Provides:
 *   - Language detection from filename/extension (LSMLanguage registry)
 *   - .m file disambiguation (Objective-C vs Magma vs MATLAB)
 *   - Gitignore-style pattern parsing and matching
 *   - Recursive directory walk with hardcoded + gitignore filtering
 *
 * Depends on: foundation (platform.h for file ops), lsm.h (LSMLanguage enum)
 */
#ifndef LSM_DISCOVER_H
#define LSM_DISCOVER_H

#include <stdbool.h>
#include <stdint.h>

/* Use the existing LSMLanguage enum from extraction layer */
#include "lsm.h"

/* ── Language detection ──────────────────────────────────────────── */

/* Detect language from a filename (basename only, not full path).
 * Checks special filenames first (Makefile, CMakeLists.txt, etc.),
 * then falls back to extension-based lookup.
 * Returns LSM_LANG_COUNT if unknown. */
LSMLanguage lsm_language_for_filename(const char *filename);

/* Detect language from a file extension (including the dot, e.g. ".go").
 * Returns LSM_LANG_COUNT if unknown. */
LSMLanguage lsm_language_for_extension(const char *ext);

/* Get the human-readable name for a language enum value.
 * Returns "Unknown" for LSM_LANG_COUNT or out-of-range values. */
const char *lsm_language_name(LSMLanguage lang);

/* Disambiguate .m files by reading first 4KB of content.
 * Returns LSM_LANG_OBJC, LSM_LANG_MAGMA, or LSM_LANG_MATLAB.
 * On read failure, defaults to LSM_LANG_MATLAB. */
LSMLanguage lsm_disambiguate_m(const char *path);

/* Disambiguate .cls files by reading first 4KB of content.
 * Returns LSM_LANG_OBJECTSCRIPT_UDL if a line starts with "Class <Uppercase>",
 * otherwise LSM_LANG_APEX. On read failure, defaults to LSM_LANG_APEX. */
LSMLanguage lsm_disambiguate_cls(const char *path);

/* Disambiguate .inc files by reading first 4KB of content.
 * Returns LSM_LANG_OBJECTSCRIPT_ROUTINE if it looks like an ObjectScript
 * include (a "ROUTINE <Uppercase>" header), otherwise LSM_LANG_BITBAKE.
 * On read failure, defaults to LSM_LANG_BITBAKE. */
LSMLanguage lsm_disambiguate_inc(const char *path);

/* Detect a supported script language from a file's shebang (#!...) first line.
 * Conservative fallback used only when filename/extension detection is unknown
 * (see detect_file_language); it never overrides extension or special-filename
 * matches. Opens the file read-only and reads only a bounded first line.
 * Recognizes the interpreter *basename* (not the parent path):
 *   python / python2 / python3 / dotted versions (python3.12) -> LSM_LANG_PYTHON
 *   sh / bash / dash / ksh / zsh                              -> LSM_LANG_BASH
 *   node / nodejs                                             -> LSM_LANG_JAVASCRIPT
 *   ruby -> RUBY, perl -> PERL, php -> PHP, lua -> LUA
 * Handles direct paths, "env <interp>", "env -S <interp> <args>", and CRLF.
 * Fails closed (returns LSM_LANG_COUNT) on read error, missing/malformed
 * shebang, an embedded NUL in the first line, or an unknown interpreter. */
LSMLanguage lsm_language_from_shebang(const char *path);

/* ── Gitignore pattern matching ──────────────────────────────────── */

typedef struct lsm_gitignore lsm_gitignore_t;

/* Parse gitignore patterns from a file. Returns NULL on error (file not found, etc.).
 * Caller must call lsm_gitignore_free(). */
lsm_gitignore_t *lsm_gitignore_load(const char *path);

/* Parse gitignore patterns from a string (for testing).
 * Caller must call lsm_gitignore_free(). */
lsm_gitignore_t *lsm_gitignore_parse(const char *content);

/* Check if a relative path matches any gitignore pattern.
 * rel_path should use '/' separators. is_dir indicates if path is a directory. */
bool lsm_gitignore_matches(const lsm_gitignore_t *gi, const char *rel_path, bool is_dir);

/* Free a gitignore matcher. NULL-safe. */
void lsm_gitignore_free(lsm_gitignore_t *gi);

/* Append all patterns from src into dst. dst takes ownership of deep copies
 * of each src pattern; src is unchanged and must still be freed by the caller.
 * NULL-safe on either argument.
 * Returns true on success (or when there is nothing to merge). Returns false on
 * allocation failure, in which case dst is left exactly as it was (atomic) — no
 * partial merge — so a failed merge degrades to "as if src was absent". */
bool lsm_gitignore_merge(lsm_gitignore_t *dst, const lsm_gitignore_t *src);

/* ── Directory skip / suffix filters ─────────────────────────────── */

/* Index mode controls filtering aggressiveness.
 * IMPORTANT: these values MUST match pipeline.h exactly.  A previous
 * mismatch (this header had FAST=1, pipeline.h has FAST=2) caused
 * fast-mode filtering to silently no-op depending on include order —
 * the pipeline passed value 2, discover.c compared against 1, and no
 * files got filtered. */
#ifndef LSM_INDEX_MODE_T_DEFINED
#define LSM_INDEX_MODE_T_DEFINED
typedef enum {
    LSM_MODE_FULL = 0,     /* parse everything supported */
    LSM_MODE_MODERATE = 1, /* aggressive filtering + similarity/semantic edges */
    LSM_MODE_FAST = 2,     /* aggressive filtering + no similarity/semantic edges */
} lsm_index_mode_t;
#endif

/* Check if a directory name should always be skipped (e.g. .git, node_modules).
 * Only checks the basename, not the full path. */
bool lsm_should_skip_dir(const char *dirname, lsm_index_mode_t mode);

/* Check if a file has a suffix that should be skipped (e.g. .pyc, .png). */
bool lsm_has_ignored_suffix(const char *filename, lsm_index_mode_t mode);

/* Check if a specific filename should be skipped in fast mode (e.g. LICENSE, go.sum). */
bool lsm_should_skip_filename(const char *filename, lsm_index_mode_t mode);

/* Check if a path matches fast-mode substring patterns (e.g. .d.ts, .pb.go). */
bool lsm_matches_fast_pattern(const char *filename, lsm_index_mode_t mode);

/* ── File discovery ──────────────────────────────────────────────── */

typedef struct {
    char *path;           /* absolute path (heap-allocated) */
    char *rel_path;       /* relative to repo root (heap-allocated) */
    LSMLanguage language; /* detected language */
    int64_t size;         /* file size in bytes */
} lsm_file_info_t;

typedef struct {
    lsm_index_mode_t mode;   /* LSM_MODE_FULL or LSM_MODE_FAST */
    const char *ignore_file; /* path to .lsmignore file, or NULL */
    int64_t max_file_size;   /* 0 = no limit */
} lsm_discover_opts_t;

typedef enum {
    LSM_DISCOVER_ERROR = -1,
    LSM_DISCOVER_OK = 0,
    LSM_DISCOVER_LIMIT_EXCEEDED = 1,
} lsm_discover_status_t;

/* Walk a repository directory tree and discover all source files.
 * Applies hardcoded filters, gitignore patterns, and language detection.
 * Returns 0 on success, -1 on error.
 * Caller must call lsm_discover_free() on the results. */
int lsm_discover(const char *repo_path, const lsm_discover_opts_t *opts, lsm_file_info_t **out,
                 int *count);

/* Apply the exact same full discovery/filter policy without retaining a file
 * array. Stops before counting more than max_files and performs no per-file
 * allocation. deadline_ms is an absolute lsm_now_ms() deadline; zero disables
 * it. Returns LIMIT_EXCEEDED when at least max_files + 1 indexable files exist,
 * ERROR on traversal/deadline/allocation failure, or OK with the exact count. */
lsm_discover_status_t lsm_discover_count_bounded(const char *repo_path,
                                                 const lsm_discover_opts_t *opts, int max_files,
                                                 uint64_t deadline_ms, int *count_out);

/* Like lsm_discover(), but also reports the directory subtrees that were
 * skipped during the walk (hardcoded ALWAYS_SKIP/FAST_SKIP dirs + gitignore
 * matches), so callers can surface which subtrees were dropped (#411).
 * On success, *excluded_out receives a heap-allocated array of strdup'd
 * relative directory paths and *excluded_count_out its length; the caller
 * owns it and must free via lsm_discover_free_excluded(). Pass NULL for
 * excluded_out (and/or excluded_count_out) to discard the list — the internal
 * accumulator is freed in that case (no leak).
 * Returns 0 on success, -1 on error. */
int lsm_discover_ex(const char *repo_path, const lsm_discover_opts_t *opts, lsm_file_info_t **out,
                    int *count, char ***excluded_out, int *excluded_count_out);

/* One deliberately-not-indexed file (#963): an individual file dropped by an
 * ignore mechanism during the walk (its parent directory was NOT excluded —
 * whole subtrees are reported separately as excluded dirs). BY DESIGN, not a
 * failure. */
typedef struct {
    char *rel_path; /* heap-allocated, relative to repo root */
    char *reason;   /* heap-allocated: "gitignore" | "lsmignore" |
                     * "skip-list" | "ignored-suffix" | "fast-pattern" |
                     * "size-cap" */
} lsm_ignored_file_t;

/* Stored per-file ignore entries are capped (the walk still counts ALL of
 * them in *ignored_total_out, so truncation is always explicit, never
 * silent). Whole excluded subtrees stay exhaustive via excluded_out. */
enum { LSM_DISCOVER_IGNORED_CAP = 2000 };

/* Like lsm_discover_ex(), but additionally reports the individual files that
 * ignore rules dropped (#963 "purposely not indexed"). *ignored_out receives
 * a heap array (caller frees via lsm_discover_free_ignored),
 * *ignored_count_out its stored length (<= LSM_DISCOVER_IGNORED_CAP), and
 * *ignored_total_out the TOTAL number of ignored files seen. Pass NULL to
 * skip the collection entirely. */
int lsm_discover_ex2(const char *repo_path, const lsm_discover_opts_t *opts, lsm_file_info_t **out,
                     int *count, char ***excluded_out, int *excluded_count_out,
                     lsm_ignored_file_t **ignored_out, int *ignored_count_out,
                     int *ignored_total_out);

/* Free an array of file info results. NULL-safe. */
void lsm_discover_free(lsm_file_info_t *files, int count);

/* Free the excluded-directory list returned by lsm_discover_ex(). NULL-safe. */
void lsm_discover_free_excluded(char **excluded, int count);

/* Free the ignored-file list returned by lsm_discover_ex2(). NULL-safe. */
void lsm_discover_free_ignored(lsm_ignored_file_t *ignored, int count);

#endif /* LSM_DISCOVER_H */
