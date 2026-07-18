#include <string.h>

#include "wcc.h"
#include "aarch64.h"

#define HISTORICAL_PUSHED_FUNCTION_PARAM_OFFSET 2

static int cur_function_stack_size;                 // The stack size of the current function
static int cur_function_has_function_calls;         // If the current function makes any function calls
static int cur_function_stack_space_for_x29_x30;    // Amount of stack space allocated for x29 and x30

static void append_register_name(char *buffer, int preg, int is_32bit) {
    if (preg >= REG_R00 && preg <= REG_R30) {
        *buffer++ = is_32bit_to_aarch64_size(is_32bit);
        sprintf(buffer, "%d", preg);
    }
    else if (preg >= REG_V00 && preg <= REG_V30)
        sprintf(buffer, "v%d", preg);
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
    else if (preg >= REG_V00 && preg <= REG_V30)
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
static int get_stack_offset(Value *v) {
    // A legacy from the original x86_64 code. Pushed args start at stack_index=2.
    int stack_index = v->stack_index;

    if (stack_index >= HISTORICAL_PUSHED_FUNCTION_PARAM_OFFSET)
        // Function parameter
        return 8 * (stack_index - HISTORICAL_PUSHED_FUNCTION_PARAM_OFFSET);
    else if (stack_index < 0) {
        if (!v->stack_offset && !debug_instsel_tiling) panic("Unexpected zero stack offset");
        return cur_function_stack_size - v->stack_offset;
    }
    else
        panic("Unexpected zero stack_index");
}

// Convert stack_offset, which has negative values for locals and positive values for passed arguments, into
// an offset relative to the sp.
void make_aarch64_stack_offsets(Function *function) {
    for (Tac *tac = function->ir; tac; tac = tac->next) {
        if (tac->dst  && tac->dst ->stack_offset) tac->dst ->stack_offset = get_stack_offset(tac->dst  ) + tac->dst ->offset;
        if (tac->src1 && tac->src1->stack_offset) tac->src1->stack_offset = get_stack_offset(tac->src1 ) + tac->src1->offset;
        if (tac->src2 && tac->src2->stack_offset) tac->src2->stack_offset = get_stack_offset(tac->src2 ) + tac->src2->offset;
    }
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
            else if (expect_preg && v->preg != -1)
                append_register_name(buffer, v->preg, is_32bit);
            else if (v->is_constant) {
                sprintf(buffer, "%ld", v->int_value);
            }
            else if (v->is_string_literal)
                sprintf(buffer, ".LS%d", v->string_literal_index);
            else if (v->global_symbol) {
                // TODO aarch64 long double
                if (v->offset) {
                    // This code normally has been called after offsets have been removed.
                    // However, debug prints can lead to this codepath being run.
                    sprintf(buffer, "%s + offset", v->global_symbol->global_identifier, v->offset);
                }
                else {
                    if (v->load_from_got)
                        panic("TODO aarch64 load from GIT");
                    else
                        sprintf(buffer, "%s", v->global_symbol->global_identifier);
                }
            }
            else if (v->stack_index)
                sprintf(buffer, "sp, %d", v->stack_offset);
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
} // TODO aarch64

static void output_aarch64_operation(Tac *tac, int function_pc) {
    char *buffer = render_target_operation(tac, function_pc, 1);
    if (buffer) {
        fprintf(output_file, "    %s\n", buffer);
        wfree(buffer);
    }
}

// Check if the constant in value v be used in a pre or post increment/decrement stp/ldp operation,
// and if not, insert a register load using the scratch register r14 aka REG_R14
static Tac *insert_load_for_add_sub_for_stp(Tac *ir) {
    if IS_ADD_SUB_IMMEDIATE(ir->src1->int_value) return ir;

    Value *dst = new_value();
    dst->type = new_type(TYPE_LONG);
    dst->preg = REG_R14;

    Value *v = dup_value(ir->src1);
    ir->src1 = dst;

    ir = new_tac_before(ir, AARCH64_OP_MOV, dst, v, 0, 0);
    ir->target_template = "mov %vdx, %v1x";
    ir = process_integer_constant_move_to_register(ir);

    return ir->next;
}

// Add push statements for callee saved registers.
// Loop over pairs of saved registers, so that the stack is always aligned on 16 bytes.
static Tac *insert_push_callee_saved_registers(Tac *ir, Tac *tac, int *saved_registers) {
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
                new_preg_value(saved_register1), new_preg_value(saved_register2), "stp %v1x, xzr, [sp, #-16]!");
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
                    new_preg_value(saved_register1), new_preg_value(saved_register2), "ldp %v1x, xzr, [sp], #16");
            }
        }

        i -= 2;
    }

    return ir;
}

static Tac *insert_function_prologue(Function *function, Tac *ir, int *saved_registers) {
    // Save callee saved registers
    ir = insert_push_callee_saved_registers(ir, function->ir, saved_registers);

    // Insert optional x29 and x30 stack saves and allocate stack for locals
    if (cur_function_stack_space_for_x29_x30)
        ir = insert_target_instruction(ir, AARCH64_OP_STP, 0, 0, 0, "stp x29, x30, [sp, -16]!");

    // Allocate stack space for locals
    if (cur_function_stack_size) {
        Value *v = new_integral_constant(TYPE_LONG, cur_function_stack_size);
        ir = insert_target_instruction(ir, AARCH64_OP_SUB, 0, v, 0, "sub sp, sp, %v1x");
        ir = insert_load_for_add_sub_for_stp(ir);
    }

    return ir;
}

static Tac *insert_end_of_function(Function *function, Tac *ir, int *saved_registers) {
    if (cur_function_stack_size) {
        // Reclaim the function' local's stack space
        Value *v1 = new_integral_constant(TYPE_LONG, cur_function_stack_size);
        ir = insert_target_instruction(ir, AARCH64_OP_ADD, 0, v1, 0, "add sp, sp, %v1x");
        ir = insert_load_for_add_sub_for_stp(ir);
    }

    // Restore x29 and x30
    if (cur_function_stack_space_for_x29_x30)
        ir = insert_target_instruction(ir, AARCH64_OP_LDP, 0, 0, 0, "ldp x29, x30, [sp], 16");

    // Restore callee saved registers
    ir = insert_pop_callee_saved_registers(ir, saved_registers);

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

void add_final_instructions(Function *function) {
    cur_function_stack_size = function->stack_size;

    make_aarch64_stack_offsets(function);
    add_load_memory_instructions(function);
    add_store_memory_instructions(function);

    prepare_x29_x30_stack_saves(function);
    int *saved_registers = make_saved_registers(function);

    int added_end_of_function;  // To ensure a double epilogue isn't emitted

    Tac *ir = function->ir;

    // Add function prologue
    ir = insert_function_prologue(function, ir, saved_registers);

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
                ir = insert_end_of_function(function, ir, saved_registers);
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
        insert_end_of_function(function, ir, saved_registers);

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
