#include <stdio.h>

#include "wc4.h"

void assert(int expected, int actual, char *message) {
    if (expected != actual) {
        printf("%s: expected %d, got %d\n", message, expected, actual);
        exit(1);
    }
}

void assert_set(struct intset *set, int v1, int v2, int v3, int v4) {
    struct intset *is;

    is = new_intset();
    if (v1 != -1) add_to_set(is, v1);
    if (v2 != -1) add_to_set(is, v2);
    if (v3 != -1) add_to_set(is, v3);
    if (v4 != -1) add_to_set(is, v4);
    assert(1, set_eq(set, is));
}

int main() {
    struct symbol *function;
    struct block *blocks;
    struct edge *edges;
    int i;

    // Test example on page 478 of engineering a compiler
    function = malloc(sizeof(struct symbol));
    memset(function, 0, sizeof(struct symbol));

    blocks = malloc(20 * sizeof(struct block));
    memset(blocks, 0, 20 * sizeof(struct block));
    edges = malloc(20 * sizeof(struct edge));
    memset(edges, 0, 20 * sizeof(struct edge));

    function->function_blocks = blocks;
    function->function_block_count = 9;
    function->function_edges = edges;
    function->function_edge_count = 11;

    edges[ 0].from = 0; edges[ 0].to = 1; // 0 -> 1
    edges[ 1].from = 1; edges[ 1].to = 2; // 1 -> 2
    edges[ 2].from = 1; edges[ 2].to = 5; // 1 -> 5
    edges[ 3].from = 2; edges[ 3].to = 3; // 2 -> 3
    edges[ 4].from = 5; edges[ 4].to = 6; // 5 -> 6
    edges[ 5].from = 5; edges[ 5].to = 8; // 5 -> 8
    edges[ 6].from = 6; edges[ 6].to = 7; // 6 -> 7
    edges[ 7].from = 8; edges[ 7].to = 7; // 8 -> 7
    edges[ 8].from = 7; edges[ 8].to = 3; // 7 -> 3
    edges[ 9].from = 3; edges[ 9].to = 4; // 3 -> 4
    edges[10].from = 3; edges[10].to = 1; // 3 -> 1

    make_block_dominance(function);

    // Expected dominance
    // 0: {0}
    // 1: {0, 1}
    // 2: {0, 1, 2}
    // 3: {0, 1, 3}
    // 4: {0, 1, 3, 4}
    // 5: {0, 1, 5}
    // 6: {0, 1, 5, 6}
    // 7: {0, 1, 5, 7}
    // 8: {0, 1, 5, 8}

    assert_set(function->function_dominance[0], 0, -1, -1, -1);
    assert_set(function->function_dominance[1], 0,  1, -1, -1);
    assert_set(function->function_dominance[2], 0,  1,  2, -1);
    assert_set(function->function_dominance[3], 0,  1,  3, -1);
    assert_set(function->function_dominance[4], 0,  1,  3,  4);
    assert_set(function->function_dominance[5], 0,  1,  5, -1);
    assert_set(function->function_dominance[6], 0,  1,  5,  6);
    assert_set(function->function_dominance[7], 0,  1,  5,  7);
    assert_set(function->function_dominance[8], 0,  1,  5,  8);
}