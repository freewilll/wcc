#include "wc4.h"

void do_oar1(Function *function) {
    sanity_test_ir_linkage(function);
    do_oar1a(function);
    do_oar1b(function);
}

void do_oar2(Function *function) {
    sanity_test_ir_linkage(function);
    make_globals_and_var_blocks(function);
    insert_phi_functions(function);
}

void do_oar1a(Function *function) {
    sanity_test_ir_linkage(function);
    disable_live_ranges_coalesce = !opt_enable_live_range_coalescing;
    optimize_arithmetic_operations(function);
    map_stack_index_to_local_index(function);
    rewrite_lvalue_reg_assignments(function);
}

void do_oar1b(Function *function) {
    sanity_test_ir_linkage(function);
    make_vreg_count(function, 0);
    make_control_flow_graph(function);
    make_block_dominance(function);
    make_block_immediate_dominators(function);
    make_block_dominance_frontiers(function);
}

void do_oar3(Function *function) {
    sanity_test_ir_linkage(function);
    rename_phi_function_variables(function);
    make_live_ranges(function);
    blast_vregs_with_live_ranges(function);
    coalesce_live_ranges(function);
}

void do_oar4(Function *function) {
    sanity_test_ir_linkage(function);
    allocate_registers(function);
    assign_vreg_locations(function);
    remove_preg_self_moves(function);
}

void eis1(Function *function) {
    do_oar1(function);
    do_oar2(function);
    do_oar3(function);
    select_instructions(function);
    do_oar1b(function);
    coalesce_live_ranges(function);
    remove_vreg_self_moves(function);
}

void eis2(Function *function) {
    do_oar4(function);
    add_spill_code(function);
}

void experimental_instruction_selection(Function *function) {
    eis1(function);
    eis2(function);
}

void compile(int print_spilled_register_count, char *compiler_input_filename, char *compiler_output_filename) {
    Symbol *symbol;
    Function *function;

    init_lexer(compiler_input_filename);
    init_parser();
    parse();
    check_incomplete_structs();

    // Compile code for all functions
    symbol = symbol_table;
    while (symbol->identifier) {
        if (symbol->is_function && symbol->function->is_defined) {
            function = symbol->function;
            if (print_ir1) print_ir(function, symbol->identifier);
            post_process_function_parse(function);
            experimental_instruction_selection(function);
            if (print_ir2) print_ir(function, symbol->identifier);
        }
        symbol++;
    }

    output_code(compiler_input_filename, compiler_output_filename);
}
