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
    struct value **lvs, *v;

    printf("%-4s %-4s %-4s %-11s %-11s %-5s\n", "type", "vreg", "preg", "global_sym", "stack_index", "is_lv");
    lvs = vs;
    while (lvs != vs_start) {
        v = *lvs;
        printf("%-4d %-4d %-4d %-11s %-11d %-5d\n",
            v->type, v->vreg, v->preg, v->global_symbol ? v->global_symbol->identifier : 0, v->stack_index, v->is_lvalue);
        lvs++;
    }
}

struct value *new_value() {
    struct value *v;

    v = malloc(sizeof(struct value));
    memset(v, 0, sizeof(struct value));
    v->preg = -1;
    v->spilled_stack_index = -1;
    v->ssa_subscript = -1;
    v->live_range = -1;

    return v;
}

// Create a new typed constant value
struct value *new_constant(int type, long value) {
    struct value *cv;

    cv = new_value();
    cv->value = value;
    cv->type = type;
    cv->is_constant = 1;
    return cv;
}

// Duplicate a value
struct value *dup_value(struct value *src) {
    struct value *dst;

    dst = new_value();
    dst->type                      = src->type;
    dst->vreg                      = src->vreg;
    dst->preg                      = src->preg;
    dst->is_lvalue                 = src->is_lvalue;
    dst->spilled_stack_index       = src->spilled_stack_index;
    dst->stack_index               = src->stack_index;
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

    return dst;
}

struct three_address_code *new_instruction(int operation) {
    struct three_address_code *tac;

    tac = malloc(sizeof(struct three_address_code));
    memset(tac, 0, sizeof(struct three_address_code));
    tac->operation = operation;

    return tac;
}

// Add instruction to the global intermediate representation ir
struct three_address_code *add_instruction(int operation, struct value *dst, struct value *src1, struct value *src2) {
    struct three_address_code *tac;

    tac = new_instruction(operation);
    tac->label = 0;
    tac->dst = dst;
    tac->src1 = src1;
    tac->src2 = src2;
    tac->next = 0;
    tac->prev = 0;
    tac->in_conditional = in_conditional;

    if (!ir_start) {
        ir_start = tac;
        ir = tac;
    }
    else {
        tac->prev = ir;
        ir->next = tac;
        ir = tac;
    }

    return tac;
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

void print_value(void *f, struct value *v, int is_assignment_rhs) {
    int type;

    if (is_assignment_rhs && !v->is_lvalue && (v->global_symbol || v->stack_index)) fprintf(f, "&");
    if (!is_assignment_rhs && v->is_lvalue && !(v->global_symbol || v->stack_index)) fprintf(f, "L");

    if (v->is_constant)
        fprintf(f, "%ld", v->value);
    else if (v->is_in_cpu_flags)
        fprintf(f, "cpu");
    else if (v->preg != -1)
        fprintf(f, "p%d", v->preg);
    else if (v->spilled_stack_index != -1)
        fprintf(f, "S[%d]", v->spilled_stack_index);
    else if (v->live_range != -1)
        fprintf(f, "LR%d", v->live_range);
    else if (v->vreg) {
        fprintf(f, "r%d", v->vreg);
        if (v->ssa_subscript != -1) fprintf(f, "_%d", v->ssa_subscript);
    }
    else if (v->stack_index)
        fprintf(f, "s[%d]", v->stack_index);
    else if (v->global_symbol)
        fprintf(f, "%s", v->global_symbol->identifier);
    else if (v->is_string_literal) {
        fprintf_escaped_string_literal(f, string_literals[v->string_literal_index]);
    }
    else
        fprintf(f, "%ld", v->value);

    fprintf(f, ":");
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

void print_instruction(void *f, struct three_address_code *tac) {
    if (tac->label)
        fprintf(f, "l%-5d", tac->label);
    else
        fprintf(f, "      ");

    if (tac->dst) {
        print_value(f, tac->dst, tac->operation != IR_ASSIGN);
        fprintf(f, " = ");
    }

    if (tac->operation == IR_LOAD_CONSTANT || tac->operation == IR_LOAD_VARIABLE || tac->operation == IR_LOAD_STRING_LITERAL) {
        print_value(f, tac->src1, 1);
    }

    else if (tac->operation == IR_START_CALL) fprintf(f, "start call %ld", tac->src1->value);
    else if (tac->operation == IR_END_CALL) fprintf(f, "end call %ld", tac->src1->value);

    else if (tac->operation == IR_ARG) {
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

    else if (tac->operation == IR_NOP)
        fprintf(f, "nop");

    else if (tac->operation == IR_JZ || tac->operation == IR_JNZ) {
             if (tac->operation == IR_JZ)  fprintf(f, "jz ");
        else if (tac->operation == IR_JNZ) fprintf(f, "jnz ");
        print_value(f, tac->src1, 1);
        fprintf(f, " l%d", tac->src2->label);
    }

    else if (tac->operation == IR_JMP)
        fprintf(f, "jmp l%d", tac->src1->label);

    else if (tac->operation == IR_PHI_FUNCTION) {
        fprintf(f, "Φ(");
        print_value(f, tac->src1, 1);
        fprintf(f, ", ");
        print_value(f, tac->src2, 2);
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

    else
        panic1d("print_instruction(): Unknown operation: %d", tac->operation);

    fprintf(f, "\n");
}

void print_intermediate_representation(struct function *function, char *name) {
    struct three_address_code *tac;
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

// We're not quite entirely SSA but a step in the right direction.
void ensure_must_be_ssa_ish(struct three_address_code *ir) {
    struct three_address_code *tac1, *tac2;

    int i, j, dst, src1, src2;

    i = 0;
    tac1 = ir;
    while (tac1) {
        dst = src1 = src2 = 0;

        // Ensure dst isn't the same as src1 nor src2
        if (tac1-> dst && tac1-> dst->vreg != -1) dst  = tac1-> dst->vreg;
        if (tac1->src1 && tac1->src1->vreg != -1) src1 = tac1->src1->vreg;
        if (tac1->src2 && tac1->src2->vreg != -1) src2 = tac1->src2->vreg;

        if (dst && ((dst == src1 || dst == src2))) {
            print_instruction(stdout, tac1);
            printf("Not SSA in IR instruction %d\n", i);
            exit(1);
        }

        // FIXME the ternary generates code with 2 register assigns, which violates SSA
        // if (ir->operation != IR_TERNARY && dst) {
        //     tac2 = tac1 + 1;
        //     j = 0;
        //     while (tac2->operation) {
        //         if (tac2->dst && tac2->dst->vreg == dst) {
        //             printf("IR instructions not SSA\n");
        //             fprintf(stdout, "%-4d > ", i); print_instruction(stdout, tac1);
        //             fprintf(stdout, "%-4d > ", j); print_instruction(stdout, tac2);
        //             exit(1);
        //         }
        //         tac2++;
        //         j++;
        //     }
        // }

        i++;
        tac1 = tac1->next;
    }
}

void update_register_liveness(int vreg, int instruction_position) {
    if (liveness[vreg].start == -1 || instruction_position < liveness[vreg].start) liveness[vreg].start = instruction_position;
    if (liveness[vreg].end == -1 || instruction_position > liveness[vreg].end) liveness[vreg].end = instruction_position;
}

int get_instruction_for_label(struct symbol *function, int label) {
    struct three_address_code *tac;
    int i;

    i = 0;
    tac = function->function->ir;
    while (tac) {
        if (tac->label == label) return i;
        tac = tac->next;
        i++;
    }

    panic1d("Unknown label %d", label);
}

// Debug function
void print_liveness(struct symbol *function) {
    int i;

    for (i = 1; i <= function->function->vreg_count; i++)
        printf("r%-4d %4d - %4d\n", i, liveness[i].start, liveness[i].end);
}

// Make a liveness_interval for each three address code in the IR. The liveness
// interval, if set, corresponds with the most outer for-loop.
struct liveness_interval **make_outer_loops(struct symbol *function) {
    struct three_address_code *tac, *end;
    struct liveness_interval **result, *l;
    int i, ir_len, loop;

    // Allocate result, one liveness_interval per three address code in the IR
    ir_len = 1;
    tac = function->function->ir;
    while ((tac = tac->next)) ir_len++;
    result = malloc(sizeof(struct liveness_interval *) * ir_len);

    i = 0;
    l = 0;
    tac = function->function->ir;
    while (tac) {
        if (!l && tac->operation == IR_START_LOOP) {
            l = malloc(sizeof(struct liveness_interval));
            l->start = i;
            loop = tac->src2->value;
        }
        else if (tac->operation == IR_END_LOOP && tac->src2->value == loop) {
            l->end = i;
            l = 0;
        }

        result[i] = l;
        tac = tac->next;
        i++;
    }

    return result;
}

void analyze_liveness(struct symbol *function) {
    int i, j, k, l;
    struct three_address_code *tac;
    struct liveness_interval *liveness_interval, **outer_loops;

    for (i = 1; i <= function->function->vreg_count; i++) {
        liveness[i].start = -1;
        liveness[i].end = -1;
    }

    // Update liveness based on usage in IR
    i = 0;
    tac = function->function->ir;
    while (tac) {
        if (tac->dst  && tac->dst->vreg)  update_register_liveness(tac->dst->vreg,  i);
        if (tac->src1 && tac->src1->vreg) update_register_liveness(tac->src1->vreg, i);
        if (tac->src2 && tac->src2->vreg) update_register_liveness(tac->src2->vreg, i);
        tac = tac->next;
        i++;
    }

    if (opt_use_registers_for_locals) {
        // Deal with backwards jumps. Any backwards jump targets that are in a live
        // range need the live range extending to include the jump instruction
        // since the value might have changed just before the jump.
        // Update liveness based on usage in IR.
        i = 0;
        tac = function->function->ir;
        while (tac) {
            if (tac->operation == IR_JMP || tac->operation == IR_JZ || tac->operation == IR_JNZ) {
                l = get_instruction_for_label(function, tac->src1->label);
                if (i > l)
                    for (j = 1; j <= function->function->vreg_count; j++)
                        if (liveness[j].start <= l && liveness[j].end >= l && i > liveness[j].end) liveness[j].end = i;
            }
            tac = tac->next;
            i++;
        }

        // Look for assignments in a conditional in a loop. The liveness needs extending.
        // to include the outmost loop.
        outer_loops = make_outer_loops(function);

        i = 0;
        tac = function->function->ir;
        while (tac) {
            if (tac->dst && tac->dst->vreg && tac->in_conditional) {
                // Only extend liveness for variables, not temporaries.
                if (tac->dst->stack_index || tac->dst->original_stack_index) {
                    if (debug_register_allocations) printf("extending liveness due to if/for for vreg=%d\n", tac->dst->vreg);
                    liveness_interval = outer_loops[i];
                    if (liveness_interval) {
                        if (liveness_interval->start < liveness[tac->dst->vreg].start) liveness[tac->dst->vreg].start = liveness_interval->start;
                        if (liveness_interval->end > liveness[tac->dst->vreg].end) liveness[tac->dst->vreg].end = liveness_interval->end;
                    }
                }
            }

            tac = tac->next;
            i++;
        }
    }
}

// Merge tac with the instruction after it. The next instruction is removed from the chain.
void merge_instructions(struct three_address_code *tac, int ir_index, int allow_labelled_next) {
    int i, label;
    struct three_address_code *next;

    if (!tac->next) panic("merge_instructions called on a tac without next\n");
    if (tac->next->label && !allow_labelled_next) panic("merge_instructions called on a tac with a label");

    next = tac->next;
    tac->next = next->next;
    if (tac->next) tac->next->prev = tac;

    // Nuke the references to the removed node to prevent confusion
    next->prev = 0;
    next->next = 0;

    for (i = 0; i < vreg_count; i++) {
        if (liveness[i].start > ir_index) liveness[i].start--;
        if (liveness[i].end > ir_index) liveness[i].end--;
    }
}

// Replace
// 1    >       r1:int = 1
// 2    >       r7:int = r1:int
// with
// 1    >       r7:int = 1
// 2    >       nop
//
// And also combinations such as
// 1    >       r1:int = 1
// 2    >       r7:int = r1:int
// 3    >       r8:int = r7:int
// with
// 1    >       r8:int = 1
// 2    >       nop
//
// Also
// 4    >       r1:int = r4:int
// 5    >       arg for call 0 r1:int
// with
// 4    >       arg for call 0 r4:int
void merge_redundant_moves(struct symbol *function) {
    struct three_address_code *tac;
    int i, vreg, changed;

    changed = 1;
    while (changed) {
        changed = 0;
        tac = function->function->ir;
        i = 0;
        while (tac) {
            if (
                    ((tac->operation == IR_LOAD_CONSTANT || tac->operation == IR_LOAD_VARIABLE || tac->operation == IR_ASSIGN)) &&
                    tac->dst->vreg &&
                    tac->next &&
                    tac->next->operation == IR_ASSIGN && tac->next->dst->vreg &&
                    ((tac->next->src1 && tac->next->src1->vreg == tac->dst->vreg) || (tac->next->src2 && tac->next->src2->vreg == tac->dst->vreg))) {

                vreg = tac->dst->vreg;
                if (liveness[vreg].start == i && liveness[vreg].end == i + 1 && !tac->next->dst->is_lvalue) {
                    liveness[tac->dst->vreg].start = -1;
                    liveness[tac->dst->vreg].end = -1;
                    tac->dst = tac->next->dst;
                    merge_instructions(tac, i, 0);
                    changed = 1;
                }
            }

            if (
                    tac->operation == IR_ASSIGN &&
                    tac->dst->vreg && !tac->src1->is_in_cpu_flags &&
                    tac->next && tac->next->operation == IR_ARG &&
                    tac->dst->vreg == tac->next->src2->vreg) {

                vreg = tac->dst->vreg;
                if (liveness[vreg].start == i && liveness[vreg].end == i + 1) {
                    liveness[tac->dst->vreg].start = -1;
                    liveness[tac->dst->vreg].end = -1;
                    tac->operation = IR_ARG;
                    tac->src2 = tac->src1;
                    tac->src1 = tac->next->src1;
                    tac->dst = 0;
                    merge_instructions(tac, i, 0);
                    changed = 1;
                }
            }

            tac = tac->next;
            i++;
        }
    }
}

// The arguments are pushed onto the stack right to left, but the ABI requries
// the seventh arg and later to be pushed in reverse order. Easiest is to flip
// all args backwards, so they are pushed left to right. This nukes the
// liveness which will need regenerating.
void reverse_function_argument_order(struct symbol *function) {
    struct three_address_code *tac, *call_start, *call;
    int i, j, arg_count, function_call_count;

    struct tac_interval *args;

    ir = function->function->ir;
    args = malloc(sizeof(struct tac_interval *) * 256);

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

    analyze_liveness(function);
    free(args);
}

void assign_local_to_register(struct value *v, int vreg) {
    v->original_stack_index = v->stack_index;
    v->original_is_lvalue = v->is_lvalue;
    v->original_vreg = v->vreg;
    v->stack_index = 0;
    v->is_lvalue = 0;
    v->vreg = vreg;
}

void spill_local_value_back_to_stack(struct value *v) {
    if (v->stack_index < 0) return; // already done
    if (debug_register_allocations) printf("spill_local_value_back_to_stack preg=%d, vreg=%d, ovreg=%d, s=%d\n", v->preg, v->vreg, v->original_vreg, v->original_stack_index);
    v->vreg = v->original_vreg;
    v->stack_index = v->original_stack_index;
    v->is_lvalue = v->original_is_lvalue;
    v->preg = -1;
}

void assign_locals_to_registers(struct symbol *function) {
    struct three_address_code *tac;
    int i, vreg;

    int *has_address_of;

    has_address_of = malloc(sizeof(int) * (function->function->local_symbol_count + 1));
    memset(has_address_of, 0, sizeof(int) * (function->function->local_symbol_count + 1));

    tac = function->function->ir;
    while (tac) {
        if (tac->dst  && !tac->dst ->is_lvalue && tac->dst ->stack_index < 0) has_address_of[-tac->dst ->stack_index] = 1;
        if (tac->src1 && !tac->src1->is_lvalue && tac->src1->stack_index < 0) has_address_of[-tac->src1->stack_index] = 1;
        if (tac->src2 && !tac->src2->is_lvalue && tac->src2->stack_index < 0) has_address_of[-tac->src2->stack_index] = 1;
        tac = tac ->next;
    }

    for (i = 1; i <= function->function->local_symbol_count; i++) {
        if (has_address_of[i]) continue;
        vreg = ++vreg_count;

        tac = function->function->ir;
        while (tac) {
            if (tac->dst  && tac->dst ->stack_index == -i) assign_local_to_register(tac->dst , vreg);
            if (tac->src1 && tac->src1->stack_index == -i) assign_local_to_register(tac->src1, vreg);
            if (tac->src2 && tac->src2->stack_index == -i) assign_local_to_register(tac->src2, vreg);
            if (tac->operation == IR_LOAD_VARIABLE && tac->src1->vreg == vreg)
                tac->operation = IR_ASSIGN;

            tac = tac ->next;
        }
    }

    function->function->vreg_count = vreg_count;
    analyze_liveness(function);
}

void renumber_ir_vreg(struct three_address_code *ir, int src, int dst) {
    struct three_address_code *tac;

    if (src == 0 || dst == 0) panic("Unexpected zero reg renumber");

    tac = ir;
    while (tac) {
        if (tac->dst  && tac->dst ->vreg == src) tac->dst ->vreg = dst;
        if (tac->src1 && tac->src1->vreg == src) tac->src1->vreg = dst;
        if (tac->src2 && tac->src2->vreg == src) tac->src2->vreg = dst;
        tac = tac->next;
    }

    liveness[dst] = liveness[src];
}

void swap_ir_registers(struct three_address_code *ir, int vreg1, int vreg2) {
    renumber_ir_vreg(ir, vreg1, -2);
    renumber_ir_vreg(ir, vreg2, vreg1);
    renumber_ir_vreg(ir, -2, vreg2);
}

struct three_address_code *insert_instruction(struct three_address_code *ir, int ir_index, struct three_address_code *tac) {
    int i;
    struct three_address_code *prev;

    prev = ir->prev;
    tac->prev = prev;
    tac->next = ir;
    prev->next = tac;

    for (i = 0; i < vreg_count; i++) {
        if (liveness[i].start >= ir_index) liveness[i].start++;
        if (liveness[i].end >= ir_index) liveness[i].end++;
    }
}

void renumber_label(struct three_address_code *ir, int l1, int l2) {
    struct three_address_code *t;

    t = ir;
    while (t) {
        if (t->src1 && t->src1->label == l1) t->src1->label = l2;
        if (t->src2 && t->src2->label == l1) t->src2->label = l2;
        if (t->label == l1) t->label = l2;
        t = t->next;
    }
}

void merge_labels(struct three_address_code *ir, struct three_address_code *tac, int ir_index) {
    struct three_address_code *deleted_tac, *t;
    int l;

    while(1) {
        if (!tac->label || !tac->next || !tac->next->label) return;

        deleted_tac = tac->next;
        merge_instructions(tac, ir_index, 1);
        renumber_label(ir, deleted_tac->label, tac->label);
    }
}

// Renumber all labels so they are consecutive. Uses label_count global.
void renumber_labels(struct three_address_code *ir) {
    struct three_address_code *t;
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

void rearrange_reverse_sub_operation(struct three_address_code *ir, struct three_address_code *tac) {
    struct value *src1, *src2;
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

void coalesce_operation_registers(struct three_address_code *ir, struct three_address_code *tac, int i) {
    struct value *src1, *src2;

    // If src2 isn't live after this instruction, then the operation
    // is allowed to change it. dst->vreg can be replaced with src2->vreg
    // which saves a vreg.
    if ((   tac->operation == IR_ADD || tac->operation == IR_RSUB ||
            tac->operation == IR_MUL || tac->operation == IR_DIV || tac->operation == IR_MOD ||
            tac->operation == IR_BOR || tac->operation == IR_BAND || tac->operation == IR_XOR) && i == liveness[tac->src2->vreg].end)
        renumber_ir_vreg(ir, tac->dst->vreg, tac->src2->vreg);

    // Free either src1 or src2's vreg if possible
    if (tac->operation == IR_LT || tac->operation == IR_GT || tac->operation == IR_LE || tac->operation == IR_GE || tac->operation == IR_EQ || tac->operation == IR_NE) {
        if (tac->dst->vreg && tac->src1->vreg && i == liveness[tac->src1->vreg].end)
            renumber_ir_vreg(ir, tac->dst->vreg, tac->src1->vreg);
        else if (tac->dst->vreg && tac->src2->vreg && i == liveness[tac->src2->vreg].end)
            renumber_ir_vreg(ir, tac->dst->vreg, tac->src2->vreg);
    }

    // The same applies to the one-operand opcodes
    if ((tac->operation == IR_INDIRECT || tac->operation == IR_BNOT || tac->operation == IR_BSHL || tac->operation == IR_BSHR) && i == liveness[tac->src1->vreg].end)
        renumber_ir_vreg(ir, tac->dst->vreg, tac->src1->vreg);
}

void preload_src1_constant_into_register(struct three_address_code *tac, int *i) {
    struct value *dst, *src1;
    struct three_address_code *load_tac;

    load_tac = new_instruction(IR_LOAD_CONSTANT);

    load_tac->src1 = tac->src1;

    dst = new_value();
    dst->vreg = new_vreg();
    dst->type = TYPE_LONG;

    load_tac->dst = dst;
    insert_instruction(tac, *i, load_tac);
    tac->src1 = dst;

    liveness[dst->vreg].start = *i;
    liveness[dst->vreg].end = *i + 1;
    (*i)++;
}

void allocate_registers_for_constants(struct three_address_code *tac, int *i) {
    // Some instructions can't handle one of the operands being a constant. Allocate a vreg for it
    // and load the constant into it.

    // 1 - i case can't be done in x86_64 and needs to be done with registers
    if (tac->operation == IR_SUB && tac->src1->is_constant)
        preload_src1_constant_into_register(tac, i);
}

void optimize_ir(struct symbol *function) {
    struct three_address_code *ir, *tac;
    int i;

    ir = function->function->ir;

    reverse_function_argument_order(function);

    if (opt_use_registers_for_locals) assign_locals_to_registers(function);
    if (opt_merge_redundant_moves) merge_redundant_moves(function);

    tac = ir;
    i = 0;
    while (tac) {
        merge_labels(ir, tac, i);
        allocate_registers_for_constants(tac, &i);
        rearrange_reverse_sub_operation(ir, tac);
        if (opt_enable_register_coalescing) coalesce_operation_registers(ir, tac, i);
        tac = tac->next;
        i++;
    }

    renumber_labels(ir);
}

void spill_local_in_register_back_to_stack(struct three_address_code *ir, int original_stack_index) {
    struct three_address_code *tac;

    tac = ir;
    while (tac) {
        if (tac->dst  && tac->dst ->original_stack_index == original_stack_index) spill_local_value_back_to_stack(tac->dst);
        if (tac->src1 && tac->src1->original_stack_index == original_stack_index) spill_local_value_back_to_stack(tac->src1);
        if (tac->src2 && tac->src2->original_stack_index == original_stack_index) spill_local_value_back_to_stack(tac->src2);
        if (tac->operation == IR_ASSIGN && tac->src1->stack_index == original_stack_index)
            tac->operation = IR_LOAD_VARIABLE;
        tac = tac->next;
    }
}

int get_spilled_register_stack_index() {
    int i, result;

    // First try and reuse a previously used spilled stack register
    for (i = 0; i < spilled_register_count; i++)
        if (!spilled_registers[i]) return i;

    // All spilled register stack locations are in use. Allocate a new one
    result = spilled_register_count++;

    if (spilled_register_count >= MAX_SPILLED_REGISTER_COUNT)
        panic1d("Exceeded max spilled register count %d", MAX_SPILLED_REGISTER_COUNT);
    return result;
}

void spill_vreg_for_value(struct three_address_code *ir, struct value *v) {
    struct three_address_code *tac;
    int i, spilled_register_stack_index, original_stack_index, original_vreg;

    original_stack_index = v->original_stack_index;
    original_vreg = v->vreg;

    if (v->original_stack_index < 0) {
        if (debug_register_allocations) printf("spill_vreg_for_value spill_local_in_register_back_to_stack to s=%d for vreg=%d\n", original_stack_index, v->vreg);
        // We have a stack index already, use that
        spill_local_in_register_back_to_stack(ir, original_stack_index);
        return;
    }

    spilled_register_stack_index = get_spilled_register_stack_index();
    v->spilled_stack_index = spilled_register_stack_index;
    spilled_registers[spilled_register_stack_index] = v->vreg;
    if (debug_register_allocations) printf("spilling to S=%d for vreg=%d\n", spilled_register_stack_index, v->vreg);
}

// Spill a previously allocated physical register to the stack
void spill_preg(struct three_address_code *ir, struct value *v, int preg) {
    struct three_address_code *tac;
    int vreg, spilled_register_stack_index, original_stack_index;

    // Check if preg belongs to a local variable. Keep looping until the latest TAC,
    // since the preg could have been resued may times.
    original_stack_index = 0;
    tac = ir;
    while (tac) {
        if (tac->dst  && tac->dst ->preg == preg) { original_stack_index = tac->dst ->original_stack_index; }
        if (tac->src1 && tac->src1->preg == preg) { original_stack_index = tac->src1->original_stack_index; }
        if (tac->src2 && tac->src2->preg == preg) { original_stack_index = tac->src2->original_stack_index; }
        tac = tac->next;
    }

    // It's already allocated a stack index, use that
    if (original_stack_index < 0) {
        if (debug_register_allocations) printf("spill_preg preg=%d to s=%d\n", preg, original_stack_index);
        spill_local_in_register_back_to_stack(ir, original_stack_index);
    }

    else {
        // Always allocate a new stack index. It must be new since this stack index
        // is applied to the entire IR, so it must not overlap with a previously
        // used stack index.
        spilled_register_stack_index = get_spilled_register_stack_index();
        vreg = physical_registers[preg];
        spilled_registers[spilled_register_stack_index] = vreg;
        if (debug_register_allocations) printf("spill_preg preg=%d for vreg=%d to S=%d\n", preg, vreg, original_stack_index);

        tac = ir;
        while (tac) {
            if (tac->dst  && tac->dst ->vreg == vreg) { tac->dst ->preg = -1; tac->dst ->spilled_stack_index = spilled_register_stack_index; }
            if (tac->src1 && tac->src1->vreg == vreg) { tac->src1->preg = -1; tac->src1->spilled_stack_index = spilled_register_stack_index; }
            if (tac->src2 && tac->src2->vreg == vreg) { tac->src2->preg = -1; tac->src2->spilled_stack_index = spilled_register_stack_index; }
            tac = tac->next;
        }
    }

    if (debug_register_allocations) {
        printf("spill_preg repurposed preg=%d for vreg=%d\n", preg, v->vreg);
        tac = ir;
        while (tac) {
            print_instruction(stdout, tac);
            tac = tac->next;
        }
    }

    physical_registers[preg] = v->vreg;
    v->preg = preg;
}

void allocate_register(struct three_address_code *ir, struct value *v) {
    int i;
    int existing_preg, existing_preg_liveness_end;
    struct three_address_code *tac;

    // Return if already allocated. This can happen if values share memory
    // between different instructions
    if (v->preg != -1) return;
    if (v->spilled_stack_index != -1) return;

    // Check for already spilled registers
    for (i = 0; i < spilled_register_count; i++) {
        if (spilled_registers[i] == v->vreg) {
            v->spilled_stack_index = i;
            return;
        }
    }

    // Check for already allocated registers
    for (i = 0; i < PHYSICAL_REGISTER_COUNT; i++) {
        if (physical_registers[i] == v->vreg) {
            v->preg = i;
            return;
        }
    }

    // Find a free register
    for (i = 0; i < PHYSICAL_REGISTER_COUNT; i++) {
        if (!physical_registers[i]) {
            if (debug_register_allocations) printf("allocated preg=%d for vreg=%d\n", i, v->vreg);
            physical_registers[i] = v->vreg;
            v->preg = i;

            return;
        }
    }

    // Failed to allocate a register, something needs to be spilled.
    existing_preg = -1;
    existing_preg_liveness_end = -1;
    for (i = 0; i < PHYSICAL_REGISTER_COUNT; i++) {
        if (physical_registers[i] > 0) {
            if (liveness[physical_registers[i]].end > existing_preg_liveness_end) {
                existing_preg = i;
                existing_preg_liveness_end = liveness[physical_registers[i]].end;
            }
        }
    }

    if (opt_spill_furthest_liveness_end && existing_preg_liveness_end > liveness[v->vreg].end) {
        spill_preg(ir, v, existing_preg);
        return;
    }

    spill_vreg_for_value(ir, v);
}

struct function_usages *get_function_usages(struct three_address_code *ir) {
    struct function_usages *result;
    struct three_address_code *tac;

    result = malloc(sizeof(struct function_usages));
    memset(result, 0, sizeof(struct function_usages));

    tac = ir;
    while (tac) {
        if (tac->operation == IR_DIV || tac->operation == IR_MOD) result->div_or_mod = 1;
        if (tac->operation == IR_CALL) result->function_call = 1;
        if (tac->operation == IR_BSHR || tac->operation == IR_BSHL) result->binary_shift = 1;
        tac = tac->next;
    }

    return result;
}

void make_available_phyical_register_list(struct three_address_code *ir) {
    struct function_usages *function_usages;

    physical_registers = malloc(sizeof(int) * PHYSICAL_REGISTER_COUNT);
    memset(physical_registers, 0, sizeof(int) * PHYSICAL_REGISTER_COUNT);

    // Blacklist registers if certain operations are happening in this function.
    function_usages = get_function_usages(ir);

    physical_registers[REG_RAX] = function_usages->function_call || function_usages->div_or_mod   ? -1 : 0;
    physical_registers[REG_RDX] = function_usages->function_call || function_usages->div_or_mod   ? -1 : 0;
    physical_registers[REG_RCX] = function_usages->function_call || function_usages->binary_shift ? -1 : 0;
    physical_registers[REG_RSI] = function_usages->function_call                                  ? -1 : 0;
    physical_registers[REG_RDI] = function_usages->function_call                                  ? -1 : 0;
    physical_registers[REG_R8]  = function_usages->function_call                                  ? -1 : 0;
    physical_registers[REG_R9]  = function_usages->function_call                                  ? -1 : 0;
    physical_registers[REG_RSP] = -1; // Stack pointer
    physical_registers[REG_RBP] = -1; // Base pointer
    physical_registers[REG_R10] = -1; // Not preserved in function calls & used as temporary
    physical_registers[REG_R11] = -1; // Not preserved in function calls & used as temporary

    if (fake_register_pressure) {
        // Allocate all registers, forcing all temporaries into the stack
        physical_registers[REG_RBX] = -1;
        physical_registers[REG_R12] = -1;
        physical_registers[REG_R13] = -1;
        physical_registers[REG_R14] = -1;
        physical_registers[REG_R15] = -1;
    }
}

void allocate_registers(struct three_address_code *ir) {
    int line, i, j, vreg;
    struct three_address_code *tac, *tac2;

    make_available_phyical_register_list(ir);

    spilled_registers = malloc(sizeof(int) * MAX_SPILLED_REGISTER_COUNT);
    memset(spilled_registers, 0, sizeof(int) * MAX_SPILLED_REGISTER_COUNT);
    spilled_register_count = 0;

    line = 0;
    tac = ir;
    while (tac) {
        if (debug_register_allocations) {
            printf("%d ", line);
            print_instruction(stdout, tac);
        }

        // Allocate registers
        if (tac->dst  && tac->dst->vreg)  allocate_register(ir, tac->dst);
        if (tac->src1 && tac->src1->vreg) allocate_register(ir, tac->src1);
        if (tac->src2 && tac->src2->vreg) allocate_register(ir, tac->src2);

        // Free registers
        for (i = 0; i < PHYSICAL_REGISTER_COUNT; i++) {
            if (physical_registers[i] > 0 && liveness[physical_registers[i]].end == line) {
                if (debug_register_allocations) printf("freeing preg=%d for vreg=%d\n", i, physical_registers[i]);
                physical_registers[i] = 0;
            }
        }

        // Free spilled stack locations
        for (i = 0; i < spilled_register_count; i++) {
            if (spilled_registers[i] > 0 && liveness[spilled_registers[i]].end == line)
                spilled_registers[i] = 0;
        }

        tac = tac->next;
        line++;
    }

    free(physical_registers);
    free(spilled_registers);

    total_spilled_register_count += spilled_register_count;
}
