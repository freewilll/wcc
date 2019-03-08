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

    do_ssa_experiments(function, 0);

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
    do_ssa_experiments(function, 0);

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
    do_ssa_experiments(function, 0);

    assert(-1, function->function_idom[0]);
    assert( 0, function->function_idom[1]);
    assert( 1, function->function_idom[2]);
    assert( 1, function->function_idom[3]);
    assert( 3, function->function_idom[4]);
    assert( 1, function->function_idom[5]);
    assert( 5, function->function_idom[6]);
    assert( 5, function->function_idom[7]);
    assert( 5, function->function_idom[8]);

    // Page 500 of engineering a compiler
    assert_set(function->function_dominance_frontiers[0], -1, -1, -1, -1, -1);
    assert_set(function->function_dominance_frontiers[1],  1, -1, -1, -1, -1);
    assert_set(function->function_dominance_frontiers[2],  3, -1, -1, -1, -1);
    assert_set(function->function_dominance_frontiers[3],  1, -1, -1, -1, -1);
    assert_set(function->function_dominance_frontiers[4], -1, -1, -1, -1, -1);
    assert_set(function->function_dominance_frontiers[5],  3, -1, -1, -1, -1);
    assert_set(function->function_dominance_frontiers[6],  7, -1, -1, -1, -1);
    assert_set(function->function_dominance_frontiers[7],  3, -1, -1, -1, -1);
    assert_set(function->function_dominance_frontiers[8],  7, -1, -1, -1, -1);
}

void check_phi(struct three_address_code *tac, int vreg) {
    assert(IR_PHI_FUNCTION, tac->operation);
    assert(vreg, tac->dst->vreg);
    assert(vreg, tac->src1->vreg);
    assert(vreg, tac->src2->vreg);
}

void test_phi_insertion2() {
    struct symbol *function;

    function = make_ir2();
    do_ssa_experiments(function, 0);

    // Page 502 of engineering a compiler
    assert_set(function->function_globals, 1, 2, 3, 4, 5);

    assert_set(function->function_var_blocks[1],  0,  3, -1, -1, -1); // i
    assert_set(function->function_var_blocks[2],  1,  5, -1, -1, -1); // a
    assert_set(function->function_var_blocks[3],  2,  7, -1, -1, -1); // b
    assert_set(function->function_var_blocks[4],  1,  2,  8, -1, -1); // c
    assert_set(function->function_var_blocks[5],  2,  5,  6, -1, -1); // d
    assert_set(function->function_var_blocks[6],  3, -1, -1, -1, -1); // y
    assert_set(function->function_var_blocks[7],  3, -1, -1, -1, -1); // z

    // Page 503 of engineering a compiler
    assert_set(function->function_phi_functions[0], -1, -1, -1, -1, -1);
    assert_set(function->function_phi_functions[1],  1,  2,  3,  4,  5);
    assert_set(function->function_phi_functions[2], -1, -1, -1, -1, -1);
    assert_set(function->function_phi_functions[3],  2,  3,  4,  5, -1);
    assert_set(function->function_phi_functions[4], -1, -1, -1, -1, -1);
    assert_set(function->function_phi_functions[5], -1, -1, -1, -1, -1);
    assert_set(function->function_phi_functions[6], -1, -1, -1, -1, -1);
    assert_set(function->function_phi_functions[7],  4,  5, -1, -1, -1);

    // Check the pre existing labels have been moved
    assert(0, function->function_blocks[0].start->label);
    assert(1, function->function_blocks[1].start->label);
    assert(0, function->function_blocks[2].start->label);
    assert(3, function->function_blocks[3].start->label);
    assert(0, function->function_blocks[4].start->label);
    assert(5, function->function_blocks[5].start->label);
    assert(0, function->function_blocks[6].start->label);
    assert(7, function->function_blocks[7].start->label);

    // Check phi functions are in place
    check_phi(function->function_blocks[1].start,                         1); // i
    check_phi(function->function_blocks[1].start->next,                   2); // a
    check_phi(function->function_blocks[1].start->next->next,             3); // b
    check_phi(function->function_blocks[1].start->next->next->next,       4); // c
    check_phi(function->function_blocks[1].start->next->next->next->next, 5); // d
    check_phi(function->function_blocks[3].start,                         2); // a
    check_phi(function->function_blocks[3].start->next,                   3); // b
    check_phi(function->function_blocks[3].start->next->next,             4); // c
    check_phi(function->function_blocks[3].start->next->next->next,       5); // d
    check_phi(function->function_blocks[7].start,                         4); // c
    check_phi(function->function_blocks[7].start->next,                   5); // d
}

// Check renumbered phi functions
void check_rphi(struct three_address_code *tac, int dst_vreg, int dst_ss, int src1_vreg, int src1_ss, int src2_vreg, int src2_ss) {
    assert(IR_PHI_FUNCTION, tac->operation);
    assert(dst_vreg,  tac->dst ->vreg); assert(dst_ss,  tac->dst ->ssa_subscript);
    assert(src1_vreg, tac->src1->vreg); assert(src1_ss, tac->src1->ssa_subscript);
    assert(src2_vreg, tac->src2->vreg); assert(src2_ss, tac->src2->ssa_subscript);
}

void test_phi_renumbering() {
    struct symbol *function;
    int vreg_count;
    int *counters;
    struct stack **stack;
    struct three_address_code *tac;

    function = make_ir2();
    do_ssa_experiments_common_prep(function);

    rename_phi_function_variables_common_prep(function, &stack, &counters, &vreg_count);

    // Special case for example, assume a, b, c, d are defined before B0
    new_subscript(stack, counters, 2);
    new_subscript(stack, counters, 3);
    new_subscript(stack, counters, 4);
    new_subscript(stack, counters, 5);

    rename_vars(function, stack, counters, 0, vreg_count);

    if (DEBUG_SSA_PHI_RENUMBERING) print_intermediate_representation(function);

    // Check renumberd args to phi functions are correct, page 509.
    // No effort is done to validate all other vars. It's safe to assume that
    // if the phi functions are correct, then it's pretty likely the other
    // vars are ok too.
    check_rphi(function->function_blocks[1].start,                         1, 1,  1, 0,  1, 2); // i
    check_rphi(function->function_blocks[1].start->next,                   2, 1,  2, 0,  2, 3); // a
    check_rphi(function->function_blocks[1].start->next->next,             3, 1,  3, 0,  3, 3); // b
    check_rphi(function->function_blocks[1].start->next->next->next,       4, 1,  4, 0,  4, 4); // c
    check_rphi(function->function_blocks[1].start->next->next->next->next, 5, 1,  5, 0,  5, 3); // d
    check_rphi(function->function_blocks[3].start,                         2, 3,  2, 2,  2, 4); // a
    check_rphi(function->function_blocks[3].start->next,                   3, 3,  3, 2,  3, 4); // b
    check_rphi(function->function_blocks[3].start->next->next,             4, 4,  4, 3,  4, 5); // c
    check_rphi(function->function_blocks[3].start->next->next->next,       5, 3,  5, 2,  5, 6); // d
    check_rphi(function->function_blocks[7].start,                         4, 5,  4, 2,  4, 6); // c
    check_rphi(function->function_blocks[7].start->next,                   5, 6,  5, 5,  5, 4); // d

    make_live_ranges(function);

    // In the textbook example, all variables end up being mapped straight onto their own live ranges
    tac = function->function_ir;
    while (tac) {
        if (tac->dst  && tac->dst ->vreg) assert(tac->dst ->vreg, tac->dst ->live_range + 1);
        if (tac->src1 && tac->src1->vreg) assert(tac->src1->vreg, tac->src1->live_range + 1);
        if (tac->src2 && tac->src2->vreg) assert(tac->src2->vreg, tac->src2->live_range + 1);

        tac = tac->next;
    }
}

int main() {
    test_dominance();
    test_liveout1();
    test_liveout2();
    test_idom2();
    test_phi_insertion2();
    test_phi_renumbering();
}
