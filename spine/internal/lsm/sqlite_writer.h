#ifndef LSM_SQLITE_WRITER_H
#define LSM_SQLITE_WRITER_H

#include <stdint.h>

// --- Input structs (flat, borrowed strings) ---

typedef struct {
    int64_t id; // sequential ID (1..N), assigned by Go
    const char *project;
    const char *label;
    const char *name;
    const char *qualified_name;
    const char *file_path;
    int start_line;
    int end_line;
    const char *properties; // JSON string
} LSMDumpNode;

typedef struct {
    int64_t id; // sequential ID (1..M), assigned by Go
    const char *project;
    int64_t source_id; // final sequential ID (1..N)
    int64_t target_id; // final sequential ID (1..N)
    const char *type;
    const char *properties; // JSON string
    const char *url_path;   // extracted from properties by Go (for idx_edges_url_path)
    const char *local_name; // for IMPORTS edges: the UNESCAPED
                            // json_extract(properties,'$.local_name') value; ""/NULL
                            // otherwise. Feeds sqlite_autoindex_edges_1 — must match
                            // what SQLite computes for the local_name_gen column or
                            // integrity_check reports the row missing from the index.
} LSMDumpEdge;

typedef struct {
    int64_t node_id; // final sequential ID (matches nodes.id)
    const char *project;
    const uint8_t *vector; // int8-quantized vector blob
    int vector_len;        // length in bytes (e.g. 256 for d=256)
} LSMDumpVector;

typedef struct {
    int64_t id; // sequential ID (1..T)
    const char *project;
    const char *token;     // the token string
    const uint8_t *vector; // int8-quantized enriched RI vector blob
    int vector_len;        // length in bytes (e.g. 256 for d=256)
    float idf;             // inverse document frequency weight
} LSMDumpTokenVec;

// --- Public API ---

// Write a complete SQLite .db file from sorted in-memory data.
// Constructs B-tree pages directly — no SQL parser, no INSERTs.
// Returns 0 on success, non-zero on error.
// vectors/vector_count and token_vecs/token_vec_count may be NULL/0.
int lsm_write_db(const char *path, const char *project, const char *root_path,
                 const char *indexed_at, LSMDumpNode *nodes, int node_count, LSMDumpEdge *edges,
                 int edge_count, LSMDumpVector *vectors, int vector_count,
                 LSMDumpTokenVec *token_vecs, int token_vec_count);

// --- Streaming writer: incremental bulk node-table append ---
//
// Lets the indexer flush node rows (including heavy `properties`) to the DB in
// batches, mid-pipeline, freeing heavy memory — while preserving the direct-page
// bulk write (no per-row INSERTs). The nodes table is built across append calls
// via a persistent page builder; everything else (edges, vectors, metadata,
// indexes, sqlite_master) is written at finalize. lsm_write_db() above is a
// one-shot wrapper over this API (open -> append all nodes -> finalize) and
// produces byte-identical output.
//
// Usage: w = lsm_writer_open(path);
//        lsm_writer_append_nodes(w, batch, n) x N  (ascending, contiguous ids);
//        lsm_writer_finalize(w, ...);   // consumes + frees w, closes the file.
typedef struct lsm_db_writer lsm_db_writer_t;

lsm_db_writer_t *lsm_writer_open(const char *path);

// Append a batch of node records. Heavy `properties` are consumed here, so the
// caller may free them after this returns. Node ids must be ascending and
// contiguous across the whole sequence of append calls. Returns 0 on success.
int lsm_writer_append_nodes(lsm_db_writer_t *w, const LSMDumpNode *nodes, int count);

// Finalize: build the nodes-table interior, write edges/vectors/token_vectors,
// metadata, all indexes, and sqlite_master + header. The node/edge/vector arrays
// supply the (light) columns the index builders sort on; node `properties` are
// NOT read here (already written during append). Frees w and closes the file.
int lsm_writer_finalize(lsm_db_writer_t *w, const char *project, const char *root_path,
                        const char *indexed_at, LSMDumpNode *nodes, int node_count,
                        LSMDumpEdge *edges, int edge_count, LSMDumpVector *vectors,
                        int vector_count, LSMDumpTokenVec *token_vecs, int token_vec_count);

#endif // LSM_SQLITE_WRITER_H
