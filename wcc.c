#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "wcc.h"

void init_instruction_selection_rules(void) {
    init_generated_instruction_selection_rules();

    init_rules_by_operation();

    rule_coverage = new_set(instr_rule_count - 1);
}

void free_instruction_selection_rules(void) {
    free_set(rule_coverage);
    free_rules_by_operation();
}

char init_memory_management_for_translation_unit(void) {
    init_type_allocations();
    init_value_allocations();
    init_function_allocations();
}

char free_memory_for_translation_unit(void) {
    free_types();
    free_values();
    free_functions();
}

char *make_temp_filename(char *template) {
    template = strdup(template);
    int fd = mkstemps(template, 2);
    if (fd == -1) {
        perror("in make_temp_filename");
        exit(1);
    }
    close(fd);

    return strdup(template);
}

void run_compiler_phases(Function *function, char *function_name, int start_at, int stop_at) {
    if (log_compiler_phase_durations) set_debug_logging_start_time();

    if (log_compiler_phase_durations) debug_log("Starting compiler phases for of %s", function_name);

    if (start_at == COMPILE_START_AT_BEGINNING) {
        convert_enums(function);
        process_struct_and_union_copies(function);
        reverse_function_argument_order(function);
        merge_consecutive_labels(function);
        renumber_labels(function);
        allocate_value_vregs(function);
        convert_long_doubles_jz_and_jnz(function);
        add_zero_memory_instructions(function);
        move_long_doubles_to_the_stack(function);
        allocate_value_stack_indexes(function);
        process_bit_fields(function);
        remove_unused_function_call_results(function);
    }

    // Prepare for SSA phi function insertion
    sanity_test_ir_linkage(function);
    if (log_compiler_phase_durations) debug_log("Optimizing arithmetic operations");
    optimize_arithmetic_operations(function);
    if (log_compiler_phase_durations) debug_log("Function arg/param manipulation");
    add_function_param_moves(function, function_name);
    add_function_return_moves(function, function_name);
    add_function_call_result_moves(function);
    process_function_varargs(function);
    add_function_call_arg_moves(function);

    if (log_compiler_phase_durations) debug_log("Adding PIC loads & saves");
    add_PIC_load_and_saves(function);
    if (log_compiler_phase_durations) debug_log("Converting & functions to loads");
    convert_functions_address_of(function);
    if (log_compiler_phase_durations) debug_log("Converting lvalue assignments");
    rewrite_lvalue_reg_assignments(function);

    if (log_compiler_phase_durations) debug_log("Analyzing dominance");
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

    if (log_compiler_phase_durations) debug_log("Make live ranges");
    make_live_ranges(function);
    blast_vregs_with_live_ranges(function);
    coalesce_live_ranges(function, 1);
    if (stop_at == COMPILE_STOP_AFTER_LIVE_RANGES) return;
    free_interference_graph(function);

    // Instruction selection
    if (log_compiler_phase_durations) debug_log("Instruction Selection");
    select_instructions(function);
    free_dominance(function);
    compress_vregs(function);
    if (log_compiler_phase_durations) debug_log("Analyzing dominance");
    analyze_dominance(function);
    coalesce_live_ranges(function, 0);
    free_interference_graph(function);
    remove_vreg_self_moves(function);
    if (rule_coverage_file) write_rule_coverage_file();

    if (stop_at == COMPILE_STOP_AFTER_INSTRUCTION_SELECTION) return;

    // Register allocation and spilling
    if (log_compiler_phase_durations) debug_log("Register allocation");
    sanity_test_ir_linkage(function);
    make_interference_graph(function, 1, 0);
    free_dominance(function);
    allocate_registers(function);
    free_interference_graph(function);
    remove_stack_self_moves(function);
    add_spill_code(function);

    if (stop_at == COMPILE_STOP_AFTER_ADD_SPILL_CODE) return;

    // Final x86_64 changes
    make_stack_offsets(function, function_name);
    add_final_x86_instructions(function, function_name);
    remove_nops(function);
    merge_rsp_func_call_add_subs(function);

    if (log_compiler_phase_durations) debug_log("Finished compilation");
}

static void compile_internals(void) {
    init_lexer_from_string(internals());
    parse();
}

void compile(char *input, char *original_input_filename, char *output_filename) {
    init_parser();

    if (!debug_dont_compile_internals) compile_internals();

    memcpy_symbol = lookup_symbol("memcpy", global_scope, 0);
    memset_symbol = lookup_symbol("memset", global_scope, 0);

    compile_phase = CP_PARSING;
    init_lexer_from_string(input);
    parse();

    if (debug_exit_after_parser) goto exit_compile;

    compile_phase = CP_POST_PARSING;

    init_codegen();

    // Compile all functions
    for (int i = 0; i < global_scope->symbol_list->length; i++) {
        Symbol *symbol = global_scope->symbol_list->elements[i];
        if (symbol->type->type == TYPE_FUNCTION && symbol->function->is_defined) {
            Function *function = symbol->function;
            if (print_ir1) print_ir(function, symbol->identifier, 0);

            run_compiler_phases(function, symbol->identifier, COMPILE_START_AT_BEGINNING, COMPILE_STOP_AT_END);
            if (print_ir2) print_ir(function, symbol->identifier, 0);
        }
    }

    for (int i = 0; i < global_scope->symbol_list->length; i++) {
        Symbol *symbol = global_scope->symbol_list->elements[i];
        if (symbol->type->type == TYPE_FUNCTION && symbol->function->is_defined)
            free_function(symbol->function);
    }

    output_code(original_input_filename, output_filename);

    free_codegen();

exit_compile:;
    free_parser();
}
