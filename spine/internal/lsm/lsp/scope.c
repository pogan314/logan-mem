#include "scope.h"
#include <string.h>

LSMScope* lsm_scope_push(LSMArena* a, LSMScope* current) {
    LSMScope* scope = (LSMScope*)lsm_arena_alloc(a, sizeof(LSMScope));
    if (!scope) {
        return current;
    }
    memset(scope, 0, sizeof(LSMScope));
    scope->parent = current;
    scope->arena = a;
    return scope;
}

LSMScope* lsm_scope_pop(LSMScope* scope) {
    if (!scope) {
        return NULL;
    }
    return scope->parent;
}

static LSMScopeChunk* alloc_chunk(LSMScope* scope) {
    if (!scope->arena) {
        return NULL;
    }
    LSMScopeChunk* c = (LSMScopeChunk*)lsm_arena_alloc(scope->arena, sizeof(LSMScopeChunk));
    if (!c) {
        return NULL;
    }
    memset(c, 0, sizeof(LSMScopeChunk));
    c->next = scope->chunks;
    scope->chunks = c;
    return c;
}

/* Returns false when the binding could NOT be recorded in THIS frame.
 *
 * The failure that matters is arena exhaustion in alloc_chunk: the old void
 * form returned silently, so a caller that then consulted the scope CHAIN saw
 * the parent's binding for the same name and concluded the child had been
 * bound. For callable-value proof that is a fabricated identity -- the shadow
 * never took effect, yet the parent's callable looks like the child's. Callers
 * needing that distinction must use the checked form and consult the LOCAL
 * result, not a chain lookup. */
static bool lsm_scope_bind_value(LSMScope *scope, const char *name, const LSMType *type,
                                 const char *callable_qn) {
    if (!scope || !name) {
        return false;
    }
    for (LSMScopeChunk* c = scope->chunks; c != NULL; c = c->next) {
        for (int i = 0; i < c->used; i++) {
            if (c->bindings[i].name && strcmp(c->bindings[i].name, name) == 0) {
                c->bindings[i].type = type;
                c->bindings[i].callable_qn = callable_qn;
                return true;
            }
        }
    }
    LSMScopeChunk* head = scope->chunks;
    if (!head || head->used >= LSM_SCOPE_CHUNK_BINDINGS) {
        head = alloc_chunk(scope);
        if (!head) {
            return false; /* arena exhausted: the shadow did NOT take effect */
        }
    }
    head->bindings[head->used].name = name;
    head->bindings[head->used].type = type;
    head->bindings[head->used].callable_qn = callable_qn;
    head->used++;
    return true;
}

void lsm_scope_bind(LSMScope *scope, const char *name, const LSMType *type) {
    (void)lsm_scope_bind_value(scope, name, type, NULL);
}

bool lsm_scope_bind_checked(LSMScope *scope, const char *name, const LSMType *type) {
    return lsm_scope_bind_value(scope, name, type, NULL);
}

void lsm_scope_bind_callable(LSMScope *scope, const char *name, const LSMType *type,
                             const char *callable_qn) {
    (void)lsm_scope_bind_value(scope, name, type, callable_qn);
}

bool lsm_scope_bind_callable_checked(LSMScope *scope, const char *name, const LSMType *type,
                                     const char *callable_qn) {
    return lsm_scope_bind_value(scope, name, type, callable_qn);
}

const LSMType* lsm_scope_lookup(const LSMScope* scope, const char* name) {
    if (!name) {
        return lsm_type_unknown();
    }
    for (const LSMScope* s = scope; s != NULL; s = s->parent) {
        for (LSMScopeChunk* c = s->chunks; c != NULL; c = c->next) {
            for (int i = 0; i < c->used; i++) {
                if (c->bindings[i].name && strcmp(c->bindings[i].name, name) == 0) {
                    return c->bindings[i].type;
                }
            }
        }
    }
    return lsm_type_unknown();
}

bool lsm_scope_contains(const LSMScope *scope, const char *name) {
    if (!name) {
        return false;
    }
    for (const LSMScope *s = scope; s != NULL; s = s->parent) {
        for (const LSMScopeChunk *c = s->chunks; c != NULL; c = c->next) {
            for (int i = 0; i < c->used; i++) {
                if (c->bindings[i].name && strcmp(c->bindings[i].name, name) == 0) {
                    return true;
                }
            }
        }
    }
    return false;
}

const char *lsm_scope_lookup_callable(const LSMScope *scope, const char *name) {
    if (!name) {
        return NULL;
    }
    for (const LSMScope *s = scope; s != NULL; s = s->parent) {
        for (const LSMScopeChunk *c = s->chunks; c != NULL; c = c->next) {
            for (int i = 0; i < c->used; i++) {
                if (c->bindings[i].name && strcmp(c->bindings[i].name, name) == 0) {
                    return c->bindings[i].callable_qn;
                }
            }
        }
    }
    return NULL;
}

bool lsm_scope_update_callable(LSMScope *scope, const char *name, const char *callable_qn) {
    if (!name) {
        return false;
    }
    for (LSMScope *s = scope; s != NULL; s = s->parent) {
        for (LSMScopeChunk *c = s->chunks; c != NULL; c = c->next) {
            for (int i = 0; i < c->used; i++) {
                if (c->bindings[i].name && strcmp(c->bindings[i].name, name) == 0) {
                    c->bindings[i].callable_qn = callable_qn;
                    return true;
                }
            }
        }
    }
    return false;
}
