#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "wcc.h"

Scope *global_scope;

// Initialize the global scope
void init_scopes() {
    global_scope = malloc(sizeof(Scope));
    memset(global_scope, 0, sizeof(Scope));
    global_scope->symbols = malloc(sizeof(Symbol) * MAX_GLOBAL_SCOPE_IDENTIFIERS);
    global_scope->max_symbol_count = MAX_GLOBAL_SCOPE_IDENTIFIERS;
    global_scope->symbol_count = 0;
    global_scope->parent = 0;

    cur_scope = global_scope;
}

// Initialize a local scope and set cur_scope as the parent
void enter_scope() {
    Scope *scope = malloc(sizeof(Scope));
    memset(scope, 0, sizeof(Scope));
    scope->symbols = malloc(sizeof(Symbol) * MAX_LOCAL_SCOPE_IDENTIFIERS);
    scope->max_symbol_count = MAX_LOCAL_SCOPE_IDENTIFIERS;
    scope->symbol_count = 0;
    scope->parent = cur_scope;

    cur_scope = scope;
}

// Leave the current scope and go back up to the parent scope
void exit_scope() {
    cur_scope = cur_scope->parent;
    if (!cur_scope) panic("Attempt to exit the global scope");
}
