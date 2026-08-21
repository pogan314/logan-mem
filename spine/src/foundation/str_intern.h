/*
 * str_intern.h — String interning pool.
 *
 * Deduplicates strings: identical strings share a single allocation.
 * Returns stable pointers — safe to compare by pointer equality after interning.
 *
 * Uses an arena for string storage (bulk free) + hash table for dedup lookup.
 */
#ifndef LSM_STR_INTERN_H
#define LSM_STR_INTERN_H

#include <stddef.h>
#include <stdint.h>

typedef struct LSMInternPool LSMInternPool;

/* Create a new intern pool. */
LSMInternPool *lsm_intern_create(void);

/* Free the pool and all interned strings. */
void lsm_intern_free(LSMInternPool *pool);

/* Intern a NUL-terminated string. Returns a stable pointer.
 * The same input always returns the same pointer. */
const char *lsm_intern(LSMInternPool *pool, const char *s);

/* Intern a string of known length. */
const char *lsm_intern_n(LSMInternPool *pool, const char *s, size_t len);

/* Number of unique strings in the pool. */
uint32_t lsm_intern_count(const LSMInternPool *pool);

/* Total bytes stored (unique strings only). */
size_t lsm_intern_bytes(const LSMInternPool *pool);

#endif /* LSM_STR_INTERN_H */
