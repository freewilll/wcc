#include <string.h>

#include "wcc.h"
#include "aarch64.h"

static int cur_stack_push_count; // Used in codegen to keep track of stack position

// Codegen
char *register_name(int preg) {
    char *buffer = wcalloc(1, 16);

    if (preg >= REG_R00 && preg <= REG_R30)
        sprintf(buffer, "x%d", preg);
    else if (preg >= REG_V00 && preg <= REG_V30)
        sprintf(buffer, "v%d", preg);
    else if (preg == REG_SP)
        sprintf(buffer, "sp");
    else if (preg == REG_FSPR)
        sprintf(buffer, "fspr");

    return buffer;
}

char *render_target_operation(Tac *tac, int function_pc, int expect_preg) {
    char *t = tac->target_template;

    if (!t) return 0;

    char *buffer = wcalloc(1, 128);
    char *result = buffer;

    while (*t && *t != ' ') *buffer++ = *t++;
    while (*t && *t == ' ') t++;

    if (*t) {
        int mnemonic_length = buffer - result;
        for (int i = 0; i < 12 - mnemonic_length; i++) *buffer++ = ' ';
    }

    while (*t) {
        if (*t == '%') {
            Value *v;

            t++;

            if (t[0] != 'v') panic("Unknown placeholder in %s", tac->target_template);

            t++;

                 if (t[0] == '1') v = tac->src1;
            else if (t[0] == '2') v = tac->src2;
            else if (t[0] == 'd') v = tac->dst;
            else panic("Indecipherable placeholder \"%s\"", tac->target_template);

            int is_32bit = 0;

                 if (t[1] == 'w') { t++; is_32bit = 1; }
            else if (t[1] == 'x') { t++; is_32bit = 0; }

            if (!v) panic("Unexpectedly got a null value while the template %s is expecting it", tac->target_template);

            // TODO aarch64 offset
            if (!expect_preg && v->vreg) {
                if (v->global_symbol) panic("Got global symbol in vreg");

                *buffer++ = 'r';
                sprintf(buffer, "%d", v->vreg);
                while (*buffer) buffer++;
                *buffer++ = is_32bit_to_aarch64_size(is_32bit);
            }
            else if (expect_preg && v->preg != -1) {
                *buffer++ = is_32bit_to_aarch64_size(is_32bit);
                sprintf(buffer, "%d", v->preg);
            }
            else if (v->is_constant) {
                sprintf(buffer, "%ld", v->int_value);
            }
            else if (v->is_string_literal)
                sprintf(buffer, ".LS%d", v->string_literal_index);
            else if (v->global_symbol) {
                // TODO aarch64 long double
                if (v->offset)
                    panic("TODO aarch64 global symbol with offset");
                else {
                    if (v->load_from_got)
                        panic("TODO aarch64 load from GIT");
                    else
                        sprintf(buffer, "%s", v->global_symbol->global_identifier);
                }
            }
            else {
                print_value(stdout, v, 0);
                printf("\n");
                panic("Don't know how to render template value");
            }
        }
        else {
            *buffer++ = *t;
        }

        while (*buffer) buffer++;
        t++;
    }

    return result;
} // TODO aarch64

static void output_aarch64_operation(Tac *tac, int function_pc) {
    char *buffer = render_target_operation(tac, function_pc, 1);
    if (buffer) {
        fprintf(output_file, "    %s\n", buffer);
        wfree(buffer);
    }
}

static Tac *process_integer_constant_move_to_register(Tac *tac) {
    static const int MOVZ = 0; // Set zeroes
    static const int MOVN = 1; // Set ones and negate
    static const int MOVK = 2; // Keep other half words

    // Define all possible templates at compile time to avoid memory allocations
    static char *templates[3][4][2] = {
        "movz %vdw, %v1w" ,        "movz %vdx, %v1x" ,
        "movz %vdw, %v1w, lsl 16", "movz %vdx, %v1x, lsl 16",
        "movz %vdw, %v1w, lsl 32", "movz %vdx, %v1x, lsl 32",
        "movz %vdw, %v1w, lsl 48", "movz %vdx, %v1x, lsl 48",

        "movn %vdw, %v1w" ,        "movn %vdx, %v1x" ,
        "movn %vdw, %v1w, lsl 16", "movn %vdx, %v1x, lsl 16",
        "movn %vdw, %v1w, lsl 32", "movn %vdx, %v1x, lsl 32",
        "movn %vdw, %v1w, lsl 48", "movn %vdx, %v1x, lsl 48",

        "movk %vdw, %v1w" ,        "movk %vdx, %v1x" ,
        "movk %vdw, %v1w, lsl 16", "movk %vdx, %v1x, lsl 16",
        "movk %vdw, %v1w, lsl 32", "movk %vdx, %v1x, lsl 32",
        "movk %vdw, %v1w, lsl 48", "movk %vdx, %v1x, lsl 48",
    };

    Value *dst = tac->dst;
    Value *src1 = dup_value(tac->src1);

    if (!dst->vreg) panic("Expected a vreg for dst in process_integer_constant_move_to_register()");
    if (!src1->is_constant) panic("Expected a constant for src1 in process_integer_constant_move_to_register()");

    long constant_value = src1->int_value;
    int size_offset = dst->target_size > 3;

    int c[4] = {
        constant_value         & 0xffff,
        (constant_value >> 16) & 0xffff,
        (constant_value >> 32) & 0xffff,
        (constant_value >> 48) & 0xffff,
    };

    int zeroes = c[0] == 0 + c[1] == 0;
    int ones = c[0] == 0xffff + c[1] == 0xffff;

    if (dst->target_size > 3) {
        zeroes += c[2] == 0 + c[3] == 0;
        ones += c[2] == 0xffff + c[3] == 0xffff;
    }

    tac->operation.id = IR_NOP;
    tac->dst = 0;
    tac->src1 = 0;
    tac->src2 = 0;

    int base_operation;
    int base_value;
    int negate = 0;

    if (zeroes >= ones) {
        base_operation = MOVZ;
        base_value = 0;
        negate = 0;
    }
    else {
        base_operation = MOVN;
        base_value = 0xffff;
        negate = 1;
    }

    int half_words = dst->target_size > 3 ? 4 : 2;

    int initted = 0;
    int emissions = 0;

    for (int i = 0; i < half_words; i++) {
        if (c[i] == base_value) continue;

        tac = new_tac_after(tac, AARCH64_OP_MOV, dst, dup_value(src1), NULL);
        tac->src1->int_value = c[i];
        if (!initted) {
            if (negate)
                tac->src1->int_value = (~c[i]) & 0xffff;
            else
                tac->src1->int_value = c[i];

            tac->target_template = templates[base_operation][i][size_offset];
            initted = 1;
        }
        else {
            tac->target_template = templates[MOVK][i][size_offset];
            tac->src1->int_value = c[i];
        }

        emissions++;
    }

    if (!emissions) {
        tac = new_tac_after(tac, AARCH64_OP_MOV, dst, src1, NULL);
        tac->src1->int_value = 0;
        tac->target_template = templates[base_operation][0][size_offset];
    }

    return tac;
}

void make_stack_offsets(Function *function) {} // TODO aarch64

// Determine which registers are used in a function, push them onto the stack and return the list
static Tac *insert_push_callee_saved_registers(Tac *ir, Tac *tac, int *saved_registers) {
    for (int i = 0; i < physical_register_count; i++) {
        if (saved_registers[i]) {
            cur_stack_push_count++;
            ir = insert_target_instruction(ir, AARCH64_OP_PUSH_DOUBLE_WORD, new_preg_value(i), 0, 0, "str %vdx, [sp, #-8]!");
        }
    }

    // TODO aarch64. For now, align the stack to 16-bytes here
    if (cur_stack_push_count & 1) {
        cur_stack_push_count++;
        ir = insert_target_instruction(ir, AARCH64_OP_ALLOCATE_STACK, 0, 0, 0, "sub sp, sp, #8");
    }

    return ir;
}

// TODO aarch64 push saved registers
static Tac *insert_end_of_function(Tac *ir, int *saved_registers) {
    int popped_args = 0;
    for (int i = physical_register_count - 1; i >= 0; i--)
        if (saved_registers[i]) {
            popped_args += 1;
            ir = insert_target_instruction(ir, AARCH64_OP_POP_DOUBLE_WORD, new_preg_value(i), 0, 0, "ldr %vdx, [sp], 8");
        }

    // TODO aarch64. For now, align the stack to 16-bytes here
    if (popped_args & 1) {
        ir = insert_target_instruction(ir, AARCH64_OP_DEALLOCATE_STACK, 0, 0, 0, "add sp, sp, #8");
    }

    ir = insert_target_instruction(ir, AARCH64_OP_LDP, 0, 0, 0, "ldp x29, x30, [sp], 48"); // TODO aarch64 allocate stack
    ir = insert_target_instruction(ir, AARCH64_OP_RET_FROM_FUNC, 0, 0, 0, "ret");

    return ir;
}

void add_final_instructions(Function *function) {
    int added_end_of_function;  // To ensure a double epilogue isn't emitted

    Tac *ir = function->ir;

    cur_stack_push_count = 0;

    // Add function prologue
    ir = insert_target_instruction(ir, AARCH64_OP_STP, 0, 0, 0, "stp x29, x30, [sp, -48]!"); // TODO aarch64 allocate stack
    ir = insert_target_instruction(ir, AARCH64_OP_MOV, 0, 0, 0, "mov x29, sp");

    int *saved_registers = make_saved_registers(function);
    ir = insert_push_callee_saved_registers(ir, function->ir, saved_registers);

    // TODO aarch64 stack

    while (ir) {
        added_end_of_function = 0;

        switch (ir->operation.id) {
            case IR_NOP:
                break;

            case AARCH64_OP_MOV_INT_CST:
                ir = process_integer_constant_move_to_register(ir);
                break;

            case AARCH64_OP_CALL: {
                // TODO aarch64 dedupe code with x86
                ir->operation.id = IR_NOP;

                Tac *orig_ir = ir;

                // A function can be either a direct function or a function pointer

                Tac *tac = new_instruction(AARCH64_OP_CALL_FROM_FUNC);

                if (!orig_ir->src1->function_call.function_symbol) {
                    panic("TODO aarch64 codegen function call from register");
                }
                else {
                    wasprintf(&(tac->target_template), "bl %s", orig_ir->src1->function_call.function_symbol->global_identifier);
                    append_to_list(allocated_strings, tac->target_template);
                 }

                ir = insert_tac_after(ir, tac);
                break;
            }

            case IR_RETURN:
                ir = insert_end_of_function(ir, saved_registers);
                added_end_of_function = 1;
                break;
        }

        ir = ir->next;
    }

    // Add function epilogue
    ir = function->ir;
    while (ir->next) ir = ir->next;

    // Special case for main, return 0 if no return statement is present
    if (function_is_main(function))
        ir = insert_target_instruction(ir, AARCH64_OP_MOV, new_preg_value(REG_R00), 0, 0, "mov w0, 0");

    if (!added_end_of_function)
        insert_end_of_function(ir, saved_registers);

    wfree(saved_registers);
}

void optimize_final_instructions(Function *function) {} // TODO aarch64

void merge_rsp_func_call_add_subs(Function *function) {} // TODO aarch64

// Output code from the IR of a function
static void output_function_body_code(Symbol *symbol) {
    int function_pc = symbol->function->type->function->param_count;

    for (Tac *tac = symbol->function->ir; tac; tac = tac->next) {
        if (tac->label) fprintf(output_file, ".L%d:\n", tac->label);
        if (tac->operation.id != IR_NOP) {
            // output_debug_loc(tac); // TODO aarch64
            output_aarch64_operation(tac, function_pc);
        }
    }
}

// Output data for a defined object symbol
void output_defined_object_symbol(Symbol *symbol) {
    panic("TODO aarch64 output_defined_object_symbol for %s\n", symbol->identifier);
}

// TODO aarch64 check completeness
void output_code(char *input_filename, char *output_filename) {
    open_output_file(input_filename, output_filename);

    fprintf(output_file, "    .arch armv8-a\n\n");

    output_object_symbols();

    // Output string literals
    if (string_literal_count > 0) {
        fprintf(output_file, "\n    .section .rodata\n\n");
        fprintf(output_file, ".Ltext0:\n\n");

        for (int i = 0; i < string_literal_count; i++) {
            StringLiteral *sl = &(string_literals[i]);

            if (sl->is_wide_char)
                fprintf(output_file, "    .align   4\n");
            else
                fprintf(output_file, "    .align   2\n");

            fprintf(output_file, ".LS%d:\n", i);
            fprintf_escaped_string_literal(output_file, sl, 1);
        }
        fprintf(output_file, "\n");
    }

    // Output code
    fprintf(output_file, "    .text\n");

    // Output symbols for all functions that are defined and have external linkage
    for (int i = 0; i < global_scope->symbol_list->length; i++) {
        Symbol *symbol = global_scope->symbol_list->elements[i];
        if (symbol->type->type == TYPE_FUNCTION && symbol->function->is_defined) {
            if (symbol->linkage == LINKAGE_EXTERNAL)
                fprintf(output_file, "    .globl  %s\n", symbol->identifier);
            fprintf(output_file, "    .type   %s, %%function\n", symbol->global_identifier);
        }
    }

    fprintf(output_file, "\n");

    label_count = 0; // Used in label renumbering

    string_literal_count = 0;

    // Output functions code
    for (int i = 0; i < global_scope->symbol_list->length; i++) {
        Symbol *symbol = global_scope->symbol_list->elements[i];
        if (symbol->type->type == TYPE_FUNCTION && symbol->function->is_defined) {
            fprintf(output_file, "%s:\n", symbol->identifier);
            fprintf(output_file, ".L%s.start:\n", symbol->identifier);
            output_function_body_code(symbol);
            fprintf(output_file, "    .size       %s, .-%s\n", symbol->global_identifier, symbol->global_identifier);
            fprintf(output_file, ".L%s.end:\n", symbol->identifier);
            fprintf(output_file, "\n");
        }
    }

    fclose(output_file);
}
