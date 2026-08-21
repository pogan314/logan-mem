/*
 * constants.h — Project-wide named constants.
 *
 * Eliminates magic numbers flagged by readability-magic-numbers.
 * Every literal integer/float in source should reference a named constant.
 */
#ifndef LSM_CONSTANTS_H
#define LSM_CONSTANTS_H

/* ── Allocation counts ───────────────────────────────────────── */
enum { LSM_ALLOC_ONE = 1 }; /* calloc(LSM_ALLOC_ONE, sizeof(T)) */

/* ── Byte / character constants ──────────────────────────────── */
enum {
    LSM_BYTE_RANGE = 256, /* full byte range 0x00–0xFF */
    LSM_QUOTE_PAIR = 2,   /* two quote characters (open + close) */
    LSM_QUOTE_OFFSET = 1, /* skip opening quote */
};

/* ── Size units (powers of 2) ────────────────────────────────── */
enum {
    LSM_SZ_2 = 2,
    LSM_SZ_3 = 3,
    LSM_SZ_4 = 4,
    LSM_SZ_5 = 5,
    LSM_SZ_6 = 6,
    LSM_SZ_7 = 7,
    LSM_SZ_8 = 8,
    LSM_SZ_16 = 16,
    LSM_SZ_32 = 32,
    LSM_SZ_64 = 64,
    LSM_SZ_128 = 128,
    LSM_SZ_256 = 256,
    LSM_SZ_512 = 512,
    LSM_SZ_1K = 1024,
    LSM_SZ_2K = 2048,
    LSM_SZ_4K = 4096,
    LSM_SZ_8K = 8192,
    LSM_SZ_16K = 16384,
    LSM_SZ_32K = 32768,
    LSM_SZ_64K = 65536,
};

/* ── Numeric bases and common factors ────────────────────────── */
enum {
    LSM_DECIMAL_BASE = 10,
    LSM_HEX_BASE = 16,
    LSM_PERCENT = 100,
};

/* ── Tree-sitter field name helper ───────────────────────────── */
/* Usage: ts_node_child_by_field_name(node, TS_FIELD("callee"))
 * Expands to: ts_node_child_by_field_name(node, TS_FIELD("callee"))
 * The sizeof includes the NUL terminator, so subtract 1. */
#define TS_FIELD(name) (name), (uint32_t)(sizeof(name) - SKIP_ONE)

/* ── Tree-sitter line offset ─────────────────────────────────── */
/* ts_node row is 0-based; source lines are 1-based. */
enum { TS_LINE_OFFSET = 1 };

/* Common offset constants. */

/* Common offset constants. */

/* ── Sentinel values ─────────────────────────────────────────── */
enum {
    LSM_NOT_FOUND = -1, /* search miss, invalid index */
    LSM_INIT_DONE = 1,  /* initialization flag */
};

/* ── Default pagination limits ───────────────────────────────── */
/* Default page size for search_graph and the underlying store-layer search.
 * Responses land in an LLM agent's context window, so the default favors a
 * cheap first page (~50 TOON rows ≈ 1.5K tokens) over raw coverage; the
 * response always carries 'total' and 'has_more', and agents page via
 * offset+limit or narrow with label/file_pattern when has_more is true. */
enum { LSM_DEFAULT_SEARCH_LIMIT = 50 };

/* ── Time conversion factors ─────────────────────────────────── */
#define LSM_NSEC_PER_SEC 1000000000ULL
#define LSM_USEC_PER_SEC 1000000ULL
#define LSM_MSEC_PER_SEC 1000ULL
#define LSM_NSEC_PER_USEC 1000ULL
#define LSM_NSEC_PER_MSEC 1000000ULL

/* ── Common string/buffer sizes ──────────────────────────────── */
enum {
    LSM_SMALL_BUF = 3,   /* small scratch buffers */
    LSM_NAME_BUF = 4,    /* name buffer slots */
    LSM_PATH_MAX = 1024, /* path buffer size */
    LSM_LINE_BUF = 512,  /* line read buffer */
};

/* Common offset constants (used across many files). */
enum { SKIP_ONE = 1, PAIR_LEN = 2 };

/* ── Label allowlists for SQL ────────────────────────────────────
 * SQL mirror of lsm_label_is_type_like() (internal/lsm/helpers.c). That
 * function is documented as the single source of truth for type-like labels
 * "instead of scattering `|| strcmp(label,\"Struct\")==0` across the tree" —
 * but a SQL string literal cannot call it, so several queries hardcoded
 * ('Function','Method','Class') and silently stopped matching the moment
 * Struct/Interface/Enum/Type/Trait began being emitted.
 *
 * Use these instead of inlining a label list. tests/test_store_nodes.c pins
 * them against lsm_label_is_type_like(), so adding a type-like label there
 * without updating these fails CI rather than quietly shrinking query results. */
#define LSM_SQL_TYPE_LIKE_LABELS "'Class','Struct','Interface','Enum','Type','Trait'"
#define LSM_SQL_CALLABLE_LABELS "'Function','Method'"
#define LSM_SQL_CALLABLE_OR_TYPE_LABELS LSM_SQL_CALLABLE_LABELS "," LSM_SQL_TYPE_LIKE_LABELS

#endif /* LSM_CONSTANTS_H */
