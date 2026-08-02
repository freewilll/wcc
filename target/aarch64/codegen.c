#include <string.h>

#include "wcc.h"
#include "aarch64.h"

#define HISTORICAL_PUSHED_FUNCTION_PARAM_OFFSET 2

static int cur_function_stack_size;                 // The stack size of the current function
static int cur_function_has_function_calls;         // If the current function makes any function calls
static int cur_function_stack_space_for_x29_x30;    // Amount of stack space allocated for x29 and x30

static void append_register_name(char *buffer, int preg, int is_32bit) {
    if (preg >= REG_R00 && preg <= REG_R30) {
        *buffer++ = is_32bit_to_aarch64_integer_register_size(is_32bit);
        sprintf(buffer, "%d", preg);
    }
    else if (preg >= REG_V00 && preg <= REG_V31) {
        *buffer++ = is_32bit_to_aarch64_floating_point_register_size(is_32bit);
        sprintf(buffer, "%d", preg - REG_V00);
    }
    else if (preg == REG_SP)
        sprintf(buffer, "sp");
    else if (preg == REG_FSPR)
        sprintf(buffer, "fspr");
    else
        panic("Unable to translate preg %d to a string", preg);
}

// Codegen
char *register_name(int preg) {
    char *buffer = wcalloc(1, 16);

    if (preg >= REG_R00 && preg <= REG_R30)
        sprintf(buffer, "r%d", preg);
    else if (preg >= REG_V00 && preg <= REG_V31)
        sprintf(buffer, "r%d", preg);
    else if (preg == REG_SP)
        sprintf(buffer, "sp");
    else if (preg == REG_FSPR)
        sprintf(buffer, "fspr");
    else
        panic("Unable to translate preg %d to a string", preg);

    return buffer;
}

// Get offset from the stack in bytes, from a stack_index for function args
// or stack offset for local params.
// The stack layout for a function with
// - 16 bytes of local variables
// - 16 bytes of reserved space for function calls
// - 2 function args pushed to the stack.
// During execution, sp has to always be aligned to 16 bytes.
//
// stack index  offset    what
//  +3          +56       Pushed arg #1
//  +2          +48       Pushed arg #0
// ----------- Stack at the point of the function call -----------
//              +48       Pushed x29 and x30 (optional)
// ----------- Callee saved variables -----------
//              +32       Callee saved registers, e.g. 1 pair
// ----------- Local variables -----------
//              +24       Padding
// -1           +20       Second local variable
// -2           +16       First local variable, e.g. an int
// ----------- Reserved space for function calls -----------
//              +8        Arg 1
//              +0        Arg 0
static void process_stack_offset(Tac *tac, Value *v, int *stack_offsets) {
    // printf("process_stack_offset stack_index=%d offset=%d\n", v->stack_index, v->stack_offset);
    int result;

    // A legacy from the original x86_64 code. Pushed args start at stack_index=2.
    int stack_index = v->stack_index;

    if (stack_index >= HISTORICAL_PUSHED_FUNCTION_PARAM_OFFSET) {
        // Function parameter
        result = 8 * (stack_index - HISTORICAL_PUSHED_FUNCTION_PARAM_OFFSET);

        if (stack_offsets) stack_offsets[-stack_index] = result;
    }
    else if (stack_index <= 0) {
        result = cur_function_stack_size - v->stack_offset;
    }

    if (v->spilled) {
        v->stack_offset = result;
    }
    else {
        v->stack_offset = result + v->offset;
        v->offset = 0;
    }

}

// Some instructions inherit an offset from previous instructions.
// Offsets are only applicable to a small subset of the instructions.
// They will either remain, in the case of a STR or LDR, or have the instruction rewritten.
#define REMOVE_OFFSET(v)  if ((v) && (v)->offset && tac->operation.id != AARCH64_OP_LDR && tac->operation.id != AARCH64_OP_STR && tac->operation.id != AARCH64_OP_ADRP) { \
    (v) = dup_value((v)); \
    (v)->offset = 0; \
}

// Convert stack_offset, which has negative values for locals and positive values for passed arguments, into
// an offset relative to the sp.
void make_aarch64_stack_offsets(Function *function) {
    int *stack_offsets = NULL;

    if (debug_stack_frame_layout) stack_offsets= wmalloc((function->stack_register_count + 1) * sizeof(int));

    cur_function_stack_size = function->stack_size;

    for (Tac *tac = function->ir; tac; tac = tac->next) {
        if (tac->dst  && tac->dst ->stack_index) process_stack_offset(tac, tac->dst,  stack_offsets);
        if (tac->src1 && tac->src1->stack_index) process_stack_offset(tac, tac->src1, stack_offsets);
        if (tac->src2 && tac->src2->stack_index) process_stack_offset(tac, tac->src2, stack_offsets);

        REMOVE_OFFSET(tac->dst);
        REMOVE_OFFSET(tac->src1);
        REMOVE_OFFSET(tac->src2);
    }

    // Print a summary of remapped indexes
    if (debug_stack_frame_layout) {
        int count = function->stack_register_count;
        printf("\nAarch64 modified stack frame for %s:\n", function->identifier);
        printf("Stack index   Offset\n");
        printf("--------------------\n");

        for (int i = 1; i <= count; i++)
            printf("%-4d          %-8d\n", -i, stack_offsets[i]);
    }

    wfree(stack_offsets);
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

            switch (t[1]) {
                case 'w':
                case 'S':
                    t++; is_32bit = 1; break;
                case 'x':
                case 'D':
                    t++; is_32bit = 0; break;
            }

            if (!v) panic("Unexpectedly got a null value while the template %s is expecting it", tac->target_template);

            if (!expect_preg && v->vreg) {
                if (v->global_symbol) panic("Got global symbol in vreg");

                *buffer++ = 'r';
                sprintf(buffer, "%d", v->vreg);
                while (*buffer) buffer++;

                if (is_floating_point_type(v->type))
                    *buffer++ = is_32bit_to_aarch64_floating_point_register_size(is_32bit);
                else
                    *buffer++ = is_32bit_to_aarch64_integer_register_size(is_32bit);

                if (v->offset) {
                    while (*buffer) buffer++;
                    sprintf(buffer, "[%d]", v->offset);
                }
            }
            else if (expect_preg && v->preg != -1) {
                append_register_name(buffer, v->preg, is_32bit);

                if (v->offset) {
                    while (*buffer) buffer++;
                    sprintf(buffer, ", %d", v->offset);
                }

            }
            else if (v->is_constant) {
                if (is_floating_point_type(v->type)) {
                    sprintf(buffer, ".LFP%d", v->offset);
                }
                else {
                    sprintf(buffer, "%ld", v->int_value);
                }
            }
            else if (v->is_string_literal)
                sprintf(buffer, ".LS%d", v->string_literal_index);
            else if (v->global_symbol) {
                // TODO aarch64 long double
                if (v->offset) {
                    // This code normally has been called after offsets have been removed.
                    // However, debug prints can lead to this codepath being run.
                    sprintf(buffer, "%s + offset %d", v->global_symbol->global_identifier, v->offset);
                }
                else {
                    if (v->load_from_got)
                        panic("TODO aarch64 load from GIT");
                    else
                        sprintf(buffer, "%s", v->global_symbol->global_identifier);
                }
            }
            else if (v->stack_index) {
                if (v->stack_offset)
                    sprintf(buffer, "sp, %d", v->stack_offset);
                else
                sprintf(buffer, "sp");
            }
            else if (v->label)
                sprintf(buffer, ".L%d", v->label);
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
}

static void output_aarch64_operation(Tac *tac, int function_pc) {
    char *buffer = render_target_operation(tac, function_pc, 1);
    if (buffer) {
        fprintf(output_file, "    %s\n", buffer);
        wfree(buffer);
    }
}

// Add push statements for callee saved registers.
// Loop over pairs of saved registers, so that the stack is always aligned on 16 bytes.
static Tac *insert_push_callee_saved_registers(Tac *ir, int *saved_registers) {
    int i = 0;
    while (1) {
        int saved_register1 = saved_registers[i];
        if (saved_register1 == -1) break;
        int saved_register2 = saved_registers[i + 1];

        if (saved_register2 != -1) {
            ir = insert_target_instruction(ir, AARCH64_OP_PUSH_DOUBLE_WORD, 0,
                new_preg_value(saved_register1), new_preg_value(saved_register2), "stp %v1x, %v2x, [sp, #-16]!");
        }
        else {
            ir = insert_target_instruction(ir, AARCH64_OP_PUSH_DOUBLE_WORD, 0,
                new_preg_value(saved_register1), new_preg_value(saved_register2), "str %v1x, [sp, #-16]!");
        }

        i += 2;
    }

    return ir;
}

static Tac *insert_pop_callee_saved_registers(Tac *ir, int *saved_registers) {
    int i = (physical_register_count + 2) & (~1);
    while (i >= 0) {
        int saved_register1 = saved_registers[i];
        int saved_register2 = saved_registers[i + 1];
        if (saved_register1 != -1 || saved_register2 != -1) {
            if (saved_register2 != -1) {
                ir = insert_target_instruction(ir, AARCH64_OP_POP_DOUBLE_WORD, 0,
                    new_preg_value(saved_register1), new_preg_value(saved_register2), "ldp %v1x, %v2x, [sp], #16");
            }
            else {
                ir = insert_target_instruction(ir, AARCH64_OP_POP_DOUBLE_WORD, 0,
                    new_preg_value(saved_register1), new_preg_value(saved_register2), "ldr %v1x, [sp], #16");
            }
        }

        i -= 2;
    }

    return ir;
}

static Tac *insert_function_prologue(Function *function, Tac *ir) {
    // Insert optional x29 and x30 stack saves and allocate stack for locals
    if (cur_function_stack_space_for_x29_x30)
        ir = insert_target_instruction(ir, AARCH64_OP_STP, 0, 0, 0, "stp x29, x30, [sp, -16]!");

    // Allocate stack space for locals
    if (cur_function_stack_size) {
        Value *v = new_integral_constant(TYPE_LONG, cur_function_stack_size);
        ir = insert_target_instruction(ir, AARCH64_OP_SUB, 0, 0, v, "sub sp, sp, %v2x");
        ir = insert_constant_load_for_add_sub_using_preg(ir, REG_R16);
    }

    return ir;
}

static Tac *insert_end_of_function(Function *function, Tac *ir, int *saved_registers_int, int *saved_registers_fp) {
    if (cur_function_stack_size) {
        // Reclaim the function' local's stack space
        Value *v1 = new_integral_constant(TYPE_LONG, cur_function_stack_size);
        ir = insert_target_instruction(ir, AARCH64_OP_ADD, 0, 0, v1, "add sp, sp, %v2x");
        ir = insert_constant_load_for_add_sub_using_preg(ir, REG_R16);
    }

    // Restore x29 and x30
    if (cur_function_stack_space_for_x29_x30)
        ir = insert_target_instruction(ir, AARCH64_OP_LDP, 0, 0, 0, "ldp x29, x30, [sp], 16");

    // Restore callee saved registers
    ir = insert_pop_callee_saved_registers(ir, saved_registers_fp);
    ir = insert_pop_callee_saved_registers(ir, saved_registers_int);

    // Add the return instruction
    ir = insert_target_instruction(ir, AARCH64_OP_RET_FROM_FUNC, 0, 0, 0, "ret");

    return ir;
}

static void prepare_x29_x30_stack_saves(Function *function) {
    // Determine if the function has any function calls
    cur_function_has_function_calls = 0;
    for (Tac *tac = function->ir; tac; tac = tac->next)
        if (tac->operation.id == AARCH64_OP_CALL) cur_function_has_function_calls = 1;

    cur_function_stack_space_for_x29_x30 = cur_function_has_function_calls ? 16 : 0;
}

static void register_floating_point_literals(Function *function) {
    for (Tac *tac = function->ir; tac; tac = tac->next) {
        if (tac->src1 && tac->src1->is_constant && is_floating_point_type(tac->src1->type)) {
            Value *v = tac->src1;
                 if (v->type->type == TYPE_FLOAT)       v->offset = add_float_literal(v);
            else if (v->type->type == TYPE_DOUBLE)      v->offset = add_double_literal(v);
            else if (v->type->type == TYPE_LONG_DOUBLE) v->offset = add_long_double_literal(v);
        }
    }
}

// Ensure the first instruction is a nop it's conventient to append after
// the first instruction
static void prepend_nop(Function *function) {
    Tac *tac = new_instruction(IR_NOP);
    tac->next = function->ir;
    function->ir->prev = tac;
    function->ir = tac;
}

void add_final_instructions(Function *function) {
    prepend_nop(function);
    add_load_memory_instructions(function);
    add_store_memory_instructions(function);
    add_address_of_instructions(function);
    expand_adrp_instructions(function);
    expand_indirect_offsets(function);
    register_floating_point_literals(function);

    prepare_x29_x30_stack_saves(function);

    int added_end_of_function;  // To ensure a double epilogue isn't emitted

    Tac *ir = function->ir;

    // Saved called saved registers.
    // int and FP registers are loaded/saved in separate blocks since they
    // can't be loaded/saved together with a pair load (ldp).
    int *saved_registers_int = make_saved_registers(function, PC_INT);
    int *saved_registers_fp = make_saved_registers(function, PC_FP);
    ir = insert_push_callee_saved_registers(ir, saved_registers_int);
    ir = insert_push_callee_saved_registers(ir, saved_registers_fp);

    // Add function prologue
    ir = insert_function_prologue(function, ir);

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
                ir = insert_end_of_function(function, ir, saved_registers_int, saved_registers_fp);
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
        insert_end_of_function(function, ir, saved_registers_int, saved_registers_fp);

    wfree(saved_registers_int);
    wfree(saved_registers_fp);
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

    output_floating_point_literals();

    fclose(output_file);
}
