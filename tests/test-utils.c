#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../wcc.h"

void assert_long(long expected, long actual) {
    if (expected != actual) {
        failures++;
        printf("Expected %ld, got %ld\n", expected, actual);
    }
}

void assert_value(Value *v1, Value *v2) {
    if (v1->is_constant)
        assert_long(v1->int_value, v2->int_value);
    else if (v1->is_string_literal)
        assert_long(v1->vreg, v2->vreg);
    else if (v1->vreg) {
        if (v1->vreg == -1)
            assert_long(1, !!v2->vreg); // Check non-zero
        else
            assert_long(v1->vreg, v2->vreg);
    }
    else if (v1->global_symbol)
        assert_long(v1->global_symbol->identifier[1], v2->global_symbol->identifier[1]);
    else if (v1->label)
        assert_long(v1->label, v2->label);
    else if (v1->stack_index)
        assert_long(v1->stack_index, v2->stack_index);
    else if (v1->function_symbol)
        assert_long(v1->function_symbol->identifier[1], v2->function_symbol->identifier[1]);
    else
        panic("Don't know how to assert_value");
}

void assert_x86_op(char *expected) {
    char *got;

    if (!ir_start) {
        printf("Expected %s, got nothing\n", expected);
        failures++;
        return;
    }

    got = render_x86_operation(ir_start, 0, 0);

    if (!got) {
        printf("Mismatch:\n  expected: %s\n  got:      null\n", expected);
        failures++;
    }
    else {

        if (strcmp(got, expected)) {
            printf("Mismatch:\n  expected: %s\n  got:      %s\n", expected, got);
            failures++;
        }
    }

    ir_start = ir_start->next;
}

void assert_tac(Tac *tac, int operation, Value *dst, Value *src1, Value *src2) {
    if (!tac) {
        failures++;
        printf("Expected a tac, got nil\n");
        return;
    }

    assert_long(operation, tac->operation);

    if (dst)
        assert_value(dst,  tac->dst);
    else
        assert_long(0, !!tac->dst);

    if (src1)
        assert_value(src1, tac->src1);
    else
        assert_long(0, !!tac->src1);

    if (src2)
        assert_value(src2, tac->src2);
    else
        assert_long(0, !!tac->src2);

}

Tac *i(int label, int operation, Value *dst, Value *src1, Value *src2) {
    Tac *tac;

    tac = add_instruction(operation, dst, src1, src2);
    tac->label = label;
    return tac;
}

Value *p(Value *v) {
    v->type = make_pointer(v->type);
    return v;
}

Value *v(int vreg) {
    Value *v;

    v = new_value();
    v->type = new_type(TYPE_LONG);
    v->vreg = vreg;

    return v;
}

Value *uv(int vreg) {
    Value *v;

    v = new_value();
    v->type = new_type(TYPE_LONG);
    v->type->is_unsigned = 1;
    v->vreg = vreg;

    return v;
}

Value *vsz(int vreg, int type) {
    Value *v;

    v = new_value();
    v->type = new_type(type);
    v->vreg = vreg;

    return v;
}

Value *vusz(int vreg, int type) {
    Value *v;

    v = new_value();
    v->type = new_type(type);
    v->type->is_unsigned = 1;
    v->vreg = vreg;

    return v;
}


Value *a(int vreg) {
    Value *v;

    v = new_value();
    v->type = make_pointer(new_type(TYPE_LONG));
    v->vreg = vreg;

    return v;
}


Value *asz(int vreg, int type) {
    Value *v;

    v = new_value();
    v->type = make_pointer(new_type(type));
    v->vreg = vreg;

    return v;
}

Value *ausz(int vreg, int type) {
    Value *v;

    v = new_value();
    v->type = make_pointer(new_type(type));
    v->type->is_unsigned = 1;
    v->vreg = vreg;

    return v;
}

Value *l(int label) {
    Value *v;

    v = new_value();
    v->label = label;

    return v;
}

Value *csz(long value, int type_type) {
    return new_integral_constant(type_type, value);
}

Value *ci(int value) {
    return new_integral_constant(TYPE_INT, value);
}

Value *c(long value) {
    return new_integral_constant(TYPE_LONG, value);
}

Value *cld(long double value) {
    return new_floating_point_constant(TYPE_LONG_DOUBLE, value);
}

Value *uc(long value) {
    Value *v;
    v = new_integral_constant(TYPE_LONG, value);
    v->type->is_unsigned = 1;
    return v;
}

Value *s(int string_literal_index) {
    Value *v;

    v = new_value();
    v->type = make_pointer(new_type(TYPE_CHAR));
    v->string_literal_index = string_literal_index;
    v->is_string_literal = 1;
    asprintf(&(string_literals[string_literal_index]), "Test SL %d", string_literal_index);

    return v;
}

Value *S(int stack_index) {
    Value *v;

    v = new_value();
    v->type = new_type(TYPE_LONG);
    v->local_index = 0;
    v->stack_index = stack_index;
    v->is_lvalue = 1;

    return v;
}

Value *Ssz(int stack_index, int type) {
    Value *v;

    v = new_value();
    v->type = new_type(type);
    v->stack_index = stack_index;

    return v;
}

Value *g(int index) {
    Value *v;
    Symbol *s;

    s = malloc(sizeof(Symbol));
    memset(s, 0, sizeof(Symbol));
    asprintf(&(s->identifier), "g%d", index);

    v = new_value();
    v->type = new_type(TYPE_LONG);
    v->global_symbol = s;
    v->is_lvalue = 1;

    return v;
}

Value *gsz(int index, int type) {
    Value *v;

    v = g(index);
    v->type = new_type(type);

    return v;
}

Value *fu(int index) {
    Value *v;
    Symbol *s;

    v = new_value();
    v->type = 0;
    v->function_symbol = s;

    v->function_symbol = malloc(sizeof(Symbol));
    memset(v->function_symbol, 0, sizeof(Symbol));

    v->function_symbol->type = new_type(TYPE_FUNCTION);
    v->function_symbol->type->function = new_function();
    asprintf(&(v->function_symbol->identifier), "f%d", index);

    return v;
}

void remove_reserved_physical_register_count_from_tac(Tac *ir) {
    Tac *tac;

    if (!remove_reserved_physical_registers) return;

    tac = ir;
    while (tac) {
        if (tac->dst  && tac->dst ->vreg && tac->dst ->vreg >= live_range_reserved_pregs_offset) tac->dst ->vreg -= live_range_reserved_pregs_offset;
        if (tac->src1 && tac->src1->vreg && tac->src1->vreg >= live_range_reserved_pregs_offset) tac->src1->vreg -= live_range_reserved_pregs_offset;
        if (tac->src2 && tac->src2->vreg && tac->src2->vreg >= live_range_reserved_pregs_offset) tac->src2->vreg -= live_range_reserved_pregs_offset;

        tac = tac->next;
    }
}

void start_ir() {
    ir_start = 0;
    rule_coverage_file = "instrsel-tests.rulecov";
}

void _finish_ir(Function *function, int stop_after_live_ranges, int stop_after_instruction_selection) {
    function->ir = ir_start;
    function->stack_register_count = 0;

    if (stop_after_live_ranges)
        run_compiler_phases(function, "dummy", COMPILE_START_AT_ARITHMETIC_MANPULATION, COMPILE_STOP_AFTER_LIVE_RANGES);
    else if (stop_after_instruction_selection)
        run_compiler_phases(function, "dummy", COMPILE_START_AT_ARITHMETIC_MANPULATION, COMPILE_STOP_AFTER_INSTRUCTION_SELECTION);
    else
        run_compiler_phases(function, "dummy", COMPILE_START_AT_ARITHMETIC_MANPULATION, COMPILE_STOP_AFTER_ADD_SPILL_CODE);

    remove_reserved_physical_register_count_from_tac(function->ir);
    make_stack_register_count(function);
    make_stack_offsets(function, "dummy");

    // Move ir_start to first non-noop for convenience
    ir_start = function->ir;
    while (ir_start && ir_start->operation == IR_NOP) ir_start = ir_start->next;
    function->ir = ir_start;
}

void finish_register_allocation_ir(Function *function) {
    _finish_ir(function, 1, 0);
}

void finish_ir(Function *function) {
    _finish_ir(function, 0, 1);
}

void finish_spill_ir(Function *function) {
    _finish_ir(function, 0, 0);
}

Value *make_arg_src1() {
    // Setup rdi as a register
    Value *arg_src1 = c(0);
    FunctionParamLocations *fpl = malloc(sizeof(FunctionParamLocations));
    fpl->locations = malloc(sizeof(FunctionParamLocation));
    fpl->count = 1;
    fpl->locations[0].int_register = 0;
    fpl->locations[0].sse_register = -1;
    arg_src1->function_call_arg_locations = fpl;

    return arg_src1;
}
