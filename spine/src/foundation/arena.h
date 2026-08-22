/*
 * arena.h — Bump allocator with block-based growth.
 *
 * All memory is freed at once via lsm_arena_destroy(). Individual frees are
 * not supported — this is by design for per-file extraction where all data
 * has the same lifetime.
 *
 * Restructured from internal/lsm/arena.h for the pure C rewrite.
 * New additions: lsm_arena_reset() for reuse without realloc.
 */
#ifndef LSM_ARENA_H
#define LSM_ARENA_H

#include <stddef.h>
#include <stdarg.h>

#define LSM_ARENA_MAX_BLOCKS 256
#define LSM_ARENA_DEFAULT_BLOCK_SIZE ((size_t)64 * 1024) /* 64KB */

typedef struct {
    char *blocks[LSM_ARENA_MAX_BLOCKS];
    size_t block_sizes[LSM_ARENA_MAX_BLOCKS]; /* per-block sizes (for stats) */
    int nblocks;
    size_t block_size;  /* current block capacity */
    size_t used;        /* bytes used in current block */
    size_t total_alloc; /* cumulative bytes allocated (for stats) */
} LSMArena;

/* Initialize arena with default block size. */
void lsm_arena_init(LSMArena *a);

/* Initialize arena with a custom initial block size. */
void lsm_arena_init_sized(LSMArena *a, size_t block_size);

/* Allocate n bytes (8-byte aligned). Returns NULL on OOM. */
void *lsm_arena_alloc(LSMArena *a, size_t n);

/* Allocate n bytes, zero-initialized. */
void *lsm_arena_calloc(LSMArena *a, size_t n);

/* Duplicate a NUL-terminated string. */
char *lsm_arena_strdup(LSMArena *a, const char *s);

/* Duplicate a string of known length, NUL-terminate. */
char *lsm_arena_strndup(LSMArena *a, const char *s, size_t len);

/* sprintf into arena memory. */
char *lsm_arena_sprintf(LSMArena *a, const char *fmt, ...) __attribute__((format(printf, 2, 3)));

/* Reset arena for reuse: keeps first block, frees the rest. */
void lsm_arena_reset(LSMArena *a);

/* Free all blocks. Arena is zeroed after this. */
void lsm_arena_destroy(LSMArena *a);

/* Return total bytes allocated (for diagnostics). */
size_t lsm_arena_total(const LSMArena *a);

#endif /* LSM_ARENA_H */
