#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "wc4.h"

void assert(long expected, long actual) {
    if (expected != actual) {
        printf("Expected %ld, got %ld\n", expected, actual);
        exit(1);
    }
}

void assert_set(Set *set, int v1, int v2, int v3, int v4, int v5) {
    Set *is;

    is = new_set(set->max_value);
    if (v1 != -1) add_to_set(is, v1);
    if (v2 != -1) add_to_set(is, v2);
    if (v3 != -1) add_to_set(is, v3);
    if (v4 != -1) add_to_set(is, v4);
    if (v5 != -1) add_to_set(is, v5);
    assert(1, set_eq(set, is));
}

Tac *i(int label, int operation, Value *dst, Value *src1, Value *src2) {
    Tac *tac;

    tac = add_instruction(operation, dst, src1, src2);
    tac->label = label;
    return tac;
}

Value *v(int vreg) {
    Value *v;

    v = new_value();
    v->type = TYPE_INT;
    v->vreg = vreg;

    return v;
}

Value *l(int label) {
    Value *v;

    v = new_value();
    v->label = label;

    return v;
}

Value *c(int value) {
    return new_constant(TYPE_INT, value);
}

// Ensure a JMP statement in the middle of a block ends the block
void test_cfg_jmp() {
    Function *function;
    Tac *t1, *t2, *t3, *t4;

    function = new_function();

    ir_start = 0;

    t1 = i(0, IR_NOP, 0, 0,    0);
    t2 = i(0, IR_JMP, 0, l(1), 0);
    t3 = i(0, IR_NOP, 0, 0,    0);
    t4 = i(1, IR_NOP, 0, 0,    0);

    function->ir = ir_start;

    make_vreg_count(function, 0);
    make_control_flow_graph(function);

    assert(2, function->cfg->node_count);
    assert(1, function->cfg->edge_count);
    assert(t1, function->blocks[0].start); assert(t2, function->blocks[0].end);
    assert(t4, function->blocks[1].start); assert(t4, function->blocks[1].end);
    assert(0, function->cfg->edges[0].from->id); assert(1, function->cfg->edges[0].to->id);
}

// Test example on page 478 of engineering a compiler
void test_dominance() {
    int i;
    Function *function;
    Block *blocks;

    function = new_function();

    blocks = malloc(9 * sizeof(Block));
    memset(blocks, 0, 9 * sizeof(Block));

    function->cfg = new_graph(9, 11);
    function->blocks = blocks;

    add_graph_edge(function->cfg, 0, 1);
    add_graph_edge(function->cfg, 1, 2);
    add_graph_edge(function->cfg, 1, 5);
    add_graph_edge(function->cfg, 2, 3);
    add_graph_edge(function->cfg, 5, 6);
    add_graph_edge(function->cfg, 5, 8);
    add_graph_edge(function->cfg, 6, 7);
    add_graph_edge(function->cfg, 8, 7);
    add_graph_edge(function->cfg, 7, 3);
    add_graph_edge(function->cfg, 3, 4);
    add_graph_edge(function->cfg, 3, 1);

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

    assert_set(function->dominance[0], 0, -1, -1, -1, -1);
    assert_set(function->dominance[1], 0,  1, -1, -1, -1);
    assert_set(function->dominance[2], 0,  1,  2, -1, -1);
    assert_set(function->dominance[3], 0,  1,  3, -1, -1);
    assert_set(function->dominance[4], 0,  1,  3,  4, -1);
    assert_set(function->dominance[5], 0,  1,  5, -1, -1);
    assert_set(function->dominance[6], 0,  1,  5,  6, -1);
    assert_set(function->dominance[7], 0,  1,  5,  7, -1);
    assert_set(function->dominance[8], 0,  1,  5,  8, -1);
}

// Test example on page 448 of engineering a compiler
void test_liveout1() {
    Function *function;
    Tac *tac;

    function = new_function();

    ir_start = 0;
    i(0, IR_NOP,    0,    0,    0   );
    i(0, IR_ASSIGN, v(1), c(1), 0   );
    i(1, IR_JZ,     0,    v(1), l(2));
    i(0, IR_ASSIGN, v(2), c(0), 0   );
    i(2, IR_ADD,    v(2), v(1), v(2));
    i(0, IR_ADD,    v(1), v(1), c(1));
    i(0, IR_JZ,     0,    v(1), l(1));
    i(0, IR_ARG,    0,    c(1), v(2));

    function->ir = ir_start;

    if (DEBUG_SSA) print_intermediate_representation(function, 0);

    do_oar1(function);
    make_uevar_and_varkill(function);
    make_liveout(function);

    assert(5, function->cfg->node_count);
    assert(6, function->cfg->edge_count);

    assert_set(function->uevar[0], -1, -1, -1, -1, -1);
    assert_set(function->uevar[1],  1, -1, -1, -1, -1);
    assert_set(function->uevar[2], -1, -1, -1, -1, -1);
    assert_set(function->uevar[3],  1,  2, -1, -1, -1);
    assert_set(function->uevar[4], -1,  2, -1, -1, -1);

    assert_set(function->varkill[0],  1, -1, -1, -1, -1);
    assert_set(function->varkill[1], -1, -1, -1, -1, -1);
    assert_set(function->varkill[2], -1,  2, -1, -1, -1);
    assert_set(function->varkill[3],  1,  2, -1, -1, -1);
    assert_set(function->varkill[4], -1, -1, -1, -1, -1);

    assert_set(function->liveout[0],  1,  2, -1, -1, -1);
    assert_set(function->liveout[1],  1,  2, -1, -1, -1);
    assert_set(function->liveout[2],  1,  2, -1, -1, -1);
    assert_set(function->liveout[3],  1,  2, -1, -1, -1);
    assert_set(function->liveout[4], -1, -1, -1, -1, -1);
}

// Make IR for the test example on page 484 of engineering a compiler
Function *make_ir2(int init_four_vars) {
    Function *function;
    Tac *tac;

    function = new_function();

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

    if (init_four_vars) {
        // Initialize a, b, c, d
        i(0, IR_ASSIGN,  v(2), c(1), 0);
        i(0, IR_ASSIGN,  v(3), c(1), 0);
        i(0, IR_ASSIGN,  v(4), c(1), 0);
        i(0, IR_ASSIGN,  v(5), c(1), 0);
    }

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

    function->ir = ir_start;

    if (DEBUG_SSA) print_intermediate_representation(function, 0);

    return function;
}

// Test example on page 484 of engineering a compiler
void test_liveout2() {
    Function *function;

    function = make_ir2(0);
    do_oar1(function);
    make_uevar_and_varkill(function);
    make_liveout(function);

    assert(9, function->cfg->node_count);
    assert(11, function->cfg->edge_count);

    assert_set(function->uevar[0], -1, -1, -1, -1, -1);
    assert_set(function->uevar[1], -1, -1, -1, -1, -1);
    assert_set(function->uevar[2], -1, -1, -1, -1, -1);
    assert_set(function->uevar[3],  1,  2,  3,  4,  5);
    assert_set(function->uevar[4], -1, -1, -1, -1, -1);
    assert_set(function->uevar[5], -1, -1, -1, -1, -1);
    assert_set(function->uevar[6], -1, -1, -1, -1, -1);
    assert_set(function->uevar[7], -1, -1, -1, -1, -1);
    assert_set(function->uevar[8], -1, -1, -1, -1, -1);

    assert_set(function->varkill[0],  1, -1, -1, -1, -1);
    assert_set(function->varkill[1],  2,  4, -1, -1, -1);
    assert_set(function->varkill[2],  3,  4,  5, -1, -1);
    assert_set(function->varkill[3],  1,  6,  7, -1, -1);
    assert_set(function->varkill[4], -1, -1, -1, -1, -1);
    assert_set(function->varkill[5],  2,  5, -1, -1, -1);
    assert_set(function->varkill[6],  5, -1, -1, -1, -1);
    assert_set(function->varkill[7],  3, -1, -1, -1, -1);
    assert_set(function->varkill[8],  4, -1, -1, -1, -1);

    assert_set(function->liveout[0],  1, -1, -1, -1, -1);
    assert_set(function->liveout[1],  2,  4,  1, -1, -1);
    assert_set(function->liveout[2],  1,  2,  3,  4,  5);
    assert_set(function->liveout[3],  1, -1, -1, -1, -1);
    assert_set(function->liveout[4], -1, -1, -1, -1, -1);
    assert_set(function->liveout[5],  1,  2,  4,  5, -1);
    assert_set(function->liveout[6],  1,  2,  4,  5, -1);
    assert_set(function->liveout[7],  1,  2,  3,  4,  5);
    assert_set(function->liveout[8],  1,  2,  4,  5, -1);
}

// Test example on page 484 and 531 of engineering a compiler
void test_idom2() {
    Function *function;

    function = make_ir2(0);
    do_oar1(function);
    // do_oar2(function);

    assert(-1, function->idom[0]);
    assert( 0, function->idom[1]);
    assert( 1, function->idom[2]);
    assert( 1, function->idom[3]);
    assert( 3, function->idom[4]);
    assert( 1, function->idom[5]);
    assert( 5, function->idom[6]);
    assert( 5, function->idom[7]);
    assert( 5, function->idom[8]);

    // Page 500 of engineering a compiler
    assert_set(function->dominance_frontiers[0], -1, -1, -1, -1, -1);
    assert_set(function->dominance_frontiers[1],  1, -1, -1, -1, -1);
    assert_set(function->dominance_frontiers[2],  3, -1, -1, -1, -1);
    assert_set(function->dominance_frontiers[3],  1, -1, -1, -1, -1);
    assert_set(function->dominance_frontiers[4], -1, -1, -1, -1, -1);
    assert_set(function->dominance_frontiers[5],  3, -1, -1, -1, -1);
    assert_set(function->dominance_frontiers[6],  7, -1, -1, -1, -1);
    assert_set(function->dominance_frontiers[7],  3, -1, -1, -1, -1);
    assert_set(function->dominance_frontiers[8],  7, -1, -1, -1, -1);
}

void check_phi(Tac *tac, int vreg) {
    assert(IR_PHI_FUNCTION, tac->operation);
    assert(vreg, tac->dst->vreg);
    assert(vreg, tac->phi_values[0].vreg);
    assert(vreg, tac->phi_values[1].vreg);
}

void test_phi_insertion() {
    Function *function;

    function = make_ir2(0);
    do_oar1(function);
    do_oar2(function);

    // Page 502 of engineering a compiler
    assert_set(function->globals, 1, 2, 3, 4, 5);

    assert_set(function->var_blocks[1],  0,  3, -1, -1, -1); // i
    assert_set(function->var_blocks[2],  1,  5, -1, -1, -1); // a
    assert_set(function->var_blocks[3],  2,  7, -1, -1, -1); // b
    assert_set(function->var_blocks[4],  1,  2,  8, -1, -1); // c
    assert_set(function->var_blocks[5],  2,  5,  6, -1, -1); // d
    assert_set(function->var_blocks[6],  3, -1, -1, -1, -1); // y
    assert_set(function->var_blocks[7],  3, -1, -1, -1, -1); // z

    // Page 503 of engineering a compiler
    assert_set(function->phi_functions[0], -1, -1, -1, -1, -1);
    assert_set(function->phi_functions[1],  1,  2,  3,  4,  5);
    assert_set(function->phi_functions[2], -1, -1, -1, -1, -1);
    assert_set(function->phi_functions[3],  2,  3,  4,  5, -1);
    assert_set(function->phi_functions[4], -1, -1, -1, -1, -1);
    assert_set(function->phi_functions[5], -1, -1, -1, -1, -1);
    assert_set(function->phi_functions[6], -1, -1, -1, -1, -1);
    assert_set(function->phi_functions[7],  4,  5, -1, -1, -1);

    // Check the pre existing labels have been moved
    assert(0, function->blocks[0].start->label);
    assert(1, function->blocks[1].start->label);
    assert(0, function->blocks[2].start->label);
    assert(3, function->blocks[3].start->label);
    assert(0, function->blocks[4].start->label);
    assert(5, function->blocks[5].start->label);
    assert(0, function->blocks[6].start->label);
    assert(7, function->blocks[7].start->label);

    // Check phi functions are in place
    check_phi(function->blocks[1].start,                         1); // i
    check_phi(function->blocks[1].start->next,                   2); // a
    check_phi(function->blocks[1].start->next->next,             3); // b
    check_phi(function->blocks[1].start->next->next->next,       4); // c
    check_phi(function->blocks[1].start->next->next->next->next, 5); // d
    check_phi(function->blocks[3].start,                         2); // a
    check_phi(function->blocks[3].start->next,                   3); // b
    check_phi(function->blocks[3].start->next->next,             4); // c
    check_phi(function->blocks[3].start->next->next->next,       5); // d
    check_phi(function->blocks[7].start,                         4); // c
    check_phi(function->blocks[7].start->next,                   5); // d
}

// Check renumbered phi functions
void check_rphi(Tac *tac, int dst_vreg, int dst_ss, int src1_vreg, int src1_ss, int src2_vreg, int src2_ss, int src3_vreg, int src3_ss) {
    assert(IR_PHI_FUNCTION, tac->operation);
    assert(dst_vreg,  tac->dst ->vreg); assert(dst_ss,  tac->dst ->ssa_subscript);
    assert(src1_vreg, tac->phi_values[0].vreg); assert(src1_ss, tac->phi_values[0].ssa_subscript);
    assert(src2_vreg, tac->phi_values[1].vreg); assert(src2_ss, tac->phi_values[1].ssa_subscript);

    if (src3_vreg) {
        assert(src3_vreg, tac->phi_values[2].vreg); assert(src3_ss, tac->phi_values[2].ssa_subscript);
    }
}

void test_phi_renumbering1() {
    Function *function;
    int vreg_count;
    int *counters;
    Stack **stack;
    Tac *tac;

    function = make_ir2(1);
    do_oar1(function);
    do_oar2(function);
    rename_phi_function_variables(function);

    if (DEBUG_SSA_PHI_RENUMBERING) print_intermediate_representation(function, 0);

    // Check renumbered args to phi functions are correct, page 509.
    // No effort is done to validate all other vars. It's safe to assume that
    // if the phi functions are correct, then it's pretty likely the other
    // vars are ok too.
    check_rphi(function->blocks[1].start,                         1, 1,  1, 0,  1, 2,  0, 0); // i
    check_rphi(function->blocks[1].start->next,                   2, 1,  2, 0,  2, 3,  0, 0); // a
    check_rphi(function->blocks[1].start->next->next,             3, 1,  3, 0,  3, 3,  0, 0); // b
    check_rphi(function->blocks[1].start->next->next->next,       4, 1,  4, 0,  4, 4,  0, 0); // c
    check_rphi(function->blocks[1].start->next->next->next->next, 5, 1,  5, 0,  5, 3,  0, 0); // d
    check_rphi(function->blocks[3].start,                         2, 3,  2, 2,  2, 4,  0, 0); // a
    check_rphi(function->blocks[3].start->next,                   3, 3,  3, 2,  3, 4,  0, 0); // b
    check_rphi(function->blocks[3].start->next->next,             4, 4,  4, 3,  4, 5,  0, 0); // c
    check_rphi(function->blocks[3].start->next->next->next,       5, 3,  5, 2,  5, 6,  0, 0); // d
    check_rphi(function->blocks[7].start,                         4, 5,  4, 2,  4, 6,  0, 0); // c
    check_rphi(function->blocks[7].start->next,                   5, 6,  5, 5,  5, 4,  0, 0); // d

    // In the textbook example, all variables end up being mapped straight onto their own live ranges
    make_live_ranges(function);
    tac = function->ir;
    while (tac) {
        if (tac->dst  && tac->dst ->vreg) assert(tac->dst ->vreg, tac->dst ->live_range - live_range_reserved_pregs_offset);
        if (tac->src1 && tac->src1->vreg) assert(tac->src1->vreg, tac->src1->live_range - live_range_reserved_pregs_offset);
        if (tac->src2 && tac->src2->vreg) assert(tac->src2->vreg, tac->src2->live_range - live_range_reserved_pregs_offset);
        tac = tac->next;
    }
}

// Test the case of a block that has three predecessors.
// The example here should create a phi function with 3 arguments
void test_phi_renumbering2() {
    Function *function;
    int vreg_count;
    int *counters;
    Stack **stack;
    Tac *tac;

    function = new_function();
    ir_start = 0;

    i(0, IR_NOP,    0,    0,    0  );
    i(0, IR_ASSIGN, v(1), c(0), 0  );
    i(0, IR_JZ,     0,    v(1), l(1));
    i(0, IR_ASSIGN, v(1), c(1), 0  );
    i(0, IR_JZ,     0,    v(1), l(1));
    i(0, IR_ASSIGN, v(1), c(2), 0  );
    i(1, IR_NOP,    0,    0,    0  );
    i(0, IR_ASSIGN, v(2), v(1), 0  );

    function->ir = ir_start;

    do_oar1(function);
    do_oar2(function);
    rename_phi_function_variables(function);

    if (DEBUG_SSA_PHI_RENUMBERING) print_intermediate_representation(function, 0);

    // r1_3:long = Φ(r1_0:long, r1_1:long, r1_2:long)
    check_rphi(function->blocks[3].start, 1, 3,  1, 0,  1, 1,  1, 2);
}

// Make IR for the test example on page 484 of engineering a compiler
Function *make_ir3(int loop_count) {
    Function *function;
    Tac *tac;

    function = new_function();

    ir_start = 0;
    i(0, IR_NOP,    0,    0,    0   );
    i(0, IR_ASSIGN, v(1), c(1), 0   ); // a = 0
    i(0, IR_JZ,     0,    v(1), l(1)); // jz l1
    i(0, IR_ASSIGN, v(2), c(0), 0   ); // b   = 0
    i(0, IR_ADD,    0,    v(2), v(2)); // ... = b
    i(0, IR_ASSIGN, v(4), c(0), 0   ); // d   = 0
    i(0, IR_JMP,     0,   l(2), 0   ); // jmp l2
    i(1, IR_NOP,    0,    0,    0   );
    if (loop_count > 0) i(0, IR_START_LOOP,  0, c(0), c(1));
    if (loop_count > 1) i(0, IR_START_LOOP,  0, c(1), c(2));
    i(0, IR_ASSIGN, v(3), c(0), 0   ); // c   = 0
    if (loop_count > 1) i(0, IR_END_LOOP,  0, c(1), c(2));
    if (loop_count > 0) i(0, IR_END_LOOP,  0, c(0), c(1));
    i(0, IR_ASSIGN, v(4), v(3), 0   ); // d   = c
    i(2, IR_NOP,    0,    0,    0   );
    i(0, IR_ADD,    0,    v(1), v(1)); // ... = a
    i(0, IR_ADD,    0,    v(4), v(4)); // ... = d

    function->ir = ir_start;

    return function;
}

void assert_has_ig_edge(char *ig, int vreg_count, int from, int to) {
    return (to > from && ig[from * vreg_count + to]) || ig[to * vreg_count + from];
}

void test_interference_graph1() {
    int l;
    char *ig;
    Function *function;

    l = live_range_reserved_pregs_offset;

    function = make_ir3(0);

    if (DEBUG_SSA_INTERFERENCE_GRAPH) print_intermediate_representation(function, 0);

    do_oar1(function);
    do_oar2(function);
    disable_live_ranges_coalesce = 1;
    do_oar3(function);

    ig = function->interference_graph;
    vreg_count = function->vreg_count;

    assert_has_ig_edge(ig, vreg_count, l + 1, l + 2);
    assert_has_ig_edge(ig, vreg_count, l + 1, l + 3);
    assert_has_ig_edge(ig, vreg_count, l + 1, l + 4);
}

void test_interference_graph2() {
    int l;
    char *ig;
    Function *function;

    l = live_range_reserved_pregs_offset;

    function = make_ir2(1);

    do_oar1(function);
    do_oar2(function);
    do_oar3(function);

    if (DEBUG_SSA_INTERFERENCE_GRAPH) print_intermediate_representation(function, 0);

    ig = function->interference_graph;
    vreg_count = function->vreg_count;

    assert_has_ig_edge(ig, vreg_count, l + 1, l + 2);
    assert_has_ig_edge(ig, vreg_count, l + 1, l + 3);
    assert_has_ig_edge(ig, vreg_count, l + 1, l + 4);
    assert_has_ig_edge(ig, vreg_count, l + 1, l + 5);
    assert_has_ig_edge(ig, vreg_count, l + 1, l + 6);
    assert_has_ig_edge(ig, vreg_count, l + 1, l + 7);
    assert_has_ig_edge(ig, vreg_count, l + 2, l + 3);
    assert_has_ig_edge(ig, vreg_count, l + 2, l + 4);
    assert_has_ig_edge(ig, vreg_count, l + 2, l + 5);
    assert_has_ig_edge(ig, vreg_count, l + 3, l + 4);
    assert_has_ig_edge(ig, vreg_count, l + 3, l + 5);
    assert_has_ig_edge(ig, vreg_count, l + 4, l + 5);
    assert_has_ig_edge(ig, vreg_count, l + 4, l + 6);
    assert_has_ig_edge(ig, vreg_count, l + 5, l + 6);
}

// Test the special case of a register copy not introducing an edge
void test_interference_graph3() {
    int l;
    char *ig;
    Function *function;

    l = live_range_reserved_pregs_offset;

    function = new_function();

    ir_start = 0;

    i(0, IR_NOP,    0,    0,    0   );
    i(0, IR_ASSIGN, v(1), c(1), 0   ); // a   = 0
    i(0, IR_ASSIGN, v(2), c(1), 0   ); // b   = 0
    i(0, IR_ASSIGN, v(3), v(2), 0   ); // c   = b  c interferes with a, but not with b
    i(0, IR_ADD,    0,    v(3), v(3)); // ... = c
    i(0, IR_ADD,    0,    v(1), v(1)); // ... = a

    function->ir = ir_start;

    do_oar1(function);
    do_oar2(function);
    disable_live_ranges_coalesce = 1;
    do_oar3(function);

    if (DEBUG_SSA_INTERFERENCE_GRAPH) print_intermediate_representation(function, 0);

    ig = function->interference_graph;
    vreg_count = function->vreg_count;

    assert_has_ig_edge(ig, vreg_count, l + 1, l + 2);
    assert_has_ig_edge(ig, vreg_count, l + 1, l + 3);
}

void test_spill_cost() {
    int i, *spill_cost, p, l;
    Function *function;

    l = live_range_reserved_pregs_offset;

    for (i = 0; i < 3; i++) {
        function = make_ir3(i);

        do_oar1(function);
        do_oar2(function);
        disable_live_ranges_coalesce = 1;
        do_oar3(function);

        if (DEBUG_SSA_SPILL_COST) print_intermediate_representation(function, 0);

        if (i == 0) p = 1;
        else if (i == 1) p = 10;
        else if (i == 2) p = 100;

        spill_cost = function->spill_cost;
        assert(4, spill_cost[l + 1]);
        assert(3, spill_cost[l + 2]);
        assert(p + 1, spill_cost[l + 3]);
        assert(4, spill_cost[l + 4]);
    }
}

void test_top_down_register_allocation() {
    int i, l, vreg_count;
    char *ig;
    Function *function;
    VregLocation *vl;

    // Don't reserve any physical registers, for simplicity
    live_range_reserved_pregs_offset = 0;

    // Create the graph on page 700 of engineering a compiler
    // 1 - 4
    // | \
    // 2   3

    function = new_function();

    vreg_count =  4;
    function->vreg_count = vreg_count;

    // For some determinism, assign costs equal to the vreg number
    function->spill_cost = malloc((vreg_count + 1) * sizeof(int));
    for (i = 1; i <= vreg_count; i++) function->spill_cost[i] = i;

    ig = malloc((vreg_count + 1) * (vreg_count + 1) * sizeof(int));
    memset(ig, 0, (vreg_count + 1) * (vreg_count + 1) * sizeof(int));
    function->interference_graph = ig;

    add_ig_edge(ig, vreg_count, 1, 2);
    add_ig_edge(ig, vreg_count, 1, 3);
    add_ig_edge(ig, vreg_count, 1, 4);

    // Everything is spilled. All nodes are constrained.
    // The vregs with the lowest cost get spilled first
    function->spilled_register_count = 0;
    allocate_registers_top_down(function, 0);
    vl = function->vreg_locations;
    assert(-4, vl[1].stack_index);
    assert(-3, vl[2].stack_index);
    assert(-2, vl[3].stack_index);
    assert(-1, vl[4].stack_index);

    // Only one register is available. All nodes are constrained.
    // The most expensive non interfering nodes get the register
    function->spilled_register_count = 0;
    allocate_registers_top_down(function, 1);
    vl = function->vreg_locations;
    assert(-1, vl[1].stack_index);
    assert(0,  vl[2].preg);
    assert(0,  vl[3].preg);
    assert(0,  vl[4].preg);

    // Register bliss. Only node 1 is constrained and it gets allocated the
    // first register. The rest don't interfere and all get the second
    // register.
    function->spilled_register_count = 0;
    allocate_registers_top_down(function, 2);
    vl = function->vreg_locations;
    assert(0, vl[1].preg);
    assert(1, vl[2].preg);
    assert(1, vl[3].preg);
    assert(1, vl[4].preg);

    // Add an edge from 2->4
    // 1 - 4
    // | X
    // 2   3
    // 1, 2, 4 are constrained and handled first
    // First 4 gets register 0 since it's the most expensieve
    // Then 2 gets register 1
    // Then 1 gets spilled to 0
    // Finally, when 3 gets done, all registers are free since node 0 was spilled. So it gets 0.

    // function->interference_graph_edge_count++;
    // edges[3].from = 2; edges[3].to = 4;
    add_ig_edge(ig, vreg_count, 2, 4);

    function->spilled_register_count = 0;
    allocate_registers_top_down(function, 2);
    vl = function->vreg_locations;
    assert(-1, vl[1].stack_index);
    assert(1,  vl[2].preg);
    assert(0,  vl[3].preg);
    assert(0,  vl[4].preg);

    // 3 registers. 1 is the only constrained one and gets a register first
    // 4, 3, 2 are done in order.
    // 4 gets 1, since 0 is used by node 1
    // 3 gets 1 since 0 is used by node 1
    // 2 gets 2 since 0 and 1 are in use by 1 and 4
    function->spilled_register_count = 0;
    allocate_registers_top_down(function, 3);
    vl = function->vreg_locations;
    assert(0, vl[1].preg);
    assert(2, vl[2].preg);
    assert(1, vl[3].preg);
    assert(1, vl[4].preg);
}

int main() {
    init_allocate_registers();

    test_cfg_jmp();
    test_dominance();
    test_liveout1();
    test_liveout2();
    test_idom2();
    test_phi_insertion();
    test_phi_renumbering1();
    test_phi_renumbering2();
    test_interference_graph1();
    test_interference_graph2();
    test_interference_graph3();
    test_spill_cost();
    test_top_down_register_allocation();
}
