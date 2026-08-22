// Vendored tree-sitter grammar: haskell
// Each grammar compiled as separate unit (conflicting static symbols).
#include "vendored/common/tree_sitter/array.h" // strict-aliasing-safe helpers (tree-sitter#5242)
typedef void *(*lsm_haskell_array_grow_fn)(void *, uint32_t, uint32_t *, uint32_t, size_t);
_Static_assert(_Generic(&_array__grow, lsm_haskell_array_grow_fn: 1, default: 0),
               "Haskell scanner requires strict-aliasing-safe array helpers");
#include "vendored/grammars/haskell/parser.c"
#include "vendored/grammars/haskell/scanner.c"
