#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "wcc.h"

Scope *global_scope;

// Initialize the global scope
void init_scopes(void) {
    global_scope = malloc(sizeof(Scope));
    memset(global_scope, 0, sizeof(Scope));
    global_scope->symbol_list = malloc(sizeof(Symbol) * MAX_GLOBAL_SCOPE_IDENTIFIERS);
    global_scope->symbols = new_strmap();
    global_scope->tags = malloc(sizeof(Tag) * MAX_GLOBAL_SCOPE_IDENTIFIERS);
    global_scope->max_count = MAX_GLOBAL_SCOPE_IDENTIFIERS;
    global_scope->symbol_count = 0;
    global_scope->tag_count = 0;
    global_scope->parent = 0;

    cur_scope = global_scope;
}

// Initialize a local scope and set cur_scope as the parent
void enter_scope(void) {
    Scope *scope = malloc(sizeof(Scope));
    memset(scope, 0, sizeof(Scope));
    scope->symbol_list = malloc(sizeof(Symbol) * MAX_LOCAL_SCOPE_IDENTIFIERS);
    scope->symbols = new_strmap();
    scope->tags = malloc(sizeof(Tag) * MAX_LOCAL_SCOPE_IDENTIFIERS);
    scope->max_count = MAX_LOCAL_SCOPE_IDENTIFIERS;
    scope->symbol_count = 0;
    scope->tag_count = 0;
    scope->parent = cur_scope;

    cur_scope = scope;
}

// Leave the current scope and go back up to the parent scope
void exit_scope(void) {
    cur_scope = cur_scope->parent;
    if (!cur_scope) panic("Attempt to exit the global scope");
}

// Add a symbol to the current scope
Symbol *new_symbol(char *identifier) {
    if (cur_scope->symbol_count == cur_scope->max_count)
        panic("Exceeded max symbol table size of %d symbols", cur_scope->max_count);

    Symbol *symbol = malloc(sizeof(Symbol));
    memset(symbol, 0, sizeof(Symbol));
    symbol->identifier = identifier;
    cur_scope->symbol_list[cur_scope->symbol_count++] = symbol;
    strmap_put(cur_scope->symbols, identifier, symbol);
    symbol->scope = cur_scope;

    return symbol;
}

// Search for a symbol in a scope and recurse to parents if not found.
// Returns zero if not found in any parents
Symbol *lookup_symbol(char *name, Scope *scope, int recurse) {
    Symbol *symbol = strmap_get(scope->symbols, name);
    if (symbol) return symbol;
    else if (recurse && scope->parent) return lookup_symbol(name, scope->parent, recurse);
    else return 0;
}

Tag *new_tag(void) {
    if (cur_scope->tag_count == cur_scope->max_count)
        panic("Exceeded max tag table size of %d tags", cur_scope->max_count);

    Tag *tag = malloc(sizeof(Tag));
    memset(tag, 0, sizeof(Tag));
    cur_scope->tags[cur_scope->tag_count++] = tag;

    return tag;
}

// Search for a tag in a scope and recurse to parents if not found.
// Returns zero if not found in any parents
Tag *lookup_tag(char *name, Scope *scope, int recurse) {
    for (int i = 0; i < scope->tag_count; i++) {
        Tag *tag = scope->tags[i];
        if (!strcmp(tag->identifier, name)) return tag;
    }

    if (recurse && scope->parent) return lookup_tag(name, scope->parent, recurse);

    return 0;
}
