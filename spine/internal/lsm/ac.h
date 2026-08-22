#ifndef LSM_AC_H
#define LSM_AC_H

#include <stdint.h>

// Forward declaration — full struct in ac.c
typedef struct LSMAutomaton LSMAutomaton;

// Input for batch LZ4 scanning.
typedef struct {
    const char *data;
    int compressed_len;
    int original_len;
} LSMLz4Entry;

// Output for batch LZ4 scanning.
typedef struct {
    int file_index;
    uint64_t bitmask;
} LSMLz4Match;

// Output for batch name scanning.
typedef struct {
    int name_index;
    int pattern_id;
} LSMMatchResult;

// Build an Aho-Corasick automaton from patterns.
LSMAutomaton *lsm_ac_build(const char **patterns, const int *lengths, int count,
                           const uint8_t *alpha_map, int alpha_size);
void lsm_ac_free(LSMAutomaton *ac);

// Single-text scanning (returns bitmask of matched pattern IDs).
uint64_t lsm_ac_scan_bitmask(const LSMAutomaton *ac, const char *text, int text_len);

// LZ4-compressed scanning.
uint64_t lsm_ac_scan_lz4_bitmask(const LSMAutomaton *ac, const char *compressed, int compressed_len,
                                 int original_len);
int lsm_ac_scan_lz4_batch(const LSMAutomaton *ac, const LSMLz4Entry *entries, int num_entries,
                          LSMLz4Match *out_matches, int max_matches);

// Batch name scanning.
int lsm_ac_scan_batch(const LSMAutomaton *ac, const char *names_buf, const int *name_offsets,
                      const int *name_lengths, int num_names, LSMMatchResult *out_matches,
                      int max_matches);

// Introspection.
int lsm_ac_num_states(const LSMAutomaton *ac);
int lsm_ac_num_patterns(const LSMAutomaton *ac);
int lsm_ac_table_bytes(const LSMAutomaton *ac);

#endif // LSM_AC_H
