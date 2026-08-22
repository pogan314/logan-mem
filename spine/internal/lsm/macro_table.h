#pragma once
#include <stddef.h>
#include "arena.h"

#define LSM_MACRO_MAX_PARAMS 4
#define LSM_MACRO_TABLE_CAP 4096

typedef struct {
    const char *name;
    int param_count;
    const char *param_names[LSM_MACRO_MAX_PARAMS];
    const char *expansion;
    const char *resolved_callee;
} LSMMacroEntry;

typedef struct LSMMacroTable {
    LSMMacroEntry entries[LSM_MACRO_TABLE_CAP];
    int count;
    LSMArena arena;
} LSMMacroTable;

// Add an entry. Silently drops on overflow.
void lsm_macro_table_add(LSMMacroTable *t, LSMArena *arena, const char *name, int param_count,
                         const char **param_names, const char *expansion,
                         const char *resolved_callee);

// Look up by name. Returns NULL if not found.
const LSMMacroEntry *lsm_macro_table_find(const LSMMacroTable *t, const char *name);

// Parse a single .inc file content into the table (arena-allocated strings).
void lsm_parse_inc_file(LSMMacroTable *t, LSMArena *arena, const char *content);

// Expand a macro call: substitute args into expansion text.
// Returns arena-allocated expanded text, or NULL if no expansion.
char *lsm_macro_expand(LSMArena *arena, const LSMMacroEntry *entry, const char **args,
                       int arg_count);

// Extract a callee name from expanded text (looks for ##class(X).Method or $$Label^Routine).
// Returns arena-allocated "X.Method" or "Label^Routine", or NULL.
char *lsm_macro_extract_callee(LSMArena *arena, const char *expansion);

// Allocate and populate a new table with the hardcoded system macros.
// Caller owns the table (stack or heap).
void lsm_macro_table_init_system(LSMMacroTable *t);

// Destroy the arena inside t and free t itself. NULL-safe.
void lsm_macro_table_free(LSMMacroTable *t);
