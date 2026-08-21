/*
 * str_util.h — Safe string operations.
 *
 * All functions that return char* allocate via the provided arena
 * (no malloc, no free needed).
 */
#ifndef LSM_STR_UTIL_H
#define LSM_STR_UTIL_H

#include "arena.h"
#include <stdbool.h>
#include <stddef.h>

/* Join two path components with '/'. Handles trailing/leading slashes. */
char *lsm_path_join(LSMArena *a, const char *base, const char *name);

/* Join N path components. parts is an array of N strings. */
char *lsm_path_join_n(LSMArena *a, const char **parts, int n);

/* Get the file extension (without dot). Returns "" if none. */
const char *lsm_path_ext(const char *path);

/* Get the base name (after last '/'). Returns path if no '/'. */
const char *lsm_path_base(const char *path);

/* Get the directory part (before last '/'). Returns "." if no '/'. */
char *lsm_path_dir(LSMArena *a, const char *path);

/* Check if string starts with prefix. */
bool lsm_str_starts_with(const char *s, const char *prefix);

/* Check if string ends with suffix. */
bool lsm_str_ends_with(const char *s, const char *suffix);

/* Check if string contains substring. */
bool lsm_str_contains(const char *s, const char *sub);

/* Convert to lowercase (arena-allocated copy). */
char *lsm_str_tolower(LSMArena *a, const char *s);

/* Replace all occurrences of 'from' char with 'to' char (arena copy). */
char *lsm_str_replace_char(LSMArena *a, const char *s, char from, char to);

/* Strip file extension: "foo.go" → "foo" (arena copy). */
char *lsm_str_strip_ext(LSMArena *a, const char *path);

/* Split string by delimiter. Returns arena-allocated array + count.
 * The array itself and all substrings are arena-allocated. */
char **lsm_str_split(LSMArena *a, const char *s, char delim, int *out_count);

/* Validate a string is safe for shell interpolation inside single quotes.
 * Rejects: ' " ; | & $ ` < > \n \r \0 (embedded NULs via len check).
 * The Windows search path wraps shell args in cmd.exe-level "powershell -Command
 * \"...'%s'...\"", so " can close the cmd.exe outer quote even if PowerShell's
 * single quotes hold; < > would then become cmd.exe redirection (file-write
 * primitive). Blocking these unconditionally hardens both POSIX and Windows.
 * Returns true if safe, false if the string contains shell metacharacters. */
bool lsm_validate_shell_arg(const char *s);

/* Validate a filesystem path that will be interpolated into a shell command.
 * Everything lsm_validate_shell_arg rejects, plus the cmd.exe expansion
 * metacharacters % ! ^ on Windows: a path reaching `git -C "%s"` through
 * cmd.exe would otherwise get %VAR% / delayed-!VAR! expansion applied to it.
 *
 * Use this — not lsm_validate_shell_arg — for every path that crosses into a
 * shell command, so the three git shell-out sites cannot drift apart again.
 * Returns true if safe. */
bool lsm_validate_shell_path_arg(const char *path);

/* Validate a project name is safe for file path construction.
 * Allows: alphanumeric, dash, underscore, dot (but not leading dot or dot-dot).
 * Rejects: path separators (/ \), directory traversal (..), and control chars.
 * Returns true if safe, false if the name could escape the cache directory. */
bool lsm_validate_project_name(const char *name);

/* Safe snprintf append: clamps offset to prevent buffer overflow on truncation.
 * When snprintf truncates, it returns what it WOULD have written, which can make
 * offset > bufsize. Next call: bufsize - offset wraps unsigned → huge → overflow.
 * This macro guards against that by checking bounds before writing and clamping after.
 *
 * Usage: LSM_SNPRINTF_APPEND(buf, sizeof(buf), off, "fmt %s", arg);
 * Requires: <stdio.h> included by caller. */
#define LSM_SNPRINTF_APPEND(buf, sz, off, ...)                                       \
    do {                                                                             \
        if ((off) >= 0 && (off) < (int)(sz)) {                                       \
            int _cbm_r = snprintf((buf) + (off), (sz) - (size_t)(off), __VA_ARGS__); \
            if (_cbm_r > 0)                                                          \
                (off) += _cbm_r;                                                     \
            if ((off) >= (int)(sz))                                                  \
                (off) = (int)(sz) - 1;                                               \
        }                                                                            \
    } while (0)

/* Escape a string for safe embedding in JSON: escapes " \ and control chars.
 * Writes into buf (including NUL). Returns number of chars written (excl NUL).
 * If buf is too small, output is truncated but always NUL-terminated. */
int lsm_json_escape(char *buf, int bufsize, const char *src);

#endif /* LSM_STR_UTIL_H */
