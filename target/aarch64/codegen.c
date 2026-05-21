#include <string.h>

#include "wcc.h"
#include "aarch64.h"

// Codegen
char *register_name(int preg) {
    return "TODO register_name";
} // TODO aarch64

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

void add_final_instructions(Function *function) {
    // TODO aarch64 prologue
    // TODO aarch64 push saved registers

    // TODO aarch64 function calls

    ir = function->ir;
    while (ir->next) ir = ir->next;

    // Special case for main, return 0 if no return statement is present
    if (function_is_main(function))
        ir = insert_target_instruction(ir, AARCH64_OP_NULL, new_preg_value(REG_R00), 0, 0, "mov w0, 0");

    insert_target_instruction(ir, AARCH64_OP_RET_FROM_FUNC, 0, 0, 0, "ret");

    // TODO aarch64 pop saved registers

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
    fprintf(stderr, "; TODO aarch64 output_defined_object_symbol for %s\n", symbol->identifier);
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
