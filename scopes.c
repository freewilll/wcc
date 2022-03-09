#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "wcc.h"

Scope *global_scope;

static List *allocated_scopes;

// Initialize the global scope
void init_scopes(void) {
    allocated_scopes = new_list(128);

    global_scope = wcalloc(1, sizeof(Scope));
    append_to_list(allocated_scopes, global_scope);
    global_scope->symbol_list = new_list(128);
    global_scope->symbols = new_strmap();
    global_scope->tags = new_strmap();
    global_scope->parent = 0;

    cur_scope = global_scope;
}

void free_scopes(void) {
    for (int i = 0; i < allocated_scopes->length; i++) {
        Scope *scope = allocated_scopes->elements[i];

        for (int j = 0; j < scope->symbol_list->length; j++) {
            Symbol *symbol = scope->symbol_list->elements[j];

            List *initializers = symbol->initializers;
            if (initializers) {
                for (int j = 0; j < initializers->length; j++) {
                    Initializer *in = initializers->elements[j];
                    if (in->data) free(in->data);
                    free(in);
                }

                free_list(initializers);
            }

            free(symbol);
        }
        free_list(scope->symbol_list);
        free_strmap(scope->symbols);

        strmap_foreach(scope->tags, it) {
            char *key = strmap_iterator_key(&it);
            free(strmap_get(scope->tags, key));
        }
        free_strmap(scope->tags);

        free(scope);
    }

    free_list(allocated_scopes);
}

// Initialize a local scope and set cur_scope as the parent
void enter_scope(void) {
    Scope *scope = wcalloc(1, sizeof(Scope));
    append_to_list(allocated_scopes, scope);
    scope->symbol_list = new_list(128);
    scope->symbols = new_strmap();
    scope->tags = new_strmap();
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
    Symbol *symbol = wcalloc(1, sizeof(Symbol));
    symbol->identifier = identifier;
    append_to_list(cur_scope->symbol_list, symbol);
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

Tag *new_tag(char *identifier) {
    Tag *tag = wcalloc(1, sizeof(Tag));
    tag->identifier = identifier;
    strmap_put(cur_scope->tags, identifier, tag);

    return tag;
}

// Search for a tag in a scope and recurse to parents if not found.
// Returns zero if not found in any parents
Tag *lookup_tag(char *name, Scope *scope, int recurse) {
    Tag *tag = strmap_get(scope->tags, name);
    if (tag) return tag;
    else if (recurse && scope->parent) return lookup_tag(name, scope->parent, recurse);
    else return 0;
}
