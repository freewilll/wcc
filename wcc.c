#include <stdlib.h>
#include "wcc.h"

void run_compiler_phases(Function *function, int stop_at) {
    // Prepare for SSA phi function insertion
    sanity_test_ir_linkage(function);
    optimize_arithmetic_operations(function);
    rewrite_lvalue_reg_assignments(function);
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
    if (stop_at == COMPILE_STOP_AFTER_REGISTER_ALLOCATION) return;

    // Instruction selection
    select_instructions(function);
    analyze_dominance(function);
    coalesce_live_ranges(function);
    remove_vreg_self_moves(function);
    if (stop_at == COMPILE_STOP_AFTER_INSTRUCTION_SELECTION) return;

    // Register allocation and spilling
    sanity_test_ir_linkage(function);
    allocate_registers(function);
    add_spill_code(function);
    make_function_call_direct_reg_counts(function);
}

void compile(int print_spilled_register_count, char *compiler_input_filename, char *compiler_output_filename) {
    Symbol *symbol;
    Function *function;

    init_lexer(compiler_input_filename);
    init_parser();
    parse();
    check_incomplete_structs();

    // Compile all functions
    symbol = symbol_table;
    while (symbol->identifier) {
        if (symbol->is_function && symbol->function->is_defined) {
            function = symbol->function;
            if (print_ir1) print_ir(function, symbol->identifier);
            post_process_function_parse(function);
            run_compiler_phases(function, COMPILE_EVERYTHING);
            if (print_ir2) print_ir(function, symbol->identifier);
        }
        symbol++;
    }

    output_code(compiler_input_filename, compiler_output_filename);
}
