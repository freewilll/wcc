#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "wcc.h"

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
    optimize_arithmetic_operations(function);
    add_function_call_arg_moves(function);
    add_function_param_moves(function, function_name);
    add_function_return_moves(function, function_name);
    add_function_call_result_moves(function);
    process_function_varargs(function);
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
    coalesce_live_ranges(function, 1);
    if (stop_at == COMPILE_STOP_AFTER_LIVE_RANGES) return;

    // Instruction selection
    select_instructions(function);
    compress_vregs(function);
    analyze_dominance(function);
    coalesce_live_ranges(function, 0);
    remove_vreg_self_moves(function);
    if (rule_coverage_file) write_rule_coverage_file();

    if (stop_at == COMPILE_STOP_AFTER_INSTRUCTION_SELECTION) return;

    // Register allocation and spilling
    sanity_test_ir_linkage(function);
    make_interference_graph(function, 1);
    allocate_registers(function);
    remove_stack_self_moves(function);
    add_spill_code(function);

    if (stop_at == COMPILE_STOP_AFTER_ADD_SPILL_CODE) return;

    // Final x86_64 changes
    make_stack_offsets(function, function_name);
    add_final_x86_instructions(function, function_name);
    remove_nops(function);
    merge_rsp_func_call_add_subs(function);
}

static void compile_internals(void) {
    char *temp_filename = make_temp_filename("/tmp/internals-XXXXXX.c");
    void *f = fopen(temp_filename, "w");
    fprintf(f, "%s", internals());
    fclose(f);

    init_scopes();
    init_parser();
    init_lexer(temp_filename);
    parse();
}

void compile(char *input_filename, char *original_input_filename, char *output_filename) {
    compile_internals();
    memcpy_symbol = lookup_symbol("memcpy", global_scope, 0);
    memset_symbol = lookup_symbol("memset", global_scope, 0);
    init_lexer(input_filename);
    parse();

    // Keep track of floating point constant values.
    floating_point_literals = malloc(sizeof(FloatingPointLiteral) * MAX_FLOATING_POINT_LITERALS);
    floating_point_literal_count = 0;

    // Compile all functions
    for (int i = 0; i < global_scope->symbol_count; i++) {
        Symbol *symbol = global_scope->symbol_list[i];
        if (symbol->type->type == TYPE_FUNCTION && symbol->type->function->is_defined) {
            Function *function = symbol->type->function;
            if (print_ir1) print_ir(function, symbol->identifier, 0);
            run_compiler_phases(function, symbol->identifier, COMPILE_START_AT_BEGINNING, COMPILE_STOP_AT_END);
            if (print_ir2) print_ir(function, symbol->identifier, 0);
        }
    }

    output_code(original_input_filename, output_filename);
}
