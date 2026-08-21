/*
 * hash_table.h — String → void* hash table.
 *
 * Public API unchanged across implementation rewrites. As of v2 the
 * internals are Verstable (https://github.com/JacksonAllan/Verstable),
 * a 2024 state-of-the-art open-addressing hash table with quadratic
 * probing + per-bucket 4-bit hash fragments. The struct is opaque —
 * callers MUST go through lsm_ht_create() and the API functions.
 *
 * Keys are borrowed pointers — the table does not copy or free them.
 * Callers own the key strings for the lifetime of the entry.
 */
#ifndef LSM_HASH_TABLE_H
#define LSM_HASH_TABLE_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/* Opaque — full definition lives in hash_table.c. */
typedef struct LSMHashTable LSMHashTable;

/* Create a hash table with initial capacity hint (used to pre-reserve
 * buckets and avoid early growth; 0 = library default). */
LSMHashTable *lsm_ht_create(uint32_t initial_capacity);

/* Free the hash table (does NOT free keys or values). */
void lsm_ht_free(LSMHashTable *ht);

/* Insert or update. Returns previous value (NULL if new key). */
void *lsm_ht_set(LSMHashTable *ht, const char *key, void *value);

/* Lookup. Returns NULL if not found. */
void *lsm_ht_get(const LSMHashTable *ht, const char *key);

/* Check if key exists. */
bool lsm_ht_has(const LSMHashTable *ht, const char *key);

/* Return the stored key pointer for a given lookup key, or NULL.
 * Useful when you need the canonical (heap-owned) key string
 * rather than your own local copy. */
const char *lsm_ht_get_key(const LSMHashTable *ht, const char *key);

/* Delete. Returns removed value (NULL if not found). */
void *lsm_ht_delete(LSMHashTable *ht, const char *key);

/* Number of entries. */
uint32_t lsm_ht_count(const LSMHashTable *ht);

/* Iteration: call fn(key, value, userdata) for each entry. */
typedef void (*lsm_ht_iter_fn)(const char *key, void *value, void *userdata);
void lsm_ht_foreach(const LSMHashTable *ht, lsm_ht_iter_fn fn, void *userdata);

/* Clear all entries (keeps allocated memory). */
void lsm_ht_clear(LSMHashTable *ht);

#endif /* LSM_HASH_TABLE_H */
