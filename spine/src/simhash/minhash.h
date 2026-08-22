/*
 * minhash.h — MinHash fingerprinting + LSH for near-clone detection.
 *
 * Computes K=64 MinHash signatures from AST node-type trigrams using
 * xxHash with distinct seeds.  No external dependencies — xxHash is
 * vendored.  Pure functions — thread-safe, no shared state.
 *
 * Workflow:
 *   1. lsm_minhash_compute()  — during AST extraction (per function)
 *   2. lsm_minhash_jaccard()  — pairwise similarity (K=64 agreement)
 *   3. lsm_lsh_*              — locality-sensitive hashing for O(n)
 *                               candidate generation
 */
#ifndef LSM_MINHASH_H
#define LSM_MINHASH_H

#include <stdint.h>
#include <stdbool.h>

/* Number of hash permutations (seeds).  Larger K = more accurate Jaccard
 * estimate, but more memory per function.  64 gives ±0.12 standard
 * error on Jaccard — sufficient for a 0.95 threshold. */
#define LSM_MINHASH_K 64

/* Minimum number of leaf AST tokens required to compute a fingerprint.
 * Leaf-only counting is language-agnostic: leaf nodes correspond to
 * actual source tokens (identifiers, literals, keywords, operators),
 * not grammar-internal structure that varies across parsers.
 * 30 leaf tokens ≈ BigCloneBench standard of 50 raw source tokens. */
#define LSM_MINHASH_MIN_NODES 30

/* Default Jaccard threshold for SIMILAR_TO edge emission. */
#define LSM_MINHASH_JACCARD_THRESHOLD 0.95

/* Maximum SIMILAR_TO edges per node (prevents utility function explosion). */
#define LSM_MINHASH_MAX_EDGES_PER_NODE 10

/* LSH parameters: b bands × r rows.  Threshold ≈ (1/b)^(1/r). */
#define LSM_LSH_BANDS 32
#define LSM_LSH_ROWS 2

/* ── MinHash fingerprint ─────────────────────────────────────────── */

/* A MinHash signature: K minimum hash values, one per seed. */
typedef struct {
    uint32_t values[LSM_MINHASH_K];
} lsm_minhash_t;

/* Opaque tree-sitter node — forward declared to avoid pulling in
 * tree_sitter/api.h from every consumer. */
typedef struct TSNode TSNode;

/* Compute MinHash fingerprint for a function body's AST.
 *
 * Walks the subtree rooted at `func_body`, normalises leaf node types
 * (identifiers → "I", strings → "S", numbers → "N", type annotations
 * → "T"), builds trigrams, and hashes each trigram with K seeds.
 *
 * Returns true and fills `out` on success.
 * Returns false if the body has fewer than LSM_MINHASH_MIN_NODES
 * normalised tokens (fingerprint not meaningful). */
bool lsm_minhash_compute(TSNode func_body, const char *source, int language, lsm_minhash_t *out);

/* Compute exact Jaccard similarity between two MinHash signatures.
 * Returns value in [0.0, 1.0]. */
double lsm_minhash_jaccard(const lsm_minhash_t *a, const lsm_minhash_t *b);

/* Hex encoding: K uint32 values × 8 hex chars each = 512 chars. */
enum { LSM_MINHASH_HEX_LEN = 512 };

/* Buffer size for hex-encoded fingerprint including NUL. */
enum { LSM_MINHASH_HEX_BUF = 513 };

/* JSON overhead for ,"fp":"..." wrapper (key + quotes + comma + colon). */
enum { LSM_MINHASH_JSON_OVERHEAD = 10 };
void lsm_minhash_to_hex(const lsm_minhash_t *fp, char *buf, int bufsize);

/* Decode a hex string back to a MinHash signature.
 * Returns true on success. */
bool lsm_minhash_from_hex(const char *hex, lsm_minhash_t *out);

/* ── LSH index ───────────────────────────────────────────────────── */

/* Opaque LSH index handle. */
typedef struct lsm_lsh_index lsm_lsh_index_t;

/* Entry stored in the LSH index. */
typedef struct {
    int64_t node_id;
    const lsm_minhash_t *fingerprint;
    const char *file_path;      /* for same-file tagging */
    const char *file_ext;       /* for same-language filtering */
    const char *qualified_name; /* canonical pair-ownership tie-break: node ids
                                   vary run-to-run under parallel extraction,
                                   qualified names do not (determinism) */
} lsm_lsh_entry_t;

/* Create a new LSH index. */
lsm_lsh_index_t *lsm_lsh_new(void);

/* Insert an entry into the LSH index. */
void lsm_lsh_insert(lsm_lsh_index_t *idx, const lsm_lsh_entry_t *entry);

/* Query candidates similar to the given fingerprint.
 * Returns candidate entries via `out` (caller does NOT free the array
 * — it is owned by the index).  Sets `count`.
 * NOT thread-safe: uses index-internal result buffer. */
void lsm_lsh_query(const lsm_lsh_index_t *idx, const lsm_minhash_t *fp,
                   const lsm_lsh_entry_t ***out, int *count);

/* Thread-safe variant: writes candidates into caller-provided buffer.
 * `out_buf` must have room for at least `out_cap` pointers.
 * Returns the actual candidate count (may exceed out_cap — result is truncated). */
int lsm_lsh_query_into(const lsm_lsh_index_t *idx, const lsm_minhash_t *fp,
                       const lsm_lsh_entry_t **out_buf, int out_cap);

/* Free the LSH index and all internal storage. */
void lsm_lsh_free(lsm_lsh_index_t *idx);

#endif /* LSM_MINHASH_H */
