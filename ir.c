#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "wcc.h"

static List *allocated_tacs;

void init_ir(void) {
    allocated_tacs = new_list(1024);
}

void free_ir(void) {
    for (int i = 0; i < allocated_tacs->length; i++) wfree(allocated_tacs->elements[i]);
    free_list(allocated_tacs);
}

// Allocate a new local variable or tempoary
static int new_local_index(Function *function) {
    return -1 - function->local_symbol_count++;
}

void init_value(Value *v) {
    v->preg = -1;
    v->stack_index = 0;
    v->ssa_subscript = -1;
    v->live_range = -1;
}

void init_value_allocations(void) {
    allocated_values = new_list(1024);
}

void free_values(void) {
    for (int i = 0; i < allocated_values->length; i++) {
        Value *v = allocated_values->elements[i];
        wfree(v);
    }

    free_list(allocated_values);
}

Value *new_value(void) {
    Value *v = wcalloc(1, sizeof(Value));
    append_to_list(allocated_values, v);
    init_value(v);

    return v;
}

// Create a new typed integral constant value.
Value *new_integral_constant(int type_type, long value) {
    Value *cv = new_value();
    cv->int_value = value;
    cv->type = new_type(type_type);
    cv->is_constant = 1;
    return cv;
}

// Create a new typed floating point constant value.
Value *new_floating_point_constant(int type_type, long double value) {
    Value *cv = new_value();
    cv->fp_value = value;
    cv->type = new_type(type_type);
    cv->is_constant = 1;
    return cv;
}

// Duplicate a value
Value *dup_value(Value *src) {
    Value *dst = new_value();
    *dst = *src;
    dst->type = dup_type(src->type);

    return dst;
}

void add_tac_to_ir(Tac *tac) {
    tac->next = 0;

    if (!ir_start) {
        ir_start = tac;
        ir = tac;
    }
    else {
        tac->prev = ir;
        ir->next = tac;
        ir = tac;
    }
}

Tac *new_instruction(int operation) {
    Tac *tac = wcalloc(1, sizeof(Tac));
    append_to_list(allocated_tacs, tac);
    tac->operation = operation;

    return tac;
}

// Add instruction to the global intermediate representation ir
Tac *add_instruction(int operation, Value *dst, Value *src1, Value *src2) {
    Tac *tac = new_instruction(operation);
    tac->dst = dst;
    tac->src1 = src1;
    tac->src2 = src2;
    add_tac_to_ir(tac);

    return tac;
}

// Ensure the double linked list in an IR is correct by checking last pointers
void sanity_test_ir_linkage(Function *function) {
    for (Tac *tac = function->ir; tac; tac = tac->next) {
        if (tac->next && tac->next->prev != tac) {
            printf("Linkage broken between:\n");
            print_instruction(stdout, tac, 0);
            print_instruction(stdout, tac->next, 0);
            panic("Bailing");
        }
    }
}

int print_value(void *f, Value *v, int is_assignment_rhs) {
    int c = 0; // Count outputted characters

    if (!v) panic("print_value() got null");

    if (v->is_lvalue) c += printf("{l}");
    if (v->is_lvalue_in_register) c += printf("{lr}");
    if (is_assignment_rhs && !v->is_lvalue && !v->vreg && !v->is_constant) c += fprintf(f, "&");
    if (!is_assignment_rhs && v->is_lvalue && v->vreg) c += fprintf(f, "L");

    if (v->is_constant) {
        if (is_integer_type(v->type))
            c += fprintf(f, "%ld", v->int_value);
        else
            c += fprintf(f, "%Lf", v->fp_value);
    }
    else if (v->preg != -1)
        c += fprintf(f, "p%d", v->preg);
    else if (v->local_index)
        c += fprintf(f, "L[%d]", v->local_index);
    else if (v->stack_index)
        c += fprintf(f, "S[%d]", v->stack_index);
    else if (v->vreg) {
        c += fprintf(f, "r%d", v->vreg);
        if (v->ssa_subscript != -1) c += fprintf(f, "_%d", v->ssa_subscript);
        if (v->live_range_preg) c += fprintf(f, "_LRpreg%d", v->live_range_preg);
    }
    else if (v->stack_index)
        c += fprintf(f, "s[%d]", v->stack_index);
    else if (v->global_symbol)
        c += fprintf(f, "%s", v->global_symbol->global_identifier);
    else if (v->is_string_literal)
        c += fprintf_escaped_string_literal(f, &(string_literals[v->string_literal_index]), 0);
    else if (v->label)
        c += fprintf(f, "l%d", v->label);
    else if (v->function_symbol) {
        c += fprintf(f, "function:%s", v->function_symbol->identifier);
        return c;
    }
    else
        // What's this?
        c += fprintf(f, "?%ld", v->int_value);

    if (v->bit_field_size) c += fprintf(f, "[bf %d:%d]", v->bit_field_offset, v->bit_field_size);
    else if (v->offset) c += fprintf(f, "[%d]", v->offset);

    if (!v->label) {
        c += fprintf(f, ":");
        c += print_type(f, v->type);
    }

    return c;
}

void print_instruction(void *f, Tac *tac, int expect_preg) {
    int o = tac->operation;

    if (tac->label)
        fprintf(f, "l%-5d", tac->label);
    else
        fprintf(f, "      ");

    if (tac->x86_template) {
        char *buffer = render_x86_operation(tac, 0, expect_preg);
        fprintf(f, "%s\n", buffer);
        wfree(buffer);
        return;
    }

    if (tac->dst && o < X_START) {
        print_value(f, tac->dst, o != IR_MOVE);
        fprintf(f, " = ");
    }

    if (o == IR_MOVE_TO_PTR) {
        fprintf(f, "(");
        print_value(f, tac->src1, 0);
        fprintf(f, ") = ");
    }

    if (o == IR_MOVE || o == IR_MOVE_PREG_CLASS || o == IR_LOAD_BIT_FIELD || o == IR_SAVE_BIT_FIELD)
        print_value(f, tac->src1, 1);

    else if (o == IR_LOAD_FROM_GOT) {
        fprintf(f, "from GOT ");
        print_value(f, tac->src1, 1);
    }

    else if (o == IR_MOVE_STACK_PTR)
        fprintf(f, "rsp");

    else if (o == IR_DECL_LOCAL_COMP_OBJ)  {
        fprintf(f, "declare ");
        print_value(f, tac->src1, 1);
    }

    else if (o == IR_START_CALL) fprintf(f, "start call %ld", tac->src1->int_value);
    else if (o == IR_END_CALL) fprintf(f, "end call %ld", tac->src1->int_value);

    else if (o == IR_ARG || o == X_ARG) {
        fprintf(f, "arg for call %ld ", tac->src1->int_value);
        print_value(f, tac->src2, 1);
    }
    else if (o == IR_ARG_STACK_PADDING) {
        fprintf(f, "arg extra push");
    }

    else if (o == IR_CALL) {
        if (tac->src1->function_symbol)
            fprintf(f, "call \"%s\"", tac->src1->function_symbol->identifier);
        else {
            fprintf(f, "call ");
            print_value(f, tac->src1, 1);
        }
    }

    else if (o == IR_VA_START) {
        fprintf(f, "va_start ");
        print_value(f, tac->src1, 1);
    }

    else if (o == IR_VA_ARG)
        fprintf(f, "va_arg ");

    else if (o == IR_ZERO) {
        fprintf(f, "zero memory with size ");
        print_value(f, tac->src1, 1);
    }

    else if (o == IR_LOAD_LONG_DOUBLE) {
        fprintf(f, "load long double ");
        print_value(f, tac->src1, 1);
    }

    else if (o == IR_RETURN) {
        fprintf(f, "return");
        if (tac->src1) {
            fprintf(f, " ");
            print_value(f, tac->src1, 1);
        }
    }

    else if (o == IR_START_LOOP) fprintf(f, "start loop par=%ld loop=%ld", tac->src1->int_value, tac->src2->int_value);
    else if (o == IR_END_LOOP)   fprintf(f, "end loop par=%ld loop=%ld",   tac->src1->int_value, tac->src2->int_value);

    else if (o == IR_MOVE_TO_PTR)
        print_value(f, tac->src2, 1);

    else if (o == IR_NOP)
        fprintf(f, "nop");

    else if (o == IR_JZ || o == IR_JNZ) {
             if (o == IR_JZ)  fprintf(f, "jz ");
        else if (o == IR_JNZ) fprintf(f, "jnz ");
        print_value(f, tac->src1, 1);
        fprintf(f, ", ");
        print_value(f, tac->src2, 1);
    }

    else if (o == IR_JMP)
        fprintf(f, "jmp l%d", tac->src1->label);

    else if (o == IR_PHI_FUNCTION) {
        fprintf(f, "Φ(");
        Value *v = tac->phi_values;
        int first = 1;
        while (v->type) {
            if (!first) fprintf(f, ", ");
            print_value(f, v, 1);
            first = 0;
            v++;
        }
        fprintf(f, ")");
    }

    else if (o == IR_INDIRECT)      {                               fprintf(f, "*");          print_value(f, tac->src1, 1); }
    else if (o == IR_ADDRESS_OF)    {                               fprintf(f, "&");          print_value(f, tac->src1, 1); }
    else if (o == IR_ADDRESS_OF_FROM_GOT) {                         fprintf(f, "& from GOT"); print_value(f, tac->src1, 1); }
    else if (o == IR_ADD)           { print_value(f, tac->src1, 1); fprintf(f, " + ");        print_value(f, tac->src2, 1); }
    else if (o == IR_SUB)           { print_value(f, tac->src1, 1); fprintf(f, " - ");        print_value(f, tac->src2, 1); }
    else if (o == IR_RSUB)          { print_value(f, tac->src2, 1); fprintf(f, " - ");        print_value(f, tac->src1, 1); fprintf(f, " [reverse]"); }
    else if (o == IR_MUL)           { print_value(f, tac->src1, 1); fprintf(f, " * ");        print_value(f, tac->src2, 1); }
    else if (o == IR_DIV)           { print_value(f, tac->src1, 1); fprintf(f, " / ");        print_value(f, tac->src2, 1); }
    else if (o == IR_MOD)           { print_value(f, tac->src1, 1); fprintf(f, " %% ");       print_value(f, tac->src2, 1); }
    else if (o == IR_BNOT)          {                               fprintf(f, "~");          print_value(f, tac->src1, 1); }
    else if (o == IR_EQ)            { print_value(f, tac->src1, 1); fprintf(f, " == ");       print_value(f, tac->src2, 1); }
    else if (o == IR_NE)            { print_value(f, tac->src1, 1); fprintf(f, " != ");       print_value(f, tac->src2, 1); }
    else if (o == IR_GT)            { print_value(f, tac->src1, 1); fprintf(f, " > ");        print_value(f, tac->src2, 1); }
    else if (o == IR_LT)            { print_value(f, tac->src1, 1); fprintf(f, " < ");        print_value(f, tac->src2, 1); }
    else if (o == IR_GE)            { print_value(f, tac->src1, 1); fprintf(f, " >= ");       print_value(f, tac->src2, 1); }
    else if (o == IR_LE)            { print_value(f, tac->src1, 1); fprintf(f, " <= ");       print_value(f, tac->src2, 1); }
    else if (o == IR_BAND)          { print_value(f, tac->src1, 1); fprintf(f, " & ");        print_value(f, tac->src2, 1); }
    else if (o == IR_BOR)           { print_value(f, tac->src1, 1); fprintf(f, " | ");        print_value(f, tac->src2, 1); }
    else if (o == IR_XOR)           { print_value(f, tac->src1, 1); fprintf(f, " ^ ");        print_value(f, tac->src2, 1); }
    else if (o == IR_BSHL)          { print_value(f, tac->src1, 1); fprintf(f, " << ");       print_value(f, tac->src2, 1); }
    else if (o == IR_BSHR)          { print_value(f, tac->src1, 1); fprintf(f, " >> ");       print_value(f, tac->src2, 1); }
    else if (o == IR_ASHR)          { print_value(f, tac->src1, 1); fprintf(f, " a>> ");      print_value(f, tac->src2, 1); }
    else if (o == X_CALL)           { fprintf(f, "call "  ); print_value(f, tac->src1, 1); if (tac->dst) { printf(" -> "); print_value(f, tac->dst, 1); } }
    else if (o == IR_CALL_ARG_REG)  { fprintf(f, "call reg arg "); print_value(f, tac->dst ? tac->dst : tac->src1 , 1); }
    else if (o == IR_ALLOCATE_STACK){ fprintf(f, "allocate stack "); print_value(f, tac->src1, 1); }
    else if (o == X_LEA)            { fprintf(f, "lea "   ); print_value(f, tac->src1, 1); fprintf(f, ", "); print_value(f, tac->dst,  1); }

    else if (o == X_MOV)    { fprintf(f, "%-6s", operation_string(o)); print_value(f, tac->src1, 1); fprintf(f, ", "); print_value(f, tac->dst,  1); }
    else if (o == X_MOVS)   { fprintf(f, "%-6s", operation_string(o)); print_value(f, tac->src1, 1); fprintf(f, ", "); print_value(f, tac->dst,  1); }
    else if (o == X_MOVZ)   { fprintf(f, "%-6s", operation_string(o)); print_value(f, tac->src1, 1); fprintf(f, ", "); print_value(f, tac->dst,  1); }
    else if (o == X_MOVC)   { fprintf(f, "%-6s", operation_string(o)); print_value(f, tac->src1, 1); fprintf(f, ", "); print_value(f, tac->dst,  1); }
    else if (o == X_ADD)    { fprintf(f, "%-6s", operation_string(o)); print_value(f, tac->src1, 1); fprintf(f, ", "); print_value(f, tac->dst,  1); }
    else if (o == X_MUL)    { fprintf(f, "%-6s", operation_string(o)); print_value(f, tac->src1, 1); fprintf(f, ", "); print_value(f, tac->dst,  1); }
    else if (o == X_IDIV)   { fprintf(f, "%-6s", operation_string(o)); print_value(f, tac->src1, 1); fprintf(f, ", "); print_value(f, tac->dst,  1); }
    else if (o == X_CQTO)   { fprintf(f, "%-6s", operation_string(o)); print_value(f, tac->src1, 1); fprintf(f, ", "); print_value(f, tac->dst,  1); }
    else if (o == X_CMP)    { fprintf(f, "%-6s", operation_string(o)); print_value(f, tac->src1, 1); fprintf(f, ", "); print_value(f, tac->src2, 1); }
    else if (o == X_CMPZ)   { fprintf(f, "%-6s", operation_string(o)); fprintf(f, "0");              fprintf(f, ", "); print_value(f, tac->src1, 1); }
    else if (o == X_JMP)    { fprintf(f, "%-6s", operation_string(o)); print_value(f, tac->src1, 1); }
    else if (o == X_JZ)     { fprintf(f, "%-6s", operation_string(o)); print_value(f, tac->src1, 1); }
    else if (o == X_JNZ)    { fprintf(f, "%-6s", operation_string(o)); print_value(f, tac->src1, 1); }
    else if (o == X_JE)     { fprintf(f, "%-6s", operation_string(o)); print_value(f, tac->src1, 1); }
    else if (o == X_JNE)    { fprintf(f, "%-6s", operation_string(o)); print_value(f, tac->src1, 1); }
    else if (o == X_JLT)    { fprintf(f, "%-6s", operation_string(o)); print_value(f, tac->src1, 1); }
    else if (o == X_JGT)    { fprintf(f, "%-6s", operation_string(o)); print_value(f, tac->src1, 1); }
    else if (o == X_JLE)    { fprintf(f, "%-6s", operation_string(o)); print_value(f, tac->src1, 1); }
    else if (o == X_JGE)    { fprintf(f, "%-6s", operation_string(o)); print_value(f, tac->src1, 1); }
    else if (o == X_JB)     { fprintf(f, "%-6s", operation_string(o)); print_value(f, tac->src1, 1); }
    else if (o == X_JA)     { fprintf(f, "%-6s", operation_string(o)); print_value(f, tac->src1, 1); }
    else if (o == X_JBE)    { fprintf(f, "%-6s", operation_string(o)); print_value(f, tac->src1, 1); }
    else if (o == X_JAE)    { fprintf(f, "%-6s", operation_string(o)); print_value(f, tac->src1, 1); }
    else if (o == X_SETE)   { fprintf(f, "%-6s", operation_string(o)); print_value(f, tac->src1, 1); }
    else if (o == X_SETNE)  { fprintf(f, "%-6s", operation_string(o)); print_value(f, tac->src1, 1); }
    else if (o == X_SETLT)  { fprintf(f, "%-6s", operation_string(o)); print_value(f, tac->src1, 1); }
    else if (o == X_SETGT)  { fprintf(f, "%-6s", operation_string(o)); print_value(f, tac->src1, 1); }
    else if (o == X_SETLE)  { fprintf(f, "%-6s", operation_string(o)); print_value(f, tac->src1, 1); }
    else if (o == X_SETGE)  { fprintf(f, "%-6s", operation_string(o)); print_value(f, tac->src1, 1); }

    else
        panic("print_instruction(): Unknown operation: %d", tac->operation);

    fprintf(f, "\n");
}

void print_ir(Function *function, char* name, int expect_preg) {
    if (name) fprintf(stdout, "%s:\n", name);

    int i = 0;
    for (Tac *tac = function->ir; tac; tac = tac->next) {
        fprintf(stdout, "%-4d > ", i++);
        print_instruction(stdout, tac, expect_preg);
    }
    fprintf(stdout, "\n");
}

// Merge tac with the instruction after it. The next instruction is removed from the chain.
static void merge_instructions(Tac *tac, int ir_index, int allow_labelled_next) {
    if (!tac->next) panic("merge_instructions called on a tac without next\n");
    if (tac->next->label && !allow_labelled_next) panic("merge_instructions called on a tac with a label");

    Tac *next = tac->next;
    tac->next = next->next;
    if (tac->next) tac->next->prev = tac;

    // Nuke the references to the removed node to prevent confusion
    next->prev = 0;
    next->next = 0;
}

int make_function_call_count(Function *function) {
    // Need to count this IR's function_call_count
    int function_call_count = 0;
    for (Tac *tac = function->ir; tac; tac = tac->next)
        if (tac->operation == IR_START_CALL) function_call_count++;

    return function_call_count;
}

// The arguments are pushed onto the stack right to left, but the ABI requries
// the seventh arg and later to be pushed in reverse order. Easiest is to flip
// all args backwards, so they are pushed left to right.
void reverse_function_argument_order(Function *function) {
    const int MAX_ARGS = 256;

    typedef struct tac_interval {
        Tac *start;
        Tac *end;
    } TacInterval;

    // Need to count this IR's function_call_count
    int function_call_count = make_function_call_count(function);

    // First index, function_id, second index arg_id
    TacInterval *function_args;
    function_args = wmalloc(sizeof(TacInterval) * function_call_count * MAX_ARGS);

    int *arg_counts = wcalloc(function_call_count, sizeof(int));
    Tac **calls = wcalloc(function_call_count, sizeof(Tac *));
    Tac **call_starts = wcalloc(function_call_count, sizeof(Tac *));

    ir = function->ir;

    // Collect function call details in one pass through the IR
    Tac *tac = function->ir;
    while (tac) {
        if (tac->operation == IR_START_CALL) {
            int func = tac->src1->int_value;
            TacInterval *args = &(function_args[func * MAX_ARGS]);
            call_starts[func] = tac;
            tac = tac->next;
            args[arg_counts[func]].start = tac;
        }
        else if (tac->operation == IR_END_CALL) {
            int func = tac->src1->int_value;
            calls[func] = tac->prev;
            tac = tac->next;
        }
        else if (tac->operation == IR_ARG) {
            int func = tac->src1->int_value;
            TacInterval *args = &(function_args[func * MAX_ARGS]);
            args[arg_counts[func]].end = tac;
            tac = tac->next;

            if (tac->operation == IR_ARG_STACK_PADDING) {
                args[arg_counts[func]].end = tac;
                tac = tac->next;
            }

            arg_counts[func]++;
            if (tac->operation != IR_END_CALL) args[arg_counts[func]].start = tac;
        }
        else
            tac = tac->next;
    }

    // Reverse the args for each function call
    for (int i = 0; i < function_call_count; i++) {
        TacInterval *args = &(function_args[i * MAX_ARGS]);
        int arg_count = arg_counts[i];
        Tac *call = calls[i];
        Tac *call_start = call_starts[i];

        if (arg_count > 1) {
            call_start->next = args[arg_count - 1].start;
            args[arg_count - 1].start->prev = call_start;
            args[0].end->next = call;
            call->prev = args[0].end;

            for (int j = 0; j < arg_count; j++) {
                // Rearrange args backwards from this IR
                // cs -> p0.start -> p0.end -> p1.start -> p1.end -> cs.end
                // cs -> p0.start -> p0.end -> p1.start -> p1.end -> p2.start -> p2.end -> cs.end
                if (j < arg_count - 1) {
                    args[j + 1].end->next = args[j].start;
                    args[j].start->prev = args[j + 1].end;
                }
            }
        }

    }

    wfree(function_args);

    wfree(arg_counts);
    wfree(calls);
    wfree(call_starts);
}

// Insert tac instruction before ir
void insert_instruction(Tac *ir, Tac *tac, int move_label) {
    Tac *prev = ir->prev;
    if (!ir->prev) panic("insert_instruction called with null prev");

    tac->origin = ir->origin;

    tac->prev = prev;
    tac->next = ir;
    ir->prev = tac;
    prev->next = tac;

    if (move_label) {
        tac->label = ir->label;
        ir->label = 0;
    }
}

Tac *insert_instruction_from_operation(Tac *ir, int operation, Value *dst, Value *src1, Value *src2, int move_label) {
    Tac *tac = new_instruction(operation);
    tac->dst = dst;
    tac->src1 = src1;
    tac->src2 = src2;
    insert_instruction(ir, tac, move_label);
    return tac;
}

// Append tac to ir
Tac *insert_instruction_after(Tac *ir, Tac *tac) {
    tac->origin = ir->origin;

    Tac *next = ir->next;
    ir->next = tac;
    tac->prev = ir;
    tac->next = next;
    if (next) next->prev = tac;

    return tac;
}

Tac *insert_instruction_after_from_operation(Tac *ir, int operation, Value *dst, Value *src1, Value *src2) {
    Tac *tac = new_instruction(operation);
    tac->dst = dst;
    tac->src1 = src1;
    tac->src2 = src2;

    Tac *next = ir->next;
    ir->next = tac;
    tac->prev = ir;
    tac->next = next;
    if (next) next->prev = tac;

    return tac;
}

// Delete an instruction inside a linked list. It cannot be the first instruction.
Tac *delete_instruction(Tac *tac) {
    if (!tac->prev) panic("Attempt to delete the first instruction");
    if (tac->next && tac->next->label) panic("Deleting an instruction would obliterate the next label");

    tac->prev->next = tac->next;
    if (tac->next) tac->next->prev = tac->prev;
    tac->next->label = tac->label;
    return tac->next;
}

// Renumber all labels so they are consecutive. Uses label_count global.
void renumber_labels(Function *function) {
    // Find highest label number in the function
    int max_label_count = 0;
    for (Tac *tac = function->ir; tac; tac = tac->next) {
        if (tac->src1 && tac->src1->label && tac->src1->label > max_label_count) max_label_count = tac->src1->label;
        if (tac->src2 && tac->src2->label && tac->src2->label > max_label_count) max_label_count = tac->src2->label;
        if (tac->label && tac->label > max_label_count) max_label_count = tac->label;
    }

    if (!max_label_count) return;

    int *mapping = wcalloc(max_label_count + 1, sizeof(int));

    // Make mapping of old to new label indexes
    for (Tac *tac = function->ir; tac; tac = tac->next) {
        if (tac->src1 && tac->src1->label && !mapping[tac->src1->label]) mapping[tac->src1->label] = ++label_count;
        if (tac->src2 && tac->src2->label && !mapping[tac->src2->label]) mapping[tac->src2->label] = ++label_count;
        if (tac->label && !mapping[tac->label]) mapping[tac->label] = ++label_count;
    }

    // Set int_value to one for renamed labels. This is needed since some Value objects
    // are shared, and should not be renamed twice.
    for (Tac *tac = function->ir; tac; tac = tac->next) {
        if (tac->src1 && tac->src1->label && !tac->src1->int_value) { tac->src1->label = mapping[tac->src1->label]; tac->src1->int_value = 1; }
        if (tac->src2 && tac->src2->label && !tac->src2->int_value) { tac->src2->label = mapping[tac->src2->label]; tac->src2->int_value = 1; }
        if (tac->label) tac->label = mapping[tac->label];
    }

    for (Tac *tac = function->ir; tac; tac = tac->next) {
        if (tac->src1 && tac->src1->label) tac->src1->int_value = 0;
        if (tac->src2 && tac->src2->label) tac->src2->int_value = 0;
    }

    wfree(mapping);
}

#define renumber_value(v) { \
    if (v && v->label) { \
        int replacement = (long) longmap_get(renumbers, v->label); \
        if (replacement) v->label = replacement; \
    } \
}

// When the parser completes, each jmp target is a nop with its own label. This can lead
// to consecutive nops with different labels. Merge those nops and labels into one.
// Gather all renumbers in one pass and apply them in a second pass.
void merge_consecutive_labels(Function *function) {
    LongMap *renumbers = new_longmap();

    int i = 0;
    for (Tac *tac = function->ir; tac; tac = tac->next) {
        while(1) {
            if (!tac->label || !tac->next || !tac->next->label) break;

            Tac *deleted_tac = tac->next;
            merge_instructions(tac, i, 1);
            int src = deleted_tac->label;
            int dst = tac->label;

            int renumbered_src = (long) longmap_get(renumbers, src); if (renumbered_src) src = renumbered_src;
            int renumbered_dst = (long) longmap_get(renumbers, dst); if (renumbered_dst) dst = renumbered_dst;
            longmap_put(renumbers, src, (void *) (long) dst);
        }

        i++;
    }

    for (Tac *tac = function->ir; tac; tac = tac->next) {
        renumber_value(tac->dst);
        renumber_value(tac->src1);
        renumber_value(tac->src2);
    }

    free_longmap(renumbers);
}

static void assign_local_to_register(Value *v, int vreg) {
    v->local_index = 0;
    v->is_lvalue = 0;
    v->vreg = vreg;
}

// Set on_stack to one if the value is a long double, struct/union or array
static void set_on_stack(Value *v, char *on_stack) {
    int type = v->type->type;

    if (type == TYPE_LONG_DOUBLE || type == TYPE_STRUCT_OR_UNION || type == TYPE_ARRAY)
        on_stack[-v->local_index] = 1;
}

// The parser allocates a local_index for temporaries and local variables. Allocate
// vregs for them unless any of them is used with an & operator, or are long doubles, in
// which case, they must be on the stack.
// Function parameters aren't touched in this function
void allocate_value_vregs(Function *function) {
    char *on_stack = wcalloc(function->local_symbol_count + 1, sizeof(char));

    for (Tac *tac = function->ir; tac; tac = tac->next) {
        // Keep variables that are used with the & operator on the stack
        if (tac->operation == IR_ADDRESS_OF && tac->src1->local_index < 0) on_stack[-tac->src1->local_index] = 1;

        // Keep long doubles, struct/unions and arrays on the stack
        if (tac->dst  && tac->dst ->type && tac->dst ->local_index < 0) set_on_stack(tac->dst,  on_stack);
        if (tac->src1 && tac->src1->type && tac->src1->local_index < 0) set_on_stack(tac->src1, on_stack);
        if (tac->src2 && tac->src2->type && tac->src2->local_index < 0) set_on_stack(tac->src2, on_stack);
    }

    for (int i = 1; i <= function->local_symbol_count; i++) {
        if (on_stack[i]) continue;
        int vreg = ++function->vreg_count;

        for (Tac *tac = function->ir; tac; tac = tac->next) {
            if (tac->dst  && tac->dst ->local_index == -i) assign_local_to_register(tac->dst , vreg);
            if (tac->src1 && tac->src1->local_index == -i) assign_local_to_register(tac->src1, vreg);
            if (tac->src2 && tac->src2->local_index == -i) assign_local_to_register(tac->src2, vreg);
        }
    }

    wfree(on_stack);
}

// IR_JZ and IR_JNZ aren't implemented in the backend for SSE & long doubles.
// Convert:
// - IR_JZ  => IR_EQ with 0.0 & IR_JNZ
// - IR_JNZ => IR_NE with 0.0 & IR_JNZ
void convert_long_doubles_jz_and_jnz(Function *function) {
    make_vreg_count(function, 0);

    for (Tac *ir = function->ir; ir; ir = ir->next) {
        if ((ir->operation == IR_JZ || ir->operation == IR_JNZ) && is_floating_point_type(ir->src1->type)) {
            ir->operation = ir->operation == IR_JZ ? IR_EQ : IR_NE;
            Tac *tac = new_instruction(IR_JNZ);
            tac->operation = IR_JNZ;
            ir->dst = new_value();
            ir->dst->type = new_type(TYPE_INT);
            ir->dst->vreg = ++function->vreg_count;
            tac->src1 = ir->dst;
            tac->src2 = ir->src2;
            ir->src2 = new_floating_point_constant(ir->src1->type->type, 0.0L);
            ir = insert_instruction_after(ir, tac);
        }
    }
}

// Long double rvalues never live in registers, ensure all of them are on the stack
void move_long_doubles_to_the_stack(Function *function) {
    make_vreg_count(function, 0);
    int *local_indexes = wcalloc(function->vreg_count + 1, sizeof(int));

    for (Tac *tac = function->ir; tac; tac = tac->next) {
        // Long doubles in vregs are moved to the stack
        if (tac->dst  && tac->dst ->vreg && tac->dst ->type && tac->dst ->type->type == TYPE_LONG_DOUBLE && !tac->dst ->is_lvalue && !local_indexes[tac->dst ->vreg])
            local_indexes[tac->dst ->vreg] = new_local_index(function);
        if (tac->src1 && tac->src1->vreg && tac->src1->type && tac->src1->type->type == TYPE_LONG_DOUBLE && !tac->src1->is_lvalue && !local_indexes[tac->src1->vreg])
            local_indexes[tac->src1->vreg] = new_local_index(function);
        if (tac->src2 && tac->src2->vreg && tac->src2->type && tac->src2->type->type == TYPE_LONG_DOUBLE && !tac->src2->is_lvalue && !local_indexes[tac->src2->vreg])
            local_indexes[tac->src2->vreg] = new_local_index(function);
    }

    for (Tac *tac = function->ir; tac; tac = tac->next) {
        if (tac-> dst && tac-> dst->vreg && local_indexes[tac-> dst->vreg]) { tac-> dst->local_index = local_indexes[tac-> dst->vreg]; tac-> dst->vreg = 0; }
        if (tac->src1 && tac->src1->vreg && local_indexes[tac->src1->vreg]) { tac->src1->local_index = local_indexes[tac->src1->vreg]; tac->src1->vreg = 0; }
        if (tac->src2 && tac->src2->vreg && local_indexes[tac->src2->vreg]) { tac->src2->local_index = local_indexes[tac->src2->vreg]; tac->src2->vreg = 0; }
    }

    wfree(local_indexes);
}

void make_stack_register_count(Function *function) {
    int min = 0;
    for (Tac *tac = function->ir; tac; tac = tac->next) {
        // Map registers forced onto the stack due to use of &
        if (tac-> dst && tac-> dst->stack_index < 0 && tac-> dst->stack_index < min) min = tac-> dst->stack_index;
        if (tac->src1 && tac->src1->stack_index < 0 && tac->src1->stack_index < min) min = tac->src1->stack_index;
        if (tac->src2 && tac->src2->stack_index < 0 && tac->src2->stack_index < min) min = tac->src2->stack_index;
    }
    function->stack_register_count = -min;
}

// allocate a stack_index values:
// - without a vreg,
// - used in a & expression,
// - are long double
//
// Pushed variables by the caller have local_index >= 2 and are mapped through.
void allocate_value_stack_indexes(Function *function) {
    int stack_register_count = 0;

    int *stack_index_map = wmalloc((function->local_symbol_count + 1) * sizeof(int));
    memset(stack_index_map, -1, (function->local_symbol_count + 1) * sizeof(int));

    if (debug_ssa_mapping_local_stack_indexes) print_ir(function, 0, 0);

    // Make stack_index_map
    for (Tac *tac = function->ir; tac; tac = tac->next) {
        // Map registers forced onto the stack
        if (tac->dst && tac->dst->local_index < 0 && stack_index_map[-tac->dst->local_index] == -1)
            stack_index_map[-tac->dst->local_index] = stack_register_count++;

        if (tac->src1 && tac->src1->local_index < 0 && stack_index_map[-tac->src1->local_index] == -1)
            stack_index_map[-tac->src1->local_index] = stack_register_count++;

        if (tac->src2 && tac->src2->local_index < 0 && stack_index_map[-tac->src2->local_index] == -1)
            stack_index_map[-tac->src2->local_index] = stack_register_count++;
    }

    for (Tac *tac = function->ir; tac; tac = tac->next) {
        // Map registers forced onto the stack
        if (tac->dst && tac->dst->local_index < 0)
            tac->dst->stack_index = -stack_index_map[-tac->dst->local_index] - 1;
        if (tac->src1 && tac->src1->local_index < 0)
            tac->src1->stack_index = -stack_index_map[-tac->src1->local_index] - 1;
        if (tac->src2 && tac->src2->local_index < 0)
            tac->src2->stack_index = -stack_index_map[-tac->src2->local_index] - 1;

        // Map function call parameters
        if (tac->dst  && tac->dst ->local_index > 0) tac->dst ->stack_index = tac->dst ->local_index;
        if (tac->src1 && tac->src1->local_index > 0) tac->src1->stack_index = tac->src1->local_index;
        if (tac->src2 && tac->src2->local_index > 0) tac->src2->stack_index = tac->src2->local_index;
    }

    // From this point onwards, local_index has no meaning and downstream code must not use it.
    for (Tac *tac = function->ir; tac; tac = tac->next) {
        if (tac->dst ) tac->dst ->local_index = 0;
        if (tac->src1) tac->src1->local_index = 0;
        if (tac->src2) tac->src2->local_index = 0;
    }

    function->stack_register_count = stack_register_count;

    if (debug_ssa_mapping_local_stack_indexes)
        printf("Moved %d registers to stack\n", stack_register_count);

    if (debug_ssa_mapping_local_stack_indexes) print_ir(function, 0, 0);

    wfree(stack_index_map);
}

// If a function returns a value and it's called, the parser will always allocate a
// vreg for the result. This function removes the vreg if it's not used anywhere further
// on.
void remove_unused_function_call_results(Function *function) {
    int vreg_count  = 0;
    for (Tac *tac = function->ir; tac; tac = tac->next) {
        if (tac->dst  && tac->dst ->vreg && tac->dst ->vreg > vreg_count) vreg_count = tac->dst ->vreg;
        if (tac->src1 && tac->src1->vreg && tac->src1->vreg > vreg_count) vreg_count = tac->src1->vreg;
        if (tac->src2 && tac->src2->vreg && tac->src2->vreg > vreg_count) vreg_count = tac->src2->vreg;
    }

    char *used_vregs = wcalloc(vreg_count + 1, sizeof(char));

    Tac *tac = function->ir;
    while (tac->next) tac = tac->next;

    while (tac) {
        if (tac->src1 && tac->src1->vreg) used_vregs[tac->src1->vreg] = 1;
        if (tac->src2 && tac->src2->vreg) used_vregs[tac->src2->vreg] = 1;
        if (tac->operation == IR_MOVE && tac->dst->vreg && tac->dst->is_lvalue) used_vregs[tac->dst->vreg] = 1;
        if (tac->operation == IR_CALL && tac->dst && tac->dst->vreg && !used_vregs[tac->dst->vreg]) tac->dst = 0;

        tac = tac->prev;
    }

    wfree(used_vregs);
}

static Value *insert_address_of_instruction_after(Function *function, Tac **ir, Value *src) {
    Value *v = new_value();

    v->vreg = ++function->vreg_count;
    v->type = make_pointer_to_void();
    *ir = insert_instruction_after_from_operation(*ir, IR_ADDRESS_OF, v, src, 0);

    return v;
}

static Tac *insert_arg_instruction_after(Tac *ir, Value *function_call_value, Value *v, int int_arg_index) {
    Value *arg_value = dup_value(function_call_value);
    arg_value->function_call_arg_index = int_arg_index;


    FunctionParamAllocation *fpa = init_function_param_allocaton(NULL);
    FunctionParamLocations *fpl = wmalloc(sizeof(FunctionParamLocations));
    append_to_list(fpa->param_locations, fpl);

    arg_value->function_call_arg_locations = fpl;
    fpl->locations = wmalloc(sizeof(FunctionParamLocation));
    memset(fpl->locations, -1, sizeof(FunctionParamLocation));
    fpl->count = 1;
    fpl->locations[0].int_register = int_arg_index;
    fpl->locations[0].sse_register = -1;

    return insert_instruction_after_from_operation(ir, IR_ARG, 0, arg_value, v);
}

static Tac *insert_function_call_instructions_after(Tac *ir, Value *call_value, Symbol *symbol) {
    // Add call instruction
    Value *function_value = new_value();
    function_value->int_value = call_value->int_value;
    function_value->function_symbol = symbol;
    function_value->type = symbol->type;
    function_value->function_call_arg_push_count = 0;
    function_value->function_call_sse_register_arg_count = 0;
    call_value->function_call_arg_push_count = 0;
    call_value->function_call_arg_stack_padding = 0;
    ir = insert_instruction_after_from_operation(ir, IR_CALL, 0, function_value, 0);

    // Add end call instruction
    ir = insert_instruction_after_from_operation(ir, IR_END_CALL, 0, call_value, 0);
}

// Add memcpy calls for struct/union -> struct/union copies
Tac *add_memory_copy_with_memcpy(Function *function, Tac *ir, Value *dst, Value *src1, int size) {
    int function_call_count = make_function_call_count(function);

    // Add start call instruction
    Value *call_value = make_function_call_value(function_call_count++);
    ir = insert_instruction_after_from_operation(ir, IR_START_CALL, 0, call_value, 0);

    // Load of addresses of src1, dst & make size value
    Value *src1_value;
    if (src1->is_lvalue && src1->vreg)
        src1_value = src1;
    else
        src1_value = insert_address_of_instruction_after(function, &ir, src1);

    Value *dst_value;
    if (dst->is_lvalue && dst->vreg)
        dst_value = dst;
    else
        dst_value = insert_address_of_instruction_after(function, &ir, dst);

    Value *size_value = new_integral_constant(TYPE_LONG, size);

    // Add arg instructions
    ir = insert_arg_instruction_after(ir, call_value, dst_value, 0);
    ir = insert_arg_instruction_after(ir, call_value, src1_value, 1);
    ir = insert_arg_instruction_after(ir, call_value, size_value, 2);

    // Add function call
    ir = insert_function_call_instructions_after(ir, call_value, memcpy_symbol);

    return ir;
}

// Copy struct/unions using registers
//
// splay_offsets is used when copying a struct with SSE values from the vararg register
// save area. The SSE registers are saved with an annoying offset of 16 instead of 8.
Tac *add_memory_copy_with_registers(Function *function, Tac *ir, Value *dst, Value *src1, int size, int splay_offsets) {
    int dst_offset = dst->offset;
    int src1_offset = src1->offset;

    // Do moves of size 8, 4, 2, 1 in order until there are no moves left
    for (int i = 3; i >= 0; i -= 1) {
        int step = 1 << i;

        while (size >= step) {
            Value *temp_value = new_value();
            temp_value->type = new_type(TYPE_CHAR + i);
            temp_value->vreg = ++function->vreg_count;

            Value *offsetted_dst = dup_value(dst);
            offsetted_dst->offset = dst_offset;
            offsetted_dst->type = temp_value->type;
            Value *offsetted_src1 = dup_value(src1);
            int splayed_offset = (splay_offsets && src1_offset >= 8) ? src1_offset + 8 : src1_offset;
            offsetted_src1->offset = splayed_offset;
            offsetted_src1->type = temp_value->type;

            if (src1->is_lvalue && src1->vreg) {
                // Make it a pointer and add an indirect instruction
                Value *indirect_src1 = dup_value(offsetted_src1);
                indirect_src1->type = make_pointer(temp_value->type);
                indirect_src1->is_lvalue = 0;
                ir = insert_instruction_after_from_operation(ir, IR_INDIRECT, temp_value, indirect_src1, 0);
            } else
                ir = insert_instruction_after_from_operation(ir, IR_MOVE, temp_value, offsetted_src1, 0);

            ir = insert_instruction_after_from_operation(ir, IR_MOVE, offsetted_dst, temp_value, 0);

            size -= step;
            dst_offset += step;
            src1_offset += step;
        }
    }

    return ir;
}

Tac *add_memory_copy(Function *function, Tac *ir, Value *dst, Value *src1, int size) {
    // If either src or dst is an lvalue in a register, or the struct size > 32,
    // use a memory copy. Otherwise use temporary registers to do the copy.
    if (size > 32)
        ir = add_memory_copy_with_memcpy(function, ir, dst, src1, size);
    else
        ir = add_memory_copy_with_registers(function, ir, dst, src1, size, 0);

    return ir;
}

// Add memcpy calls for struct/union -> struct/union copies
void process_struct_and_union_copies(Function *function) {
    for (Tac *ir = function->ir; ir; ir = ir->next) {
        if (ir->operation != IR_MOVE) continue;
        if (ir->src1->type->type != TYPE_STRUCT_OR_UNION) continue;
        if (ir->dst->type->type != ir->src1->type->type) panic("Mismatched struct/union copy type");

        int size = get_type_size(ir->dst->type);
        Value *dst = ir->dst;
        Value *src1 = ir->src1;

        ir->operation = IR_NOP;
        ir->dst = 0;
        ir->src1 = 0;

        add_memory_copy(function, ir, dst, src1, size);
    }
}

// Determine offset (in bytes), which has integer alignment.
// Within the alignment, determine the offset (in bits) and size (in bits)
void determine_bit_field_params(Value *v, int *offset, int *bit_offset, int *bit_size) {
    *offset = v->offset & (~3); // Align to the nearest integer boundary
    *bit_offset = v->bit_field_offset - (*offset << 3);
    *bit_size = v->bit_field_size;
}

// Load a bit field by loading the entire integer from a struct and bit shifting
static void add_load_bit_field(Function *function, Tac *ir) {
    int offset, bit_offset, bit_size;
    determine_bit_field_params(ir->src1, &offset, &bit_offset, &bit_size);

    Value *src1 = dup_value(ir->src1);
    src1->offset = offset;
    src1->bit_field_offset = 0;
    src1->bit_field_size = 0;

    Value *dst = ir->dst;
    Value *loaded_value = dup_value(ir->dst);
    loaded_value->vreg = ++function->vreg_count;

    ir->operation = IR_NOP;
    ir->dst = 0;
    ir->src1 = 0;
    ir->src2 = 0;

    if (src1->is_lvalue && src1->vreg) {
        // Make it a pointer and add an indirect instruction
        Value *indirect_src1 = dup_value(src1);
        indirect_src1->type = make_pointer(loaded_value->type);
        indirect_src1->is_lvalue = 0;
        ir = insert_instruction_after_from_operation(ir, IR_INDIRECT, loaded_value, indirect_src1, 0);
    } else
        ir = insert_instruction_after_from_operation(ir, IR_MOVE, loaded_value, src1, 0);

    if (src1->type->is_unsigned) {
        // Shift right
        Value *shifted_value;
        if (bit_offset) {
            shifted_value = dup_value(loaded_value);
            shifted_value->vreg = ++function->vreg_count;
            ir = insert_instruction_after_from_operation(ir, IR_BSHR, shifted_value, loaded_value, new_integral_constant(TYPE_INT, bit_offset));
        }
        else
            shifted_value = loaded_value;

        // Apply mask
        unsigned int mask = (1 << bit_size) - 1;
        ir = insert_instruction_after_from_operation(ir, IR_BAND, dst, shifted_value, new_integral_constant(TYPE_INT, mask));
    }
    else {
        // Shift left so that the value is at the end of the integer boundary
        Value *shifted_value;
        int left_shift_offset = 32 - bit_size - bit_offset;
        if (left_shift_offset) {
            shifted_value = dup_value(loaded_value);
            shifted_value->vreg = ++function->vreg_count;
            ir = insert_instruction_after_from_operation(ir, IR_BSHL, shifted_value, loaded_value, new_integral_constant(TYPE_INT, left_shift_offset));
        }
        else
            shifted_value = loaded_value;

        // Sign extend by shifting right
        if (32 - bit_size)
            ir = insert_instruction_after_from_operation(ir, IR_ASHR, dst, shifted_value, new_integral_constant(TYPE_INT, 32 - bit_size));
        else
            dst = shifted_value;
    }
}

// Save an integer to a bit field
static void add_save_bit_field(Function *function, Tac *ir) {
    int offset, bit_offset, bit_size;
    determine_bit_field_params(ir->dst, &offset, &bit_offset, &bit_size);

    Value *dst = dup_value(ir->dst);;
    Value *src1 = dup_value(ir->src1);

    ir->operation = IR_NOP;
    ir->dst = 0;
    ir->src1 = 0;
    ir->src2 = 0;

    // Load the src into an unsigned integer
    src1->type->is_unsigned = 1;
    Value *loaded_src1 = new_value();
    loaded_src1->type = new_type(TYPE_INT);
    loaded_src1->type->is_unsigned = 1;
    loaded_src1->vreg = ++function->vreg_count;
    ir = insert_instruction_after_from_operation(ir, IR_MOVE, loaded_src1, src1, 0);

    // Set bits outside of the src value to zero
    Value *masked_src1 = dup_value(loaded_src1);
    masked_src1->vreg = ++function->vreg_count;
    unsigned int mask = bit_size == 32 ? -1 : (1 << bit_size) - 1;
    Value *mask_value = new_integral_constant(TYPE_INT, mask);
    mask_value->type->is_unsigned = 1;
    ir = insert_instruction_after_from_operation(ir, IR_BAND, masked_src1, loaded_src1, mask_value);

    // Shift the src over to the right spot
    Value *shifted_src1;
    if (bit_offset) {
        shifted_src1 = dup_value(masked_src1);
        shifted_src1->vreg = ++function->vreg_count;
        ir = insert_instruction_after_from_operation(ir, IR_BSHL, shifted_src1, masked_src1, new_integral_constant(TYPE_INT, bit_offset));
    }
    else
        shifted_src1 = masked_src1;

    // Load the dst
    Value *loaded_dst = new_value();
    loaded_dst->type = new_type(TYPE_INT);
    loaded_dst->type->is_unsigned = 1;
    loaded_dst->vreg = ++function->vreg_count;

    dst->type->is_unsigned = 1;
    dst->offset = offset;
    dst->bit_field_offset = 0;
    dst->bit_field_size = 0;

    if (dst->is_lvalue && dst->vreg) {
        // Make it a pointer and add an indirect instruction
        Value *indirect_dst = dup_value(dst);
        indirect_dst->type = make_pointer(loaded_dst->type);
        indirect_dst->is_lvalue = 0;
        ir = insert_instruction_after_from_operation(ir, IR_INDIRECT, loaded_dst, indirect_dst, 0);
    } else
        ir = insert_instruction_after_from_operation(ir, IR_MOVE, loaded_dst, dst, 0);

    // Set the destination bits to zero on the loaded dst
    unsigned int inverted_shifted_mask = ~(mask << bit_offset);
    Value *masked_dst = dup_value(loaded_dst);
    masked_dst->vreg = ++function->vreg_count;
    Value *inverted_shifted_mask_value = new_integral_constant(TYPE_INT, inverted_shifted_mask);
    inverted_shifted_mask_value->type->is_unsigned = 1;
    ir = insert_instruction_after_from_operation(ir, IR_BAND, masked_dst, loaded_dst, inverted_shifted_mask_value);

    // Or two values
    Value *orred_dst = dup_value(loaded_dst);
    orred_dst->vreg = ++function->vreg_count;
    ir = insert_instruction_after_from_operation(ir, IR_BOR, orred_dst, masked_dst, shifted_src1);

    // Store the result in dst
    ir = insert_instruction_after_from_operation(ir, IR_MOVE, dst, orred_dst, 0);
}

// Add instructions for loading and saving bit fields
void process_bit_fields(Function *function) {
    for (Tac *ir = function->ir; ir; ir = ir->next) {
        if (ir->operation == IR_LOAD_BIT_FIELD) add_load_bit_field(function, ir);
        else if (ir->operation == IR_SAVE_BIT_FIELD) add_save_bit_field(function, ir);
    }
}

// Ensure any enums that make it to the instruction selection are convertd to TYPE_INT
void convert_enums(Function *function) {
    for (Tac *tac = function->ir; tac; tac = tac->next) {
        if (tac->dst  && tac->dst ->type && tac->dst ->type->type == TYPE_ENUM) tac->dst ->type = new_type(TYPE_INT);
        if (tac->src1 && tac->src1->type && tac->src1->type->type == TYPE_ENUM) tac->src1->type = new_type(TYPE_INT);
        if (tac->src2 && tac->src2->type && tac->src2->type->type == TYPE_ENUM) tac->src2->type = new_type(TYPE_INT);

        if (tac->dst  && tac->dst ->type && tac->dst ->type->type == TYPE_PTR && tac->dst ->type->target->type == TYPE_ENUM ) tac->dst ->type = make_pointer(new_type(TYPE_INT));
        if (tac->src1 && tac->src1->type && tac->src1->type->type == TYPE_PTR && tac->src1->type->target->type == TYPE_ENUM ) tac->src1->type = make_pointer(new_type(TYPE_INT));
        if (tac->src2 && tac->src2->type && tac->src2->type->type == TYPE_PTR && tac->src2->type->target->type == TYPE_ENUM ) tac->src2->type = make_pointer(new_type(TYPE_INT));
    }
}

// Add instructions that write zeroes to dst, starting at offset, with size in src1
void add_zero_memory_instructions(Function *function) {
    for (Tac *tac = function->ir; tac; tac = tac->next) {
        if (tac->operation != IR_ZERO) continue;

        Value *dst = tac->dst;
        int offset = tac->dst->offset;
        int size = tac->src1->int_value;
        tac->operation = IR_NOP;
        tac->dst = 0;
        tac->src1 = 0;

        Value *zero = new_integral_constant(TYPE_INT, 0);

        if (size < 32) {
            // Write zeros directly
            for (int i = 3; i >= 0; i -= 1) {
                int step = 1 << i;

                while (size >= step) {
                    Value *tmp = dup_value(dst);
                    tmp->offset = offset;
                    tmp->type = new_type(TYPE_CHAR + i);
                    insert_instruction_from_operation(tac, IR_MOVE, tmp, zero, 0, 1);

                    size -= step;
                    offset += step;
                }
            }
        }
        else {
            // Call memset
            int function_call_count = make_function_call_count(function);

            // Add start call instruction
            Value *call_value = make_function_call_value(function_call_count++);
            ir = insert_instruction_after_from_operation(ir, IR_START_CALL, 0, call_value, 0);

            Value *size_value = new_integral_constant(TYPE_INT, size);
            Value *dst_address = insert_address_of_instruction_after(function, &ir, dst);
            ir = insert_arg_instruction_after(ir, call_value, dst_address, 0);
            ir = insert_arg_instruction_after(ir, call_value, zero, 1);
            ir = insert_arg_instruction_after(ir, call_value, size_value, 2);

            // Add function call
            ir = insert_function_call_instructions_after(ir, call_value, memset_symbol);
        }
    }
}

// Add an instruction to load a value from the GOT. The result is a pointer to the
// value.
static Value *add_load_from_got(Function *function, Tac *tac, Value *src) {
    Value *dst = new_value();
    dst->vreg = ++function->vreg_count;
    dst->type = make_pointer(src->type);
    Value *got_src = dup_value(src);
    got_src->type = dst->type;
    got_src->load_from_got = 1;

    insert_instruction_from_operation(tac, IR_LOAD_FROM_GOT, dst, got_src, 0, 0);

    dst->offset = src->offset;
    src->offset = 0;
    got_src->offset = 0;

    return dst;
}

// When compiled with Position Independent Code (PIC) , all accesses to global objects
// go through a Global Offset Table (GOT). The GOT table has pointers to the globals.
// Any load/save must first load a pointer to the global object from the GOT and use
// that rather than going directly to memory.
void add_PIC_load_and_saves(Function *function) {
    if (!opt_PIC) return;

    for (Tac *tac = function->ir; tac; tac = tac->next) {
        // Ensure all global values are only ever loaded or saved

        if (tac->dst && tac->dst->global_symbol && tac->operation != IR_MOVE)
            panic("Unexpected global_symbol in dst");
        if (tac->src1 && tac->src1->global_symbol
                && tac->operation != IR_MOVE
                && tac->operation != IR_CALL
                && tac->operation != IR_ADDRESS_OF
                && tac->operation != IR_DECL_LOCAL_COMP_OBJ)
            panic("Unexpected operation for global_symbol in src1: %d", tac->operation);
        if (tac->src2 && tac->src2->global_symbol)
            panic("Unexpected operation for global_symbol in src2: %d", tac->operation);

        // Convert a save of a global to a mov from the GOT followed by a store to a pointer in a register
        if (tac->dst && tac->dst->global_symbol && tac->operation == IR_MOVE) {
            Value *dst = add_load_from_got(function, tac, tac->dst);

            // Convert to a move to an lvalue in a register
            Type *type = tac->dst->type;
            tac->dst = dup_value(dst);
            tac->dst->is_lvalue = 1;
            tac->dst->type = type;
        }

        // Convert a load of a global to a mov from the GOT followed by an indirect
        if (tac->src1 && tac->src1->global_symbol && tac->operation == IR_MOVE) {
            Value *dst = add_load_from_got(function, tac, tac->src1);

            // Convert to an indirect unless it's a pointer to a function
            tac->src1 = dst;
            if (!is_pointer_to_function_type(tac->src1->type))
                tac->operation = IR_INDIRECT;
        }

        else if (tac->src1 && tac->src1->global_symbol && tac->operation == IR_ADDRESS_OF) {
            tac->operation = IR_ADDRESS_OF_FROM_GOT;
            tac->src1->load_from_got = 1;
            if (tac->src1->offset) {
                int offset = tac->src1->offset;
                tac->src1->offset = 0;
                Value *dst_with_offset = dup_value(tac->dst);
                tac->dst = dup_value(tac->dst);
                tac->dst->vreg = ++function->vreg_count;
                tac = insert_instruction_after_from_operation(tac, IR_ADD, dst_with_offset, tac->dst, new_integral_constant(TYPE_INT, offset));
            }
        }
    }
}

// Convert and loads of address to functions to a load the GOT, unless the
// function has been defined.
void convert_functions_address_of(Function *function) {
    for (Tac *tac = function->ir; tac; tac = tac->next) {
        if (tac->operation == IR_ADDRESS_OF && tac->src1->global_symbol && tac->src1->type->type == TYPE_FUNCTION) {

            int is_defined = tac->src1->function_symbol && tac->src1->function_symbol->function && tac->src1->function_symbol->function->is_defined;

            if (!is_defined) {
                tac->operation = IR_ADDRESS_OF_FROM_GOT;
                tac->src1->load_from_got = 1;
            }
        }
    }
}
