/*
 * compat_regex.h — Portable regular expression API.
 *
 * POSIX: direct wrappers around <regex.h> (regcomp, regexec, regfree).
 * Windows: TODO — vendor TRE regex or use a C++ wrapper around <regex>.
 *
 * Uses our own types so callers never include <regex.h> directly.
 */
#ifndef LSM_COMPAT_REGEX_H
#define LSM_COMPAT_REGEX_H

#include "foundation/constants.h"
#include <stddef.h>

/* ── Flags ────────────────────────────────────────────────────── */

#define LSM_REG_EXTENDED 1
#define LSM_REG_ICASE 2
#define LSM_REG_NOSUB 4
#define LSM_REG_NEWLINE 8

/* ── Error codes ──────────────────────────────────────────────── */

#define LSM_REG_OK 0
#define LSM_REG_NOMATCH (-1)

/* ── Types ────────────────────────────────────────────────────── */

/* Opaque regex handle — sized to hold the platform's regex_t. */
typedef struct {
    /* LSM_SZ_256 bytes should be large enough for any platform's regex_t.
     * POSIX regex_t is typically 48-LSM_SZ_64 bytes; TRE is ~80 bytes. */
    char opaque[LSM_SZ_256];
} lsm_regex_t;

typedef struct {
    int rm_so; /* byte offset of match start, -1 if no match */
    int rm_eo; /* byte offset past match end */
} lsm_regmatch_t;

/* ── Functions ────────────────────────────────────────────────── */

/* Compile a regular expression. Returns LSM_REG_OK on success, non-zero on error. */
int lsm_regcomp(lsm_regex_t *r, const char *pattern, int flags);

/* Execute compiled regex against str. nmatch/matches may be 0/NULL.
 * eflags: 0 or combination of platform-specific exec flags.
 * Returns LSM_REG_OK on match, LSM_REG_NOMATCH on no match. */
int lsm_regexec(const lsm_regex_t *r, const char *str, int nmatch, lsm_regmatch_t *matches,
                int eflags);

/* Free compiled regex. */
void lsm_regfree(lsm_regex_t *r);

#endif /* LSM_COMPAT_REGEX_H */
