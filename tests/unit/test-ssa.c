#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../wcc.h"

Function *function;

void assert(long expected, long actual) {
    if (expected != actual) {
        printf("Expected %ld, got %ld\n", expected, actual);
        exit(1);
    }
}

void assert_set(Set *got, int v1, int v2, int v3, int v4, int v5) {
    Set *is;

    is = new_set(got->max_value);
    if (v1 != -1) add_to_set(is, v1);
    if (v2 != -1) add_to_set(is, v2);
    if (v3 != -1) add_to_set(is, v3);
    if (v4 != -1) add_to_set(is, v4);
    if (v5 != -1) add_to_set(is, v5);
    assert(1, set_eq(got, is));
}

void assert_longset(LongSet *got, int v1, int v2, int v3, int v4, int v5) {
    LongSet *expected = new_longset();
    if (v1 != -1) longset_add(expected, v1);
    if (v2 != -1) longset_add(expected, v2);
    if (v3 != -1) longset_add(expected, v3);
    if (v4 != -1) longset_add(expected, v4);
    if (v5 != -1) longset_add(expected, v5);
    assert(1, longset_eq(expected, got));
}

Function *new_function_with_type(void) {
    Function *result = new_function();
    result->type = new_type(TYPE_FUNCTION);
    result->type->function = wcalloc(1, sizeof(FunctionType));
    result->type->target = new_type(TYPE_INT);
    return result;
}

void run_arithmetic_optimization(int operation, Value *dst, Value *src1, Value *src2) {
    Function *function;
    Tac *tac;

    opt_optimize_arithmetic_operations = 1;
    function = new_function_with_type();
    ir_start = 0;
    i(0, operation, dst, src1, src2);
    function->ir = ir_start;
    optimize_arithmetic_operations(function);
}

void run_int_arithmetic_optimization(int operation, Value *src1, Value *src2) {
    run_arithmetic_optimization(operation, v(3), src1, src2);
}

void run_long_double_arithmetic_optimization(int operation, Value *src1, Value *src2) {
    run_arithmetic_optimization(operation, vsz(TYPE_LONG_DOUBLE, 3), src1, src2);
}

void test_int_arithmetic_optimization_mul() {
    // v2 = 0 * v1
    run_int_arithmetic_optimization(IR_MUL, c(0), v(1));
    assert(1, ir_start->src1->is_constant);
    assert(0, ir_start->src1->int_value);
    assert(0, (int) ir_start->src2);

    // v2 = v1 * 0
    run_int_arithmetic_optimization(IR_MUL, v(1), c(0));
    assert(1, ir_start->src1->is_constant);
    assert(0, ir_start->src1->int_value);
    assert(0, (int) ir_start->src2);

    // v2 = 1 * v1
    run_int_arithmetic_optimization(IR_MUL, c(1), v(1));
    assert(IR_MOVE, ir_start->operation);
    assert(1, ir_start->src1->vreg);

    // v2 = v1 * 1
    run_int_arithmetic_optimization(IR_MUL, v(1), c(1));
    assert(IR_MOVE, ir_start->operation);
    assert(1, ir_start->src1->vreg);

    // v2 = 2 * v1
    run_int_arithmetic_optimization(IR_MUL, c(2), v(1));
    assert(IR_BSHL, ir_start->operation);
    assert(1, ir_start->src1->vreg);
    assert(1, ir_start->src2->is_constant);
    assert(1, ir_start->src2->int_value);

    // v2 = v1 * 2
    run_int_arithmetic_optimization(IR_MUL, v(1), c(2));
    assert(IR_BSHL, ir_start->operation);
    assert(1, ir_start->src1->vreg);
    assert(1, ir_start->src2->is_constant);
    assert(1, ir_start->src2->int_value);

    // v2 = v1 * 4
    run_int_arithmetic_optimization(IR_MUL, v(1), c(4));
    assert(IR_BSHL, ir_start->operation);
    assert(1, ir_start->src1->vreg);
    assert(1, ir_start->src2->is_constant);
    assert(2, ir_start->src2->int_value);
}

void test_int_arithmetic_optimization_div() {
    // v2 = v1 / 1
    run_int_arithmetic_optimization(IR_DIV, uv(1), c(1));
    assert(IR_MOVE, ir_start->operation);
    assert(1, ir_start->src1->vreg);

    // signed v2 = v1 / 2 is unchanged
    run_int_arithmetic_optimization(IR_DIV, v(1), c(2));
    assert(IR_DIV, ir_start->operation);

    // signed v2 = v1 / 3 is unchanged
    run_int_arithmetic_optimization(IR_DIV, v(1), c(3));
    assert(IR_DIV, ir_start->operation);

    // signed v2 = v1 / 4 is unchanged
    run_int_arithmetic_optimization(IR_DIV, v(1), c(4));
    assert(IR_DIV, ir_start->operation);

    // unsigned v2 = v1 / 2
    run_int_arithmetic_optimization(IR_DIV, uv(1), c(2));
    assert(IR_BSHR, ir_start->operation);
    assert(1, ir_start->src1->vreg);
    assert(1, ir_start->src2->is_constant);
    assert(1, ir_start->src2->int_value);

    // unsigned v2 = v1 / 3 is unchanged
    run_int_arithmetic_optimization(IR_DIV, uv(1), c(3));
    assert(IR_DIV, ir_start->operation);

    // unsigned v2 = v1 / 4
    run_int_arithmetic_optimization(IR_DIV, uv(1), c(4));
    assert(IR_BSHR, ir_start->operation);
    assert(1, ir_start->src1->vreg);
    assert(1, ir_start->src2->is_constant);
    assert(2, ir_start->src2->int_value);
}

void test_int_arithmetic_optimization_mod() {
    // v2 = v1 % 1
    run_int_arithmetic_optimization(IR_MOD, v(1), c(1));
    assert(IR_MOVE, ir_start->operation);
    assert(0, ir_start->src1->vreg);

    // v2 = v1 % 2
    run_int_arithmetic_optimization(IR_MOD, v(1), c(2));
    assert(IR_BAND, ir_start->operation);
    assert(1, ir_start->src1->vreg);
    assert(1, ir_start->src2->is_constant);
    assert(1, ir_start->src2->int_value);

    // v2 = v1 % 4
    run_int_arithmetic_optimization(IR_MOD, v(1), c(4));
    assert(IR_BAND, ir_start->operation);
    assert(1, ir_start->src1->vreg);
    assert(1, ir_start->src2->is_constant);
    assert(3, ir_start->src2->int_value);

    // v2 = v1 % 8
    run_int_arithmetic_optimization(IR_MOD, v(1), c(8));
    assert(IR_BAND, ir_start->operation);
    assert(1, ir_start->src1->vreg);
    assert(1, ir_start->src2->is_constant);
    assert(7, ir_start->src2->int_value);
}

void test_long_double_arithmetic_optimization() {
    // v2 = 0 * v1
    run_long_double_arithmetic_optimization(IR_MUL, cld(0.0L), vsz(TYPE_LONG_DOUBLE, 1));
    assert(1, ir_start->src1->is_constant);
    assert(0, ir_start->src1->int_value);
    assert(0, (int) ir_start->src2);

    // v2 = v1 * 0
    run_long_double_arithmetic_optimization(IR_MUL, vsz(TYPE_LONG_DOUBLE, 1), cld(0.0L));
    assert(1, ir_start->src1->is_constant);
    assert(0, ir_start->src1->int_value);
    assert(0, (int) ir_start->src2);

    // v2 = 1 * v1
    run_long_double_arithmetic_optimization(IR_MUL, cld(1.0L), vsz(TYPE_LONG_DOUBLE, 1));
    assert(IR_MOVE, ir_start->operation);
    assert(0, (int) ir_start->src2);

    // v2 = v1 * 1
    run_long_double_arithmetic_optimization(IR_MUL, vsz(TYPE_LONG_DOUBLE, 1), cld(1.0L));
    assert(IR_MOVE, ir_start->operation);
    assert(0, (int) ir_start->src2);

    // v2 = v1 / 1
    run_long_double_arithmetic_optimization(IR_DIV, vsz(TYPE_LONG_DOUBLE, 1), cld(1.0L));
    assert(IR_MOVE, ir_start->operation);
    assert(0, (int) ir_start->src2);
}

void test_arithmetic_optimization() {
    test_int_arithmetic_optimization_mul();
    test_int_arithmetic_optimization_div();
    test_int_arithmetic_optimization_mod();
    test_long_double_arithmetic_optimization();
}

// Ensure a JMP statement in the middle of a block makes instructions noops until the next label
void test_cfg_jmp() {
    Function *function;

    function = new_function_with_type();

    ir_start = 0;

    Tac *t1 = i(0, IR_NOP, 0, 0,    0);
    Tac *t2 = i(0, IR_JMP, 0, l(1), 0);
    Tac *t3 = i(0, IR_MUL, 0, 0,    0);
    Tac *t4 = i(0, IR_NOP, 0, 0,    0);
    Tac *t5 = i(1, IR_NOP, 0, 0,    0);

    function->ir = ir_start;

    make_vreg_count(function, 0);
    make_control_flow_graph(function);

    assert(2, function->cfg->node_count);
    assert(1, function->cfg->edge_count);
    assert((long) t1, (long) function->blocks[0].start); assert((long) t4, (long) function->blocks[0].end);
    assert((long) t5, (long) function->blocks[1].start); assert((long) t5, (long) function->blocks[1].end);
    assert(0, function->cfg->edges[0].from->id); assert(1, function->cfg->edges[0].to->id);
}

// Test example on page 478 of engineering a compiler
void test_dominance() {
    int i;
    Function *function;
    Block *blocks;

    function = new_function_with_type();

    blocks = calloc(9, sizeof(Block));

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

    function = new_function_with_type();

    ir_start = 0;
    i(0, IR_NOP,         0,    0,    0   );
    i(0, IR_MOVE,        v(1), c(1), 0   );  // v(1) = i
    i(1, IR_JZ,          0,    v(1), l(2));
    i(0, IR_MOVE,        v(2), c(0), 0   );  // v(2) = s
    i(2, IR_ADD,         v(2), v(1), v(2));
    i(0, IR_ADD,         v(1), v(1), c(1));
    i(0, IR_JZ,          0,    v(1), l(1));
    i(0, IR_MOVE_TO_PTR, 0,    v(2), c(1));

    function->ir = ir_start;
    function->type = new_type(TYPE_FUNCTION);
    function->type->function = wcalloc(1, sizeof(FunctionType));
    function->type->target = new_type(TYPE_INT);

    if (debug_ssa) print_ir(function, 0, 0);

    run_compiler_phases(function, "dummy", COMPILE_START_AT_ARITHMETIC_MANPULATION, COMPILE_STOP_AFTER_ANALYZE_DOMINANCE);
    make_uevar_and_varkill(function);
    make_liveout(function);

    assert(5, function->cfg->node_count);
    assert(6, function->cfg->edge_count);

    assert_longset(function->uevar[0], -1, -1, -1, -1, -1);
    assert_longset(function->uevar[1],  1, -1, -1, -1, -1);
    assert_longset(function->uevar[2], -1, -1, -1, -1, -1);
    assert_longset(function->uevar[3],  1,  2, -1, -1, -1);
    assert_longset(function->uevar[4], -1,  2, -1, -1, -1);

    assert_longset(function->varkill[0],  1, -1, -1, -1, -1);
    assert_longset(function->varkill[1], -1, -1, -1, -1, -1);
    assert_longset(function->varkill[2], -1,  2, -1, -1, -1);
    assert_longset(function->varkill[3],  1,  2, -1, -1, -1);
    assert_longset(function->varkill[4], -1, -1, -1, -1, -1);

    assert_longset(function->liveout[0],  1,  2, -1, -1, -1);
    assert_longset(function->liveout[1],  1,  2, -1, -1, -1);
    assert_longset(function->liveout[2],  1,  2, -1, -1, -1);
    assert_longset(function->liveout[3],  1,  2, -1, -1, -1);
    assert_longset(function->liveout[4], -1, -1, -1, -1, -1);
}

// Make IR for the test example on page 484 of engineering a compiler
Function *make_ir2(int init_four_vars) {
    Function *function;
    Tac *tac;

    function = new_function_with_type();

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
        i(0, IR_MOVE, v(2), c(1), 0);
        i(0, IR_MOVE, v(3), c(1), 0);
        i(0, IR_MOVE, v(4), c(1), 0);
        i(0, IR_MOVE, v(5), c(1), 0);
    }

    i(0, IR_MOVE,    v(1), c(1), 0   );
    i(1, IR_MOVE,    v(2), c(1), 0   );
    i(0, IR_MOVE,    v(4), c(1), 0   );
    i(0, IR_ADD,     0,    v(2), v(4)); // Fake conditional on a and c
    i(0, IR_JZ,      0,    v(2), l(5));
    i(0, IR_MOVE,    v(3), c(0), 0   );
    i(0, IR_MOVE,    v(4), c(0), 0   );
    i(0, IR_MOVE,    v(5), c(0), 0   );
    i(3, IR_ADD,     v(6), v(2), v(3));
    i(0, IR_ADD,     v(7), v(4), v(5));
    i(0, IR_ADD,     v(1), v(1), c(1));
    i(0, IR_JZ,      0,    v(1), l(1));
    i(0, IR_RETURN,  0,    0,    0   );
    i(5, IR_MOVE,    v(2), c(0), 0   );
    i(0, IR_MOVE,    v(5), c(0), 0   );
    i(0, IR_ADD,     0,    v(2), v(5)); // Fake conditional on a and d
    i(0, IR_JZ,      0,    v(2), l(8));
    i(0, IR_MOVE,    v(5), c(0), 0   );
    i(7, IR_MOVE,    v(3), c(0), 0   );
    i(0, IR_JMP,     0,    l(3), 0   );
    i(8, IR_MOVE,    v(4), c(0), 0   );
    i(0, IR_JMP,     0,    l(7), 0   );

    function->ir = ir_start;
    function->type = new_type(TYPE_FUNCTION);
    function->type->function = wcalloc(1, sizeof(FunctionType));
    function->type->target = new_type(TYPE_INT);

    if (debug_ssa) print_ir(function, 0, 0);

    return function;
}

// Test example on page 484 of engineering a compiler
void test_liveout2() {
    Function *function;

    function = make_ir2(0);
    run_compiler_phases(function, "dummy", COMPILE_START_AT_ARITHMETIC_MANPULATION, COMPILE_STOP_AFTER_ANALYZE_DOMINANCE);
    make_uevar_and_varkill(function);
    make_liveout(function);

    assert(9, function->cfg->node_count);
    assert(11, function->cfg->edge_count);

    assert_longset(function->uevar[0], -1, -1, -1, -1, -1);
    assert_longset(function->uevar[1], -1, -1, -1, -1, -1);
    assert_longset(function->uevar[2], -1, -1, -1, -1, -1);
    assert_longset(function->uevar[3],  1,  2,  3,  4,  5);
    assert_longset(function->uevar[4], -1, -1, -1, -1, -1);
    assert_longset(function->uevar[5], -1, -1, -1, -1, -1);
    assert_longset(function->uevar[6], -1, -1, -1, -1, -1);
    assert_longset(function->uevar[7], -1, -1, -1, -1, -1);
    assert_longset(function->uevar[8], -1, -1, -1, -1, -1);

    assert_longset(function->varkill[0],  1, -1, -1, -1, -1);
    assert_longset(function->varkill[1],  2,  4, -1, -1, -1);
    assert_longset(function->varkill[2],  3,  4,  5, -1, -1);
    assert_longset(function->varkill[3],  1,  6,  7, -1, -1);
    assert_longset(function->varkill[4], -1, -1, -1, -1, -1);
    assert_longset(function->varkill[5],  2,  5, -1, -1, -1);
    assert_longset(function->varkill[6],  5, -1, -1, -1, -1);
    assert_longset(function->varkill[7],  3, -1, -1, -1, -1);
    assert_longset(function->varkill[8],  4, -1, -1, -1, -1);

    assert_longset(function->liveout[0],  1, -1, -1, -1, -1);
    assert_longset(function->liveout[1],  2,  4,  1, -1, -1);
    assert_longset(function->liveout[2],  1,  2,  3,  4,  5);
    assert_longset(function->liveout[3],  1, -1, -1, -1, -1);
    assert_longset(function->liveout[4], -1, -1, -1, -1, -1);
    assert_longset(function->liveout[5],  1,  2,  4,  5, -1);
    assert_longset(function->liveout[6],  1,  2,  4,  5, -1);
    assert_longset(function->liveout[7],  1,  2,  3,  4,  5);
    assert_longset(function->liveout[8],  1,  2,  4,  5, -1);
}

// Test example on page 484 and 531 of engineering a compiler
void test_idom2() {
    Function *function;

    function = make_ir2(0);
    run_compiler_phases(function, "dummy", COMPILE_START_AT_ARITHMETIC_MANPULATION, COMPILE_STOP_AFTER_ANALYZE_DOMINANCE);

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

void test_idom3() {
    // Test building dominance frontiers with unreachable code
    // int main() { do continue; while(1); }

    Function *function;
    Tac *tac;

    function = new_function_with_type();

    ir_start = 0;
    i(0, IR_NOP,         0,    0,    0   );
    i(0, IR_START_LOOP,  0,    c(0), c(1));
    i(1, IR_NOP,         0,    0,    0   );
    i(0, IR_JMP,         0,    l(1), 0   );
    i(3, IR_NOP,         0,    0,    0   ); // Unreachable
    i(0, IR_JZ,          0,    c(1), l(2)); // Unreachable
    i(0, IR_JMP,         0,    l(1), 0   ); // Unreachable
    i(2, IR_NOP,         0,    0,    0   ); // Unreachable
    i(0, IR_END_LOOP,    0,    c(0), c(1));

    function->ir = ir_start;
    function->type = new_type(TYPE_FUNCTION);
    function->type->function = wcalloc(1, sizeof(FunctionType));
    function->type->target = new_type(TYPE_INT);

    run_compiler_phases(function, "dummy", COMPILE_START_AT_ARITHMETIC_MANPULATION, COMPILE_STOP_AFTER_ANALYZE_DOMINANCE);

    assert(5, function->cfg->node_count);

    assert(-1, function->idom[0]);
    assert( 0, function->idom[1]);
    assert(-1, function->idom[2]);
    assert(-1, function->idom[3]);
    assert(-1, function->idom[4]);

    assert_set(function->dominance_frontiers[0], -1, -1, -1, -1, -1);
    assert_set(function->dominance_frontiers[1],  1, -1, -1, -1, -1);
    assert_set(function->dominance_frontiers[2], -1, -1, -1, -1, -1);
    assert_set(function->dominance_frontiers[3],  1, -1, -1, -1, -1);
    assert_set(function->dominance_frontiers[4], -1, -1, -1, -1, -1);
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
    run_compiler_phases(function, "dummy", COMPILE_START_AT_ARITHMETIC_MANPULATION, COMPILE_STOP_AFTER_INSERT_PHI_FUNCTIONS);

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
    run_compiler_phases(function, "dummy", COMPILE_START_AT_ARITHMETIC_MANPULATION, COMPILE_STOP_AFTER_INSERT_PHI_FUNCTIONS);
    rename_phi_function_variables(function);

    if (debug_ssa_phi_renumbering) print_ir(function, 0, 0);

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

    function = new_function_with_type();
    ir_start = 0;

    i(0, IR_NOP,  0,    0,    0  );
    i(0, IR_MOVE, v(1), c(0), 0  );
    i(0, IR_JZ,   0,    v(1), l(1));
    i(0, IR_MOVE, v(1), c(1), 0  );
    i(0, IR_JZ,   0,    v(1), l(1));
    i(0, IR_MOVE, v(1), c(2), 0  );
    i(1, IR_NOP,  0,    0,    0  );
    i(0, IR_MOVE, v(2), v(1), 0  );

    function->ir = ir_start;
    function->type = new_type(TYPE_FUNCTION);
    function->type->function = wcalloc(1, sizeof(FunctionType));
    function->type->target = new_type(TYPE_INT);

    run_compiler_phases(function, "dummy", COMPILE_START_AT_ARITHMETIC_MANPULATION, COMPILE_STOP_AFTER_INSERT_PHI_FUNCTIONS);
    rename_phi_function_variables(function);

    if (debug_ssa_phi_renumbering) print_ir(function, 0, 0);

    // r1_3:long = Φ(r1_0:long, r1_1:long, r1_2:long)
    check_rphi(function->blocks[3].start, 1, 3,  1, 0,  1, 1,  1, 2);
}

// Make IR for the test example on page 484 of engineering a compiler
Function *make_ir3(int loop_count) {
    Function *function;
    Tac *tac;

    function = new_function_with_type();

    ir_start = 0;
    i(0, IR_NOP,    0,    0,    0   );
    i(0, IR_MOVE,   v(1), c(1), 0   ); // a = 0
    i(0, IR_JZ,     0,    v(1), l(1)); // jz l1
    i(0, IR_MOVE,   v(2), c(0), 0   ); // b   = 0
    i(0, IR_ADD,    0,    v(2), v(2)); // ... = b
    i(0, IR_MOVE,   v(4), c(0), 0   ); // d   = 0
    i(0, IR_JMP,    0 ,   l(2), 0   ); // jmp l2
    i(1, IR_NOP,    0,    0,    0   );
    if (loop_count > 0) i(0, IR_START_LOOP,  0, c(0), c(1));
    if (loop_count > 1) i(0, IR_START_LOOP,  0, c(1), c(2));
    i(0, IR_MOVE,   v(3), c(0), 0   ); // c   = 0
    if (loop_count > 1) i(0, IR_END_LOOP,  0, c(1), c(2));
    if (loop_count > 0) i(0, IR_END_LOOP,  0, c(0), c(1));
    i(0, IR_MOVE,   v(4), v(3), 0   ); // d   = c
    i(2, IR_NOP,    0,    0,    0   );
    i(0, IR_ADD,    0,    v(1), v(1)); // ... = a
    i(0, IR_ADD,    0,    v(4), v(4)); // ... = d

    function->ir = ir_start;
    function->type = new_type(TYPE_FUNCTION);
    function->type->function = wcalloc(1, sizeof(FunctionType));
    function->type->target = new_type(TYPE_INT);

    return function;
}

void assert_has_ig_edge(char *ig, int vreg_count, int from, int to) {
    assert((to > from && ig[from * vreg_count + to]) || ig[to * vreg_count + from], 1);
}

void test_interference_graph1() {
    int l;
    char *ig;
    Function *function;

    l = live_range_reserved_pregs_offset;

    function = make_ir3(0);

    if (debug_ssa_interference_graph) print_ir(function, 0, 0);

    opt_enable_live_range_coalescing = 0;
    run_compiler_phases(function, "dummy", COMPILE_START_AT_ARITHMETIC_MANPULATION, COMPILE_STOP_AFTER_LIVE_RANGES);

    ig = function->interference_graph;
    int vreg_count = function->vreg_count;

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
    function->type = new_type(TYPE_FUNCTION);
    function->type->function = wcalloc(1, sizeof(FunctionType));
    function->type->target = new_type(TYPE_INT);

    run_compiler_phases(function, "dummy", COMPILE_START_AT_ARITHMETIC_MANPULATION, COMPILE_STOP_AFTER_LIVE_RANGES);

    if (debug_ssa_interference_graph) print_ir(function, 0, 0);

    ig = function->interference_graph;
    int vreg_count = function->vreg_count;

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

    function = new_function_with_type();

    ir_start = 0;

    i(0, IR_NOP,  0,    0,    0   );
    i(0, IR_MOVE, v(1), c(1), 0   ); // a   = 0
    i(0, IR_MOVE, v(2), c(1), 0   ); // b   = 0
    i(0, IR_MOVE, v(3), v(2), 0   ); // c   = b  c interferes with a, but not with b
    i(0, IR_ADD,  0,    v(3), v(3)); // ... = c
    i(0, IR_ADD,  0,    v(1), v(1)); // ... = a

    function->ir = ir_start;
    function->type = new_type(TYPE_FUNCTION);
    function->type->function = wcalloc(1, sizeof(FunctionType));
    function->type->target = new_type(TYPE_INT);

    opt_enable_live_range_coalescing = 0;
    run_compiler_phases(function, "dummy", COMPILE_START_AT_ARITHMETIC_MANPULATION, COMPILE_STOP_AFTER_LIVE_RANGES);

    if (debug_ssa_interference_graph) print_ir(function, 0, 0);

    ig = function->interference_graph;
    int vreg_count = function->vreg_count;

    assert_has_ig_edge(ig, vreg_count, l + 1, l + 2);
    assert_has_ig_edge(ig, vreg_count, l + 1, l + 3);
}

void test_spill_cost() {
    int i, *spill_cost, p, l;
    Function *function;

    l = live_range_reserved_pregs_offset;

    for (i = 0; i < 3; i++) {
        function = make_ir3(i);

        opt_enable_live_range_coalescing = 0;
        run_compiler_phases(function, "dummy", COMPILE_START_AT_ARITHMETIC_MANPULATION, COMPILE_STOP_AFTER_LIVE_RANGES);

        if (debug_ssa_spill_cost) print_ir(function, 0, 0);

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

void test_coalesce() {
    Tac *tac;

    opt_enable_live_range_coalescing = 1;
    remove_reserved_physical_registers = 1;

    // A single coalesce
    start_ir();
    i(0, IR_MOVE, vsz(1, TYPE_LONG), c(1),              0                );
    i(0, IR_MOVE, vsz(2, TYPE_LONG), vsz(1, TYPE_LONG), 0                );
    i(0, IR_START_CALL, 0, c(0), 0);
    i(0, IR_ARG,  0,                 make_arg_src1(),   vsz(2, TYPE_LONG));
    finish_register_allocation_ir(function);
    assert_tac(ir_start,       IR_MOVE, vsz(2, TYPE_LONG), c(1), 0);
    assert_tac(ir_start->next, IR_NOP,  0,                 0,    0);

    // Don't coalesce if the registers interfere
    start_ir();
    i(0, IR_MOVE, vsz(1, TYPE_LONG), c(1),              0               );
    i(0, IR_MOVE, vsz(2, TYPE_LONG), vsz(1, TYPE_LONG), 0               );
    i(0, IR_ADD, vsz(3, TYPE_LONG),  vsz(1, TYPE_LONG), vsz(2, TYPE_LONG));
    i(0, IR_START_CALL, 0, c(0), 0);
    i(0, IR_ARG,  0,                 make_arg_src1(),   vsz(2, TYPE_LONG));
    finish_register_allocation_ir(function);

    assert_tac(ir_start,             IR_MOVE, vsz(1, TYPE_LONG), c(1),              0                );
    assert_tac(ir_start->next,       IR_MOVE, vsz(2, TYPE_LONG), vsz(1, TYPE_LONG), 0                );
    assert_tac(ir_start->next->next, IR_ADD,  vsz(3, TYPE_LONG), vsz(1, TYPE_LONG), vsz(2, TYPE_LONG));
}

void test_coalesce_promotion() {
    // Ensure a move that promotes an integer value in a register doesn't get coalesced
    Tac *tac;

    start_ir();
    i(0, IR_MOVE, vsz(1, TYPE_INT),  c(1),             0               );
    i(0, IR_MOVE, vsz(2, TYPE_LONG), vsz(1, TYPE_INT), 0               );
    i(0, IR_START_CALL, 0, c(0), 0);
    i(0, IR_ARG,  0,                 make_arg_src1(),  vsz(2, TYPE_LONG));
    finish_register_allocation_ir(function);
    assert_tac(ir_start, IR_MOVE, vsz(1, TYPE_INT), c(1), 0   );
    assert_tac(ir_start->next, IR_MOVE, vsz(2, TYPE_LONG), vsz(1, TYPE_INT), 0   );
}

void run_allocate_registers_top_down(Function *function, int physical_register_count) {
    init_vreg_locations(function);
    allocate_registers_top_down(function, 1, physical_register_count, PC_INT);
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

    function = new_function_with_type();

    vreg_count =  4;
    function->vreg_count = vreg_count;
    function->vreg_preg_classes = malloc(sizeof(char) * 5);
    memset(function->vreg_preg_classes, PC_INT, 5);

    // For some determinism, assign costs equal to the vreg number
    function->spill_cost = malloc((vreg_count + 1) * sizeof(int));
    for (i = 1; i <= vreg_count; i++) function->spill_cost[i] = i;

    ig = calloc((vreg_count + 1) * (vreg_count + 1), sizeof(int));
    function->interference_graph = ig;

    add_ig_edge(ig, vreg_count, 1, 2);
    add_ig_edge(ig, vreg_count, 1, 3);
    add_ig_edge(ig, vreg_count, 1, 4);

    // Everything is spilled. All nodes are constrained.
    // The vregs with the lowest cost get spilled first
    function->stack_register_count = 0;
    run_allocate_registers_top_down(function, 0);
    vl = function->vreg_locations;
    assert(-4, vl[1].stack_index);
    assert(-3, vl[2].stack_index);
    assert(-2, vl[3].stack_index);
    assert(-1, vl[4].stack_index);

    // Only one register is available. All nodes are constrained.
    // The most expensive non interfering nodes get the register
    function->stack_register_count = 0;
    run_allocate_registers_top_down(function, 1);
    vl = function->vreg_locations;
    assert(-1, vl[1].stack_index);
    assert(0,  vl[2].preg);
    assert(0,  vl[3].preg);
    assert(0,  vl[4].preg);

    // Register bliss. Only node 1 is constrained and it gets allocated the
    // first register. The rest don't interfere and all get the second
    // register.
    function->stack_register_count = 0;
    run_allocate_registers_top_down(function, 2);
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

    function->stack_register_count = 0;
    run_allocate_registers_top_down(function, 2);
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
    function->stack_register_count = 0;
    run_allocate_registers_top_down(function, 3);
    vl = function->vreg_locations;
    assert(0, vl[1].preg);
    assert(2, vl[2].preg);
    assert(1, vl[3].preg);
    assert(1, vl[4].preg);
}

int main() {
    init_memory_management_for_translation_unit();

    function = new_function_with_type();
    opt_optimize_arithmetic_operations = 1;
    string_literals = malloc(MAX_STRING_LITERALS);

    init_allocate_registers();

    test_arithmetic_optimization();
    test_cfg_jmp();
    test_dominance();
    test_liveout1();
    test_liveout2();
    test_idom2();
    test_idom3();
    test_phi_insertion();
    test_phi_renumbering1();
    test_phi_renumbering2();
    test_interference_graph1();
    test_interference_graph2();
    test_interference_graph3();
    test_spill_cost();
    test_coalesce();
    test_coalesce_promotion();
    test_top_down_register_allocation();
}
