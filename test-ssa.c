#include <stdio.h>

#include "wc4.h"

void assert(int expected, int actual) {
    if (expected != actual) {
        printf("Expected %d, got %d\n", expected, actual);
        exit(1);
    }
}

void assert_set(struct intset *set, int v1, int v2, int v3, int v4, int v5) {
    struct intset *is;

    is = new_intset();
    if (v1 != -1) add_to_set(is, v1);
    if (v2 != -1) add_to_set(is, v2);
    if (v3 != -1) add_to_set(is, v3);
    if (v4 != -1) add_to_set(is, v4);
    if (v5 != -1) add_to_set(is, v5);
    assert(1, set_eq(set, is));
}

// Test example on page 478 of engineering a compiler
void test_dominance() {
    struct symbol *function;
    struct block *blocks;
    struct edge *edges;
    int i;

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

    assert_set(function->function_dominance[0], 0, -1, -1, -1, -1);
    assert_set(function->function_dominance[1], 0,  1, -1, -1, -1);
    assert_set(function->function_dominance[2], 0,  1,  2, -1, -1);
    assert_set(function->function_dominance[3], 0,  1,  3, -1, -1);
    assert_set(function->function_dominance[4], 0,  1,  3,  4, -1);
    assert_set(function->function_dominance[5], 0,  1,  5, -1, -1);
    assert_set(function->function_dominance[6], 0,  1,  5,  6, -1);
    assert_set(function->function_dominance[7], 0,  1,  5,  7, -1);
    assert_set(function->function_dominance[8], 0,  1,  5,  8, -1);
}

void i(int label, int operation, struct value *dst, struct value *src1, struct value *src2) {
    struct three_address_code *tac;

    tac = add_instruction(operation, dst, src1, src2);
    tac->label = label;
}

struct value *v(int vreg) {
    struct value *v;

    v = new_value();
    v->type = TYPE_INT;
    v->vreg = vreg;

    return v;
}

struct value *l(int label) {
    struct value *v;

    v = new_value();
    v->label = label;

    return v;
}

struct value *c(int value) {
    return new_constant(TYPE_INT, value);
}

// Test example on page 448 of engineering a compiler
void test_liveout1() {
    struct symbol *function;
    struct three_address_code *tac;

    function = malloc(sizeof(struct symbol));
    memset(function, 0, sizeof(struct symbol));

    ir_start = 0;
    i(0, IR_NOP,    0,    0,    0   );
    i(0, IR_ASSIGN, v(1), c(1), 0   );
    i(1, IR_JZ,     0,    v(1), l(2));
    i(0, IR_ASSIGN, v(2), c(0), 0   );
    i(2, IR_ADD,    v(2), v(1), v(2));
    i(0, IR_ADD,    v(1), v(1), c(1));
    i(0, IR_JZ,     0,    v(1), l(1));
    i(0, IR_ARG,    0,    c(1), v(2));

    function->function_ir = ir_start;
    function->identifier = "test";

    if (DEBUG_SSA) print_intermediate_representation(function);

    do_ssa_experiments(function);

    assert(5, function->function_block_count);
    assert(6, function->function_edge_count);

    assert_set(function->function_uevar[0], -1, -1, -1, -1, -1);
    assert_set(function->function_uevar[1],  1, -1, -1, -1, -1);
    assert_set(function->function_uevar[2], -1, -1, -1, -1, -1);
    assert_set(function->function_uevar[3],  1,  2, -1, -1, -1);
    assert_set(function->function_uevar[4], -1,  2, -1, -1, -1);

    assert_set(function->function_varkill[0],  1, -1, -1, -1, -1);
    assert_set(function->function_varkill[1], -1, -1, -1, -1, -1);
    assert_set(function->function_varkill[2], -1,  2, -1, -1, -1);
    assert_set(function->function_varkill[3],  1,  2, -1, -1, -1);
    assert_set(function->function_varkill[4], -1, -1, -1, -1, -1);

    assert_set(function->function_liveout[0],  1,  2, -1, -1, -1);
    assert_set(function->function_liveout[1],  1,  2, -1, -1, -1);
    assert_set(function->function_liveout[2],  1,  2, -1, -1, -1);
    assert_set(function->function_liveout[3],  1,  2, -1, -1, -1);
    assert_set(function->function_liveout[4], -1, -1, -1, -1, -1);
}

// Make IR for the test example on page 484 of engineering a compiler
struct symbol *make_ir2() {
    struct symbol *function;
    struct three_address_code *tac;

    function = malloc(sizeof(struct symbol));
    memset(function, 0, sizeof(struct symbol));

    // var/register
    // i = 1
    // a = 2
    // b = 3
    // c = 4
    // d = 5
    // y = 6
    // z = 7
    ir_start = 0;
    i(0, IR_NOP,     0,    0,    0   );
    i(0, IR_ASSIGN,  v(1), c(1), 0   );
    i(1, IR_ASSIGN,  v(2), c(1), 0   );
    i(0, IR_ASSIGN,  v(4), c(1), 0   );
    i(0, IR_ADD,     0,    v(2), v(4)); // Fake conditional on a and c
    i(0, IR_JZ,      0,    v(2), l(5));
    i(0, IR_ASSIGN,  v(3), c(0), 0   );
    i(0, IR_ASSIGN,  v(4), c(0), 0   );
    i(0, IR_ASSIGN,  v(5), c(0), 0   );
    i(3, IR_ADD,     v(6), v(2), v(3));
    i(0, IR_ADD,     v(7), v(4), v(5));
    i(0, IR_ADD,     v(1), v(1), c(1));
    i(0, IR_JZ,      0,    v(1), l(1));
    i(0, IR_RETURN,  0,    0,    0   );
    i(5, IR_ASSIGN,  v(2), c(0), 0   );
    i(0, IR_ASSIGN,  v(5), c(0), 0   );
    i(0, IR_ADD,     0,    v(2), v(5)); // Fake conditional on a and d
    i(0, IR_JZ,      0,    v(2), l(8));
    i(0, IR_ASSIGN,  v(5), c(0), 0   );
    i(7, IR_ASSIGN,  v(3), c(0), 0   );
    i(0, IR_JMP,     0,    l(3), 0   );
    i(8, IR_ASSIGN,  v(4), c(0), 0   );
    i(0, IR_JMP,     0,    l(7), 0   );

    function->function_ir = ir_start;
    function->identifier = "test";

    if (DEBUG_SSA) print_intermediate_representation(function);

    return function;
}

// Test example on page 484 of engineering a compiler
void test_liveout2() {
    struct symbol *function;

    function = make_ir2();

    do_ssa_experiments(function);

    assert(9, function->function_block_count);
    assert(11, function->function_edge_count);

    assert_set(function->function_uevar[0], -1, -1, -1, -1, -1);
    assert_set(function->function_uevar[1], -1, -1, -1, -1, -1);
    assert_set(function->function_uevar[2], -1, -1, -1, -1, -1);
    assert_set(function->function_uevar[3],  1,  2,  3,  4,  5);
    assert_set(function->function_uevar[4], -1, -1, -1, -1, -1);
    assert_set(function->function_uevar[5], -1, -1, -1, -1, -1);
    assert_set(function->function_uevar[6], -1, -1, -1, -1, -1);
    assert_set(function->function_uevar[7], -1, -1, -1, -1, -1);
    assert_set(function->function_uevar[8], -1, -1, -1, -1, -1);

    assert_set(function->function_varkill[0],  1, -1, -1, -1, -1);
    assert_set(function->function_varkill[1],  2,  4, -1, -1, -1);
    assert_set(function->function_varkill[2],  3,  4,  5, -1, -1);
    assert_set(function->function_varkill[3],  1,  6,  7, -1, -1);
    assert_set(function->function_varkill[4], -1, -1, -1, -1, -1);
    assert_set(function->function_varkill[5],  2,  5, -1, -1, -1);
    assert_set(function->function_varkill[6],  5, -1, -1, -1, -1);
    assert_set(function->function_varkill[7],  3, -1, -1, -1, -1);
    assert_set(function->function_varkill[8],  4, -1, -1, -1, -1);

    assert_set(function->function_liveout[0],  1, -1, -1, -1, -1);
    assert_set(function->function_liveout[1],  2,  4,  1, -1, -1);
    assert_set(function->function_liveout[2],  1,  2,  3,  4,  5);
    assert_set(function->function_liveout[3],  1, -1, -1, -1, -1);
    assert_set(function->function_liveout[4], -1, -1, -1, -1, -1);
    assert_set(function->function_liveout[5],  1,  2,  4,  5, -1);
    assert_set(function->function_liveout[6],  1,  2,  4,  5, -1);
    assert_set(function->function_liveout[7],  1,  2,  3,  4,  5);
    assert_set(function->function_liveout[8],  1,  2,  4,  5, -1);
}

// Test example on page 484 and 531 of engineering a compiler
void test_idom2() {
    struct symbol *function;

    function = make_ir2();

    do_ssa_experiments(function);
    assert(-1, function->function_idom[0]);
    assert( 0, function->function_idom[1]);
    assert( 1, function->function_idom[2]);
    assert( 1, function->function_idom[3]);
    assert( 3, function->function_idom[4]);
    assert( 1, function->function_idom[5]);
    assert( 5, function->function_idom[6]);
    assert( 5, function->function_idom[7]);
    assert( 5, function->function_idom[8]);
}

int main() {
    test_dominance();
    test_liveout1();
    test_liveout2();
    test_idom2();
}
