/*
 * arena.c — Bump allocator implementation.
 *
 * Restructured from internal/lsm/arena.c with additions:
 *   - lsm_arena_init_sized() for custom block sizes
 *   - lsm_arena_calloc() for zero-initialized allocations
 *   - lsm_arena_reset() for reuse without full destroy
 *   - lsm_arena_total() for allocation tracking
 *
 * Arena blocks use malloc/free (= mimalloc in production builds).
 */
#include "arena.h"
#include "foundation/constants.h"

enum { ARENA_ALIGN = 7, ARENA_GROW_OK = 1 };
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>

void lsm_arena_init(LSMArena *a) {
    lsm_arena_init_sized(a, LSM_ARENA_DEFAULT_BLOCK_SIZE);
}

void lsm_arena_init_sized(LSMArena *a, size_t block_size) {
    memset(a, 0, sizeof(*a));
    if (block_size < LSM_SZ_64) {
        block_size = LSM_SZ_64; /* minimum sanity */
    }
    a->block_size = block_size;
    a->blocks[0] = (char *)malloc(block_size);
    if (a->blocks[0]) {
        a->block_sizes[0] = block_size;
        a->nblocks = SKIP_ONE;
    }
}

static int arena_grow(LSMArena *a, size_t min_size) {
    if (a->nblocks >= LSM_ARENA_MAX_BLOCKS) {
        return 0;
    }
    size_t new_size = a->block_size * PAIR_LEN;
    if (new_size < min_size) {
        new_size = min_size;
    }
    char *block = (char *)malloc(new_size);
    if (!block) {
        return 0;
    }
    a->blocks[a->nblocks] = block;
    a->block_sizes[a->nblocks] = new_size;
    a->nblocks++;
    a->block_size = new_size;
    a->used = 0;
    return ARENA_GROW_OK;
}

void *lsm_arena_alloc(LSMArena *a, size_t n) {
    if (!a || n == 0) {
        return NULL;
    }
    /* 8-byte alignment */
    n = (n + ARENA_ALIGN) & ~(size_t)ARENA_ALIGN;
    if (a->nblocks == 0) {
        return NULL;
    }
    if (a->used + n > a->block_size) {
        if (!arena_grow(a, n)) {
            return NULL;
        }
    }
    char *ptr = a->blocks[a->nblocks - SKIP_ONE] + a->used;
    a->used += n;
    a->total_alloc += n;
    return ptr;
}

void *lsm_arena_calloc(LSMArena *a, size_t n) {
    void *p = lsm_arena_alloc(a, n);
    if (p) {
        memset(p, 0, n);
    }
    return p;
}

char *lsm_arena_strdup(LSMArena *a, const char *s) {
    if (!s) {
        return NULL;
    }
    size_t len = strlen(s);
    char *dst = (char *)lsm_arena_alloc(a, len + SKIP_ONE);
    if (dst) {
        memcpy(dst, s, len + SKIP_ONE);
    }
    return dst;
}

char *lsm_arena_strndup(LSMArena *a, const char *s, size_t len) {
    if (!s) {
        return NULL;
    }
    char *dst = (char *)lsm_arena_alloc(a, len + SKIP_ONE);
    if (dst) {
        memcpy(dst, s, len);
        dst[len] = '\0';
    }
    return dst;
}

char *lsm_arena_sprintf(LSMArena *a, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int needed = vsnprintf(NULL, 0, fmt, args);
    va_end(args);
    if (needed < 0) {
        return NULL;
    }

    char *dst = (char *)lsm_arena_alloc(a, (size_t)needed + SKIP_ONE);
    if (!dst) {
        return NULL;
    }

    va_start(args, fmt);
    vsnprintf(dst, (size_t)needed + SKIP_ONE, fmt, args);
    va_end(args);
    return dst;
}

void lsm_arena_reset(LSMArena *a) {
    /* Keep first block, free the rest */
    for (int i = SKIP_ONE; i < a->nblocks; i++) {
        free(a->blocks[i]);
        a->blocks[i] = NULL;
        a->block_sizes[i] = 0;
    }
    if (a->nblocks > SKIP_ONE) {
        a->nblocks = SKIP_ONE;
    }
    a->used = 0;
    a->total_alloc = 0;
    /* Reset block_size to match surviving block — prevents overflow if
     * block_size grew during previous allocations (e.g., LSM_SZ_128 → LSM_SZ_256). */
    if (a->nblocks == SKIP_ONE) {
        a->block_size = a->block_sizes[0];
    }
}

void lsm_arena_destroy(LSMArena *a) {
    for (int i = 0; i < a->nblocks; i++) {
        free(a->blocks[i]);
    }
    memset(a, 0, sizeof(*a));
}

size_t lsm_arena_total(const LSMArena *a) {
    return a->total_alloc;
}
