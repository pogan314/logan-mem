/* compact_out.h — TOON (Token-Oriented Object Notation) emission helpers.
 *
 * Tool responses are consumed by LLM agents, where every byte is context
 * tokens. TOON encodes the same data as JSON but declares tabular fields
 * once in a header and streams rows line by line, cutting 40-60% of tokens
 * on homogeneous result sets at equal-or-better retrieval accuracy
 * (toonformat.dev/guide/benchmarks). Emitters here cover the subset we
 * emit: scalar key-value lines and flat tables with explicit [N] lengths.
 *
 * Quoting: a cell/scalar is double-quoted iff it is empty, has leading or
 * trailing whitespace, contains a comma, quote, newline, or CR, or would
 * read as a non-string literal (true/false/null/number). Quotes and
 * backslashes are escaped JSON-style; newlines become \n.
 */
#ifndef LSM_MCP_COMPACT_OUT_H
#define LSM_MCP_COMPACT_OUT_H

#include <stdbool.h>
#include <stddef.h>

/* Minimal growing string buffer (OOM-safe: sticky flag, finish returns NULL). */
typedef struct {
    char *buf;
    size_t len;
    size_t cap;
    bool oom;
} lsm_sb_t;

void lsm_sb_init(lsm_sb_t *sb);
void lsm_sb_append_n(lsm_sb_t *sb, const char *s, size_t n);
void lsm_sb_append(lsm_sb_t *sb, const char *s);
/* Returns the heap buffer (caller frees) and resets sb. NULL on OOM. */
char *lsm_sb_finish(lsm_sb_t *sb);
void lsm_sb_free(lsm_sb_t *sb);

/* `key: value` scalar lines (top-level, no indent). */
void lsm_tree_scalar_str(lsm_sb_t *sb, const char *key, const char *val);
void lsm_tree_scalar_int(lsm_sb_t *sb, const char *key, long long v);
void lsm_tree_scalar_bool(lsm_sb_t *sb, const char *key, bool v);

/* `key[n]{col1,col2,...}:` table header; rows follow at 2-space indent. */
void lsm_tree_table_header(lsm_sb_t *sb, const char *key, int n, const char *const *cols,
                           int ncols);

/* Row cells: call row_begin, then cell_* per column (first=true for the
 * first cell), then row_end. Empty/NULL strings emit as empty cells. */
void lsm_tree_row_begin(lsm_sb_t *sb);
void lsm_tree_cell_str(lsm_sb_t *sb, const char *val, bool first);
void lsm_tree_cell_int(lsm_sb_t *sb, long long v, bool first);
void lsm_tree_cell_real(lsm_sb_t *sb, double v, bool first);
void lsm_tree_cell_bool(lsm_sb_t *sb, bool v, bool first);
void lsm_tree_row_end(lsm_sb_t *sb);

#endif /* LSM_MCP_COMPACT_OUT_H */
