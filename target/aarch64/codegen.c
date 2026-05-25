#include <string.h>

#include "wcc.h"
#include "aarch64.h"

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

void make_stack_offsets(Function *function) {} // TODO aarch64

// TODO aarch64 push saved registers
static Tac *insert_end_of_function(Tac *ir) {
    // TODO aarch64 pop saved registers

    ir = insert_target_instruction(ir, AARCH64_OP_LDP, 0, 0, 0, "ldp x29, x30, [sp], 48"); // TODO aarch64 allocate stack
    ir = insert_target_instruction(ir, AARCH64_OP_RET_FROM_FUNC, 0, 0, 0, "ret");

    return ir;
}

void add_final_instructions(Function *function) {
    int added_end_of_function;  // To ensure a double epilogue isn't emitted

    Tac *ir = function->ir;

    // Add function prologue
    ir = insert_target_instruction(ir, AARCH64_OP_STP, 0, 0, 0, "stp x29, x30, [sp, -48]!"); // TODO aarch64 allocate stack
    ir = insert_target_instruction(ir, AARCH64_OP_MOV, 0, 0, 0, "mov x29, sp");

    // TODO aarch64 push saved registers
    // TODO aarch64 stack

    while (ir) {
        added_end_of_function = 0;

        switch (ir->operation.id) {
            case IR_NOP:
                break;

            case AARCH64_OP_CALL: {
                // TODO aarch64 dedupe code with x86
                ir->operation.id = IR_NOP;

                Tac *orig_ir = ir;

                // A function can be either a direct function or a function pointer
                Type *function_type = ir->src1->type->type == TYPE_FUNCTION ? ir->src1->type : ir->src1->type->target;
                if (function_type->function->is_variadic) {
                    panic("TODO aarch64 codegen varargs function call");
                }

                Tac *tac = new_instruction(AARCH64_OP_CALL_FROM_FUNC);

                if (!orig_ir->src1->function_call.function_symbol) {
                    panic("TODO aarch64 codegen function call from register");
                }
                else {
                    // If a function has been defined locally, call it directly, otherwise use the PLT
                    if (orig_ir->src1->function_call.function_symbol->function && orig_ir->src1->function_call.function_symbol->function->is_defined) {
                         wasprintf(&(tac->target_template), "bl %s", orig_ir->src1->function_call.function_symbol->global_identifier);
                         append_to_list(allocated_strings, tac->target_template);
                     }
                    else {
                         panic("TODO aarch64 codegen function call PLT for undefined functions");
                    }
                 }

                ir = insert_tac_after(ir, tac);
                break;
            }

            case IR_RETURN:
                ir = insert_end_of_function(ir);
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
        insert_end_of_function(ir);
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

    // TODO aarch64 Output string literals

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
