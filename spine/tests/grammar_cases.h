/*
 * grammar_cases.h — Shared per-language fixture table.
 *
 * One minimal source fixture per supported grammar. Shared by:
 *   - test_grammar_regression.c : EXTRACTION-level breadth (lsm_extract_file with
 *     an explicit language → defs/labels). Bypasses discover, so it covers every
 *     grammar including those whose file extension collides with another's.
 *   - test_lang_contract.c      : GRAPH-level breadth (full pipeline index → the
 *     fixture's defs must reach the graph as nodes).
 *
 * Keep this the single source of truth for per-grammar fixtures.
 */
#ifndef LSM_TEST_GRAMMAR_CASES_H
#define LSM_TEST_GRAMMAR_CASES_H

#include "lsm.h"
#include <stddef.h>

typedef struct {
    const char *name;       /* short grammar label (unique) */
    LSMLanguage lang;       /* explicit language for extraction-level tests */
    const char *path;       /* representative filename (drives discover by ext/name) */
    const char *src;        /* fixture source */
    int min_defs;           /* catastrophic-break floor (0 for data/config/markup) */
    const char *expect[4];  /* expected def names (NULL-terminated), optional */
} GrammarCase;

extern const GrammarCase LSM_GRAMMAR_CASES[];
extern const size_t LSM_GRAMMAR_CASES_COUNT;

#endif /* LSM_TEST_GRAMMAR_CASES_H */
