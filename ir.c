#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "wc4.h"

// Allocate a new virtual register
int new_vreg() {
    vreg_count++;
    if (vreg_count >= MAX_VREG_COUNT) panic1d("Exceeded max vreg count %d", MAX_VREG_COUNT);
    return vreg_count;
}

// A useful function for debugging
void print_value_stack() {
    Value **lvs, *v;

    printf("%-4s %-4s %-4s %-11s %-11s %-5s\n", "type", "vreg", "preg", "global_sym", "local_index", "is_lv");
    lvs = vs;
    while (lvs != vs_start) {
        v = *lvs;
        printf("%-4d %-4d %-4d %-11s %-11d %-5d\n",
            v->type, v->vreg, v->preg, v->global_symbol ? v->global_symbol->identifier : 0, v->local_index, v->is_lvalue);
        lvs++;
    }
}

 void init_value(Value *v) {
    v->preg = -1;
    v->stack_index = 0;
    v->ssa_subscript = -1;
    v->live_range = -1;
}

Value *new_value() {
    Value *v;

    v = malloc(sizeof(Value));
    memset(v, 0, sizeof(Value));
    init_value(v);

    return v;
}

// Create a new typed constant value
Value *new_constant(int type, long value) {
    Value *cv;

    cv = new_value();
    cv->value = value;
    cv->type = type;
    cv->is_constant = 1;
    return cv;
}

// Duplicate a value
Value *dup_value(Value *src) {
    Value *dst;

    dst = new_value();
    dst->type                      = src->type;
    dst->vreg                      = src->vreg;
    dst->preg                      = src->preg;
    dst->is_lvalue                 = src->is_lvalue;
    dst->stack_index               = src->stack_index;
    dst->local_index               = src->local_index;
    dst->is_constant               = src->is_constant;
    dst->is_string_literal         = src->is_string_literal;
    dst->is_in_cpu_flags           = src->is_in_cpu_flags;
    dst->string_literal_index      = src->string_literal_index;
    dst->value                     = src->value;
    dst->function_symbol           = src->function_symbol;
    dst->function_call_arg_count   = src->function_call_arg_count;
    dst->global_symbol             = src->global_symbol;
    dst->label                     = src->label;
    dst->ssa_subscript             = src->ssa_subscript;
    dst->live_range                = src->live_range;
    dst->x86_size                  = src->x86_size;

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
    Tac *tac;

    tac = malloc(sizeof(Tac));
    memset(tac, 0, sizeof(Tac));
    tac->operation = operation;

    return tac;
}

// Add instruction to the global intermediate representation ir
Tac *add_instruction(int operation, Value *dst, Value *src1, Value *src2) {
    Tac *tac;

    tac = new_instruction(operation);
    tac->label = 0;
    tac->dst = dst;
    tac->src1 = src1;
    tac->src2 = src2;
    tac->next = 0;
    tac->prev = 0;

    add_tac_to_ir(tac);

    return tac;
}

// Ensure the double linked list in an IR is correct by checking last pointers
void sanity_test_ir_linkage(Tac *ir) {
    Tac *tac;

    tac = ir;
    while (tac) {
        if (tac->next && tac->next->prev != tac) {
            printf("Linkage broken between:\n");
            print_instruction(stdout, tac);
            print_instruction(stdout, tac->next);
            panic("Bailing");
        }

        tac = tac->next;
    }
}

void fprintf_escaped_string_literal(void *f, char* sl) {
    fprintf(f, "\"");
    while (*sl) {
             if (*sl == '\n') fprintf(f, "\\n");
        else if (*sl == '\t') fprintf(f, "\\t");
        else if (*sl == '\\') fprintf(f, "\\\\");
        else if (*sl == '"')  fprintf(f, "\\\"");
        else                  fprintf(f, "%c", *sl);
        sl++;
    }
    fprintf(f, "\"");
}

void print_value(void *f, Value *v, int is_assignment_rhs) {
    int type;

    if (!v) panic("print_value got null");

    if (is_assignment_rhs && !v->is_lvalue && (v->global_symbol || v->local_index)) fprintf(f, "&");
    if (!is_assignment_rhs && v->is_lvalue && !(v->global_symbol || v->local_index)) fprintf(f, "L");

    if (v->is_constant)
        fprintf(f, "%ld", v->value);
    else if (v->is_in_cpu_flags)
        fprintf(f, "cpu");
    else if (v->preg != -1)
        fprintf(f, "p%d", v->preg);
    else if (v->stack_index)
        fprintf(f, "S[%d]", v->stack_index);
    else if (v->vreg) {
        fprintf(f, "r%d", v->vreg);
        if (v->ssa_subscript != -1) fprintf(f, "_%d", v->ssa_subscript);
    }
    else if (v->local_index)
        fprintf(f, "s[%d]", v->local_index);
    else if (v->global_symbol)
        fprintf(f, "%s", v->global_symbol->identifier);
    else if (v->is_string_literal)
        fprintf_escaped_string_literal(f, string_literals[v->string_literal_index]);
    else if (v->label)
        fprintf(f, "l%d", v->label);
    else if (v->function_symbol) {
        fprintf(f, "function:%s", v->function_symbol->identifier);
        return;
    }
    else
        // What's this?
        fprintf(f, "?%ld", v->value);

    if (!v->label) {
        fprintf(f, ":");

        if (v->x86_size)
            fprintf(f, "%d", v->x86_size);
        else {
            type = v->type;
            while (type >= TYPE_PTR) {
                fprintf(f, "*");
                type -= TYPE_PTR;
            }

                 if (type == TYPE_VOID)   fprintf(f, "void");
            else if (type == TYPE_CHAR)   fprintf(f, "char");
            else if (type == TYPE_INT)    fprintf(f, "int");
            else if (type == TYPE_SHORT)  fprintf(f, "short");
            else if (type == TYPE_LONG)   fprintf(f, "long");
            else if (type >= TYPE_STRUCT) fprintf(f, "struct %s", all_structs[type - TYPE_STRUCT]->identifier);
            else fprintf(f, "unknown type %d", type);
        }
    }
}

void print_instruction(void *f, Tac *tac) {
    int first;
    char *buffer;
    Value *v;

    if (tac->label)
        fprintf(f, "l%-5d", tac->label);
    else
        fprintf(f, "      ");

    if (tac->x86_template) {
        buffer = render_x86_operation(tac, 0, 0, 0);
        fprintf(f, "%s\n", buffer);
        free(buffer);
        return;
    }

    if (tac->dst && tac->operation < X_START) {
        print_value(f, tac->dst, tac->operation != IR_ASSIGN);
        fprintf(f, " = ");
    }

    if (tac->operation == IR_LOAD_CONSTANT || tac->operation == IR_LOAD_VARIABLE || tac->operation == IR_LOAD_STRING_LITERAL) {
        print_value(f, tac->src1, 1);
    }

    else if (tac->operation == IR_START_CALL) fprintf(f, "start call %ld", tac->src1->value);
    else if (tac->operation == IR_END_CALL) fprintf(f, "end call %ld", tac->src1->value);

    else if (tac->operation == IR_ARG || tac->operation == X_ARG) {
        fprintf(f, "arg for call %ld ", tac->src1->value);
        print_value(f, tac->src2, 1);
    }

    else if (tac->operation == IR_CALL) {
        fprintf(f, "call \"%s\" with %d params", tac->src1->function_symbol->identifier, tac->src1->function_call_arg_count);
    }

    else if (tac->operation == IR_RETURN) {
        if (tac->src1) {
            fprintf(f, "return ");
            print_value(f, tac->src1, 1);
        }
        else
            fprintf(f, "return");
    }

    else if (tac->operation == IR_START_LOOP) fprintf(f, "start loop par=%ld loop=%ld", tac->src1->value, tac->src2->value);
    else if (tac->operation == IR_END_LOOP)   fprintf(f, "end loop par=%ld loop=%ld",   tac->src1->value, tac->src2->value);

    else if (tac->operation == IR_ASSIGN)
        print_value(f, tac->src1, 1);

    else if (tac->operation == IR_ASSIGN_TO_REG_LVALUE) {
        print_value(f, tac->src1, 0);
        fprintf(f, " = ");
        print_value(f, tac->src2, 1);
        fprintf(f, " [assign to lvalue]");
    }

    else if (tac->operation == IR_NOP)
        fprintf(f, "nop");

    else if (tac->operation == IR_JZ || tac->operation == IR_JNZ) {
             if (tac->operation == IR_JZ)  fprintf(f, "jz ");
        else if (tac->operation == IR_JNZ) fprintf(f, "jnz ");
        print_value(f, tac->src1, 1);
        fprintf(f, ", ");
        print_value(f, tac->src2, 1);
    }

    else if (tac->operation == IR_JMP)
        fprintf(f, "jmp l%d", tac->src1->label);

    else if (tac->operation == IR_PHI_FUNCTION) {
        fprintf(f, "Φ(");
        v = tac->phi_values;
        first = 1;
        while (v->type) {
            if (!first) fprintf(f, ", ");
            print_value(f, v, 1);
            first = 0;
            v++;
        }
        fprintf(f, ")");
    }

    else if (tac->operation == IR_INDIRECT)      {                               fprintf(f, "*");    print_value(f, tac->src1, 1); }
    else if (tac->operation == IR_ADD)           { print_value(f, tac->src1, 1); fprintf(f, " + ");  print_value(f, tac->src2, 1); }
    else if (tac->operation == IR_SUB)           { print_value(f, tac->src1, 1); fprintf(f, " - ");  print_value(f, tac->src2, 1); }
    else if (tac->operation == IR_RSUB)          { print_value(f, tac->src2, 1); fprintf(f, " - ");  print_value(f, tac->src1, 1); fprintf(f, " [reverse]"); }
    else if (tac->operation == IR_MUL)           { print_value(f, tac->src1, 1); fprintf(f, " * ");  print_value(f, tac->src2, 1); }
    else if (tac->operation == IR_DIV)           { print_value(f, tac->src1, 1); fprintf(f, " / ");  print_value(f, tac->src2, 1); }
    else if (tac->operation == IR_MOD)           { print_value(f, tac->src1, 1); fprintf(f, " %% "); print_value(f, tac->src2, 1); }
    else if (tac->operation == IR_BNOT)          {                               fprintf(f, "~");    print_value(f, tac->src1, 1); }
    else if (tac->operation == IR_EQ)            { print_value(f, tac->src1, 1); fprintf(f, " == "); print_value(f, tac->src2, 1); }
    else if (tac->operation == IR_NE)            { print_value(f, tac->src1, 1); fprintf(f, " != "); print_value(f, tac->src2, 1); }
    else if (tac->operation == IR_GT)            { print_value(f, tac->src1, 1); fprintf(f, " > ");  print_value(f, tac->src2, 1); }
    else if (tac->operation == IR_LT)            { print_value(f, tac->src1, 1); fprintf(f, " < ");  print_value(f, tac->src2, 1); }
    else if (tac->operation == IR_GE)            { print_value(f, tac->src1, 1); fprintf(f, " >= "); print_value(f, tac->src2, 1); }
    else if (tac->operation == IR_LE)            { print_value(f, tac->src1, 1); fprintf(f, " <= "); print_value(f, tac->src2, 1); }
    else if (tac->operation == IR_BAND)          { print_value(f, tac->src1, 1); fprintf(f, " & ");  print_value(f, tac->src2, 1); }
    else if (tac->operation == IR_BOR)           { print_value(f, tac->src1, 1); fprintf(f, " | ");  print_value(f, tac->src2, 1); }
    else if (tac->operation == IR_XOR)           { print_value(f, tac->src1, 1); fprintf(f, " ^ ");  print_value(f, tac->src2, 1); }
    else if (tac->operation == IR_BSHL)          { print_value(f, tac->src1, 1); fprintf(f, " << "); print_value(f, tac->src2, 1); }
    else if (tac->operation == IR_BSHR)          { print_value(f, tac->src1, 1); fprintf(f, " >> "); print_value(f, tac->src2, 1); }

    else if (tac->operation == X_RET)    { fprintf(f, "ret "   ); print_value(f, tac->src1, 1); }
    else if (tac->operation == X_CALL)   { fprintf(f, "call "  ); print_value(f, tac->src1, 1); }
    else if (tac->operation == X_LEA)    { fprintf(f, "lea "   ); print_value(f, tac->src1, 1); fprintf(f, ", "); print_value(f, tac->dst,  1); }
    else if (tac->operation == X_MOV)    { fprintf(f, "mov "   ); print_value(f, tac->src1, 1); fprintf(f, ", "); print_value(f, tac->dst,  1); }
    else if (tac->operation == X_ADD)    { fprintf(f, "add "   ); print_value(f, tac->src1, 1); fprintf(f, ", "); print_value(f, tac->dst,  1); }
    else if (tac->operation == X_MUL)    { fprintf(f, "mul "   ); print_value(f, tac->src1, 1); fprintf(f, ", "); print_value(f, tac->dst,  1); }
    else if (tac->operation == X_IDIV)   { fprintf(f, "idiv "  ); print_value(f, tac->src1, 1); fprintf(f, ", "); print_value(f, tac->dst,  1); }
    else if (tac->operation == X_CQTO)   { fprintf(f, "cqto "  ); print_value(f, tac->src1, 1); fprintf(f, ", "); print_value(f, tac->dst,  1); }
    else if (tac->operation == X_CMP)    { fprintf(f, "cmp "   ); print_value(f, tac->src1, 1); fprintf(f, ", "); print_value(f, tac->src2, 1); }
    else if (tac->operation == X_CMPZ)   { fprintf(f, "cmpz "  ); fprintf(f, "0");              fprintf(f, ", "); print_value(f, tac->src1, 1); }
    else if (tac->operation == X_JMP)    { fprintf(f, "jmp "   ); print_value(f, tac->src1, 1); }
    else if (tac->operation == X_JZ)     { fprintf(f, "jz "    ); print_value(f, tac->src1, 1); }
    else if (tac->operation == X_JNZ)    { fprintf(f, "jnz "   ); print_value(f, tac->src1, 1); }
    else if (tac->operation == X_JE)     { fprintf(f, "je "    ); print_value(f, tac->src1, 1); }
    else if (tac->operation == X_JNE)    { fprintf(f, "jne "   ); print_value(f, tac->src1, 1); }
    else if (tac->operation == X_JLT)    { fprintf(f, "jlt "   ); print_value(f, tac->src1, 1); }
    else if (tac->operation == X_JGT)    { fprintf(f, "jgt "   ); print_value(f, tac->src1, 1); }
    else if (tac->operation == X_JLE)    { fprintf(f, "jle "   ); print_value(f, tac->src1, 1); }
    else if (tac->operation == X_JGE)    { fprintf(f, "jge "   ); print_value(f, tac->src1, 1); }
    else if (tac->operation == X_MOVZBQ) { fprintf(f, "movzbq "); print_value(f, tac->src1, 1); }
    else if (tac->operation == X_SETE)   { fprintf(f, "sete "  ); print_value(f, tac->src1, 1); }
    else if (tac->operation == X_SETNE)  { fprintf(f, "setne " ); print_value(f, tac->src1, 1); }
    else if (tac->operation == X_SETLT)  { fprintf(f, "setlt " ); print_value(f, tac->src1, 1); }
    else if (tac->operation == X_SETGT)  { fprintf(f, "setgt " ); print_value(f, tac->src1, 1); }
    else if (tac->operation == X_SETLE)  { fprintf(f, "setle " ); print_value(f, tac->src1, 1); }
    else if (tac->operation == X_SETGE)  { fprintf(f, "setge " ); print_value(f, tac->src1, 1); }
    else if (tac->operation == X_MOVSBW) { fprintf(f, "movsbw "); print_value(f, tac->src1, 1); fprintf(f, ", "); print_value(f, tac->src2, 1); }
    else if (tac->operation == X_MOVSBL) { fprintf(f, "movsbl "); print_value(f, tac->src1, 1); fprintf(f, ", "); print_value(f, tac->src2, 1); }
    else if (tac->operation == X_MOVSBQ) { fprintf(f, "movsbq "); print_value(f, tac->src1, 1); fprintf(f, ", "); print_value(f, tac->src2, 1); }
    else if (tac->operation == X_MOVSWL) { fprintf(f, "movswl "); print_value(f, tac->src1, 1); fprintf(f, ", "); print_value(f, tac->src2, 1); }
    else if (tac->operation == X_MOVSWQ) { fprintf(f, "movswq "); print_value(f, tac->src1, 1); fprintf(f, ", "); print_value(f, tac->src2, 1); }
    else if (tac->operation == X_MOVSLQ) { fprintf(f, "movslq "); print_value(f, tac->src1, 1); fprintf(f, ", "); print_value(f, tac->src2, 1); }

    else
        panic1d("print_instruction(): Unknown operation: %d", tac->operation);

    fprintf(f, "\n");
}

void print_ir(Function *function, char *name) {
    Tac *tac;
    int i;

    if (name) fprintf(stdout, "%s:\n", name);
    i = 0;
    tac = function->ir;
    while (tac) {
        fprintf(stdout, "%-4d > ", i++);
        print_instruction(stdout, tac);
        tac = tac->next;
    }
    fprintf(stdout, "\n");
}

// Merge tac with the instruction after it. The next instruction is removed from the chain.
void merge_instructions(Tac *tac, int ir_index, int allow_labelled_next) {
    int i, label;
    Tac *next;

    if (!tac->next) panic("merge_instructions called on a tac without next\n");
    if (tac->next->label && !allow_labelled_next) panic("merge_instructions called on a tac with a label");

    next = tac->next;
    tac->next = next->next;
    if (tac->next) tac->next->prev = tac;

    // Nuke the references to the removed node to prevent confusion
    next->prev = 0;
    next->next = 0;
}

// The arguments are pushed onto the stack right to left, but the ABI requries
// the seventh arg and later to be pushed in reverse order. Easiest is to flip
// all args backwards, so they are pushed left to right.
void reverse_function_argument_order(Symbol *function) {
    Tac *tac, *call_start, *call;
    int i, j, arg_count, function_call_count;

    TacInterval *args;

    ir = function->function->ir;
    args = malloc(sizeof(TacInterval *) * 256);

    // Need to count this IR's function_call_count
    function_call_count = 0;
    tac = function->function->ir;
    while (tac) {
        if (tac->operation == IR_START_CALL) function_call_count++;
        tac = tac->next;
    }

    for (i = 0; i < function_call_count; i++) {
        tac = function->function->ir;
        arg_count = 0;
        call_start = 0;
        call = 0;
        while (tac) {
            if (tac->operation == IR_START_CALL && tac->src1->value == i) {
                call_start = tac;
                tac = tac->next;
                args[arg_count].start = tac;
            }
            else if (tac->operation == IR_END_CALL && tac->src1->value == i) {
                call = tac->prev;
                tac = tac->next;
            }
            else if (tac->operation == IR_ARG && tac->src1->value == i) {
                args[arg_count].end = tac;
                arg_count++;
                tac = tac->next;
                if (tac->operation != IR_END_CALL) args[arg_count].start = tac;
            }
            else
                tac = tac->next;
        }

        if (arg_count > 1) {
            call_start->next = args[arg_count - 1].start;
            args[arg_count - 1].start->prev = call_start;
            args[0].end->next = call;
            call->prev = args[0].end;

            for (j = 0; j < arg_count; j++) {
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

    free(args);
}

void assign_local_to_register(Value *v, int vreg) {
    v->local_index = 0;
    v->is_lvalue = 0;
    v->vreg = vreg;
}

void assign_locals_to_registers(Symbol *function) {
    Tac *tac;
    int i, vreg;

    int *has_address_of;

    has_address_of = malloc(sizeof(int) * (function->function->local_symbol_count + 1));
    memset(has_address_of, 0, sizeof(int) * (function->function->local_symbol_count + 1));

    tac = function->function->ir;
    while (tac) {
        if (tac->dst  && !tac->dst ->is_lvalue && tac->dst ->local_index < 0) has_address_of[-tac->dst ->local_index] = 1;
        if (tac->src1 && !tac->src1->is_lvalue && tac->src1->local_index < 0) has_address_of[-tac->src1->local_index] = 1;
        if (tac->src2 && !tac->src2->is_lvalue && tac->src2->local_index < 0) has_address_of[-tac->src2->local_index] = 1;
        tac = tac ->next;
    }

    for (i = 1; i <= function->function->local_symbol_count; i++) {
        if (has_address_of[i]) continue;
        vreg = ++vreg_count;

        tac = function->function->ir;
        while (tac) {
            if (tac->dst  && tac->dst ->local_index == -i) assign_local_to_register(tac->dst , vreg);
            if (tac->src1 && tac->src1->local_index == -i) assign_local_to_register(tac->src1, vreg);
            if (tac->src2 && tac->src2->local_index == -i) assign_local_to_register(tac->src2, vreg);
            if (tac->operation == IR_LOAD_VARIABLE && tac->src1->vreg == vreg)
                tac->operation = IR_ASSIGN;

            tac = tac ->next;
        }
    }

    function->function->vreg_count = vreg_count;
}

void renumber_ir_vreg(Tac *ir, int src, int dst) {
    Tac *tac;

    if (src == 0 || dst == 0) panic("Unexpected zero reg renumber");

    tac = ir;
    while (tac) {
        if (tac->dst  && tac->dst ->vreg == src) tac->dst ->vreg = dst;
        if (tac->src1 && tac->src1->vreg == src) tac->src1->vreg = dst;
        if (tac->src2 && tac->src2->vreg == src) tac->src2->vreg = dst;
        tac = tac->next;
    }
}

void swap_ir_registers(Tac *ir, int vreg1, int vreg2) {
    renumber_ir_vreg(ir, vreg1, -2);
    renumber_ir_vreg(ir, vreg2, vreg1);
    renumber_ir_vreg(ir, -2, vreg2);
}

Tac *insert_instruction(Tac *ir, int ir_index, Tac *tac) {
    int i;
    Tac *prev;

    prev = ir->prev;
    tac->prev = prev;
    tac->next = ir;
    ir->prev = tac;
    prev->next = tac;
}

void renumber_label(Tac *ir, int l1, int l2) {
    Tac *t;

    t = ir;
    while (t) {
        if (t->src1 && t->src1->label == l1) t->src1->label = l2;
        if (t->src2 && t->src2->label == l1) t->src2->label = l2;
        if (t->label == l1) t->label = l2;
        t = t->next;
    }
}

void merge_labels(Tac *ir, Tac *tac, int ir_index) {
    Tac *deleted_tac, *t;
    int l;

    while(1) {
        if (!tac->label || !tac->next || !tac->next->label) return;

        deleted_tac = tac->next;
        merge_instructions(tac, ir_index, 1);
        renumber_label(ir, deleted_tac->label, tac->label);
    }
}

// Renumber all labels so they are consecutive. Uses label_count global.
void renumber_labels(Tac *ir) {
    Tac *t;
    int temp_labels;

    temp_labels = -2;

    t = ir;
    while (t) {
        if (t->label) {
            // Rename target label out of the way
            renumber_label(ir, label_count + 1, temp_labels);
            temp_labels--;

            // Replace t->label with label_count + 1
            renumber_label(ir, t->label, -1);
            t->label = ++label_count;
            renumber_label(ir, -1, t->label);
        }
        t = t->next;
    }
}

void rearrange_reverse_sub_operation(Tac *ir, Tac *tac) {
    Value *src1, *src2;
    int vreg1, vreg2;

    if (tac->operation == IR_SUB) {
        tac->operation = IR_RSUB;

        if (tac->src2->is_constant) {
            // Flip the operands
            src1 = dup_value(tac->src1);
            src2 = dup_value(tac->src2);
            tac->src1 = src2;
            tac->src2 = src1;
        }
        else {
            // Convert sub to reverse sub. Swap registers everywhere except
            // this tac and convert the operation to RSUB.
            vreg1 = tac->src1->vreg;
            vreg2 = tac->src2->vreg;
            swap_ir_registers(ir, tac->src1->vreg, tac->src2->vreg);
            tac->src1 = dup_value(tac->src1);
            tac->src2 = dup_value(tac->src2);
            tac->src1->vreg = vreg1;
            tac->src2->vreg = vreg2;
        }
    }
}

void preload_src1_constant_into_register(Tac *tac, int *i) {
    Value *dst, *src1;
    Tac *load_tac;

    load_tac = new_instruction(IR_LOAD_CONSTANT);

    load_tac->src1 = tac->src1;

    dst = new_value();
    dst->vreg = new_vreg();
    dst->type = TYPE_LONG;

    load_tac->dst = dst;
    insert_instruction(tac, *i, load_tac);
    tac->src1 = dst;

    (*i)++;
}

void allocate_registers_for_constants(Tac *tac, int *i) {
    // Some instructions can't handle one of the operands being a constant. Allocate a vreg for it
    // and load the constant into it.

    // 1 - i case can't be done in x86_64 and needs to be done with registers
    if (tac->operation == IR_SUB && tac->src1->is_constant)
        preload_src1_constant_into_register(tac, i);
}

void optimize_ir(Symbol *function) {
    Tac *ir, *tac;
    int i;

    ir = function->function->ir;

    reverse_function_argument_order(function);
    assign_locals_to_registers(function);

    tac = ir;
    i = 0;
    while (tac) {
        merge_labels(ir, tac, i);
        allocate_registers_for_constants(tac, &i);
        if (!instruction_selection_wip) rearrange_reverse_sub_operation(ir, tac);
        tac = tac->next;
        i++;
    }

    renumber_labels(ir);
}
