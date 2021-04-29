#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "wcc.h"

char *make_temp_filename(char *template) {
    int fd;

    template = strdup(template);
    fd = mkstemps(template, 2);
    if (fd == -1) {
        perror("in make_temp_filename");
        exit(1);
    }
    close(fd);

    return strdup(template);
}

void run_compiler_phases(Function *function, int start_at, int stop_at) {
    if (start_at == COMPILE_START_AT_BEGINNING) {
        reverse_function_argument_order(function);
        merge_consecutive_labels(function);
        renumber_labels(function);
        allocate_value_vregs(function);
        allocate_value_stack_indexes(function);
        remove_unused_function_call_results(function);
    }

    // Prepare for SSA phi function insertion
    sanity_test_ir_linkage(function);
    optimize_arithmetic_operations(function);
    rewrite_lvalue_reg_assignments(function);
    add_function_call_result_moves(function);
    add_function_return_moves(function);
    add_function_call_arg_moves(function);
    add_function_param_moves(function);
    analyze_dominance(function);
    if (stop_at == COMPILE_STOP_AFTER_ANALYZE_DOMINANCE) return;

    // Insert SSA phi functions
    sanity_test_ir_linkage(function);
    make_globals_and_var_blocks(function);
    insert_phi_functions(function);
    if (stop_at == COMPILE_STOP_AFTER_INSERT_PHI_FUNCTIONS) return;

    // Come out of SSA and coalesce live ranges
    sanity_test_ir_linkage(function);
    rename_phi_function_variables(function);
    make_live_ranges(function);
    blast_vregs_with_live_ranges(function);
    coalesce_live_ranges(function);
    if (stop_at == COMPILE_STOP_AFTER_LIVE_RANGES) return;

    // Instruction selection
    select_instructions(function);
    analyze_dominance(function);
    coalesce_live_ranges(function);
    remove_vreg_self_moves(function);

    if (stop_at == COMPILE_STOP_AFTER_INSTRUCTION_SELECTION) return;

    // Register allocation and spilling
    sanity_test_ir_linkage(function);
    allocate_registers(function);
    remove_stack_self_moves(function);
    add_spill_code(function);
}

static void compile_externals() {
    char *temp_filename;
    void *f;

    temp_filename = make_temp_filename("/tmp/externals-XXXXXX.c");
    f = fopen(temp_filename, "w");
    fprintf(f, "%s", externals());
    fclose(f);

    init_parser();
    init_lexer(temp_filename);
    parse();
}

void compile(int print_spilled_register_count, char *compiler_input_filename, char *compiler_output_filename) {
    Symbol *symbol;
    Function *function;

    compile_externals();
    init_lexer(compiler_input_filename);
    parse();
    check_incomplete_structs();

    // Compile all functions
    symbol = symbol_table;
    while (symbol->identifier) {
        if (symbol->is_function && symbol->function->is_defined) {
            function = symbol->function;
            if (print_ir1) print_ir(function, symbol->identifier, 0);
            run_compiler_phases(function, COMPILE_START_AT_BEGINNING, COMPILE_STOP_AT_END);
            if (print_ir2) print_ir(function, symbol->identifier, 0);
        }
        symbol++;
    }

    output_code(compiler_input_filename, compiler_output_filename);
}
