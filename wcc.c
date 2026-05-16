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

void init_memory_management_for_translation_unit(void) {
    init_type_allocations();
    init_value_allocations();
    init_function_allocations();
    init_ir();
}

void free_memory_for_translation_unit(void) {
    free_types();
    free_values();
    free_functions();
    free_ir();
}

char *make_temp_filename(char *template) {
    template = wstrdup(template);
    int fd = mkstemps(template, 2);
    if (fd == -1) {
        perror("in make_temp_filename");
        exit(1);
    }
    close(fd);

    return template;
}

typedef void (*CompilerPhaseFunction)(Function *function);

typedef struct compiler_phase {
    CompilerPhaseFunction function;
    CompilerPhaseTag start_tag;
    CompilerPhaseTag stop_tag;
    const char *description;
} CompilerPhase;

static void make_interference_graph_local(Function *function) {
    make_interference_graph(function, 0);
}

static void write_rule_coverage_file_local(Function *function) {
    (void) function;
    if (rule_coverage_file) write_rule_coverage_file();
}

static void print_ir_local(Function *function) {
    print_ir(function, 0);

}
static CompilerPhase compiler_phases[] = {
    // Parser post processing
    { convert_enums,                               PH_BEGIN, PH_NONE,  "Convert enums" },
    { process_struct_and_union_copies,             PH_NONE,  PH_NONE,  "Process struct and union copies" },
    { merge_consecutive_labels,                    PH_NONE,  PH_NONE,  "Merge consecutive labels" },
    { renumber_labels,                             PH_NONE,  PH_NONE,  "Renumber labels" },
    { allocate_value_vregs,                        PH_NONE,  PH_NONE,  "Allocate value vregs" },
    { add_zero_memory_instructions,                PH_NONE,  PH_NONE,  "Add zero memory instructions" },
    { move_long_doubles_to_the_stack,              PH_NONE,  PH_NONE,  "Move long doubles to the stack" },
    { allocate_value_stack_indexes,                PH_NONE,  PH_NONE,  "Allocate value stack indexes" },
    { process_bit_fields,                          PH_NONE,  PH_NONE,  "Process bit fields" },
    { remove_unused_function_call_results,         PH_NONE,  PH_NONE,  "Remove unused function call results" },

    // Arithmetic optimization
    { optimize_arithmetic_operations,              PH_NONE, PH_NONE,   "Optimizing arithmetic operations" },

    // Convert the IR to SSA and back out again
    // It's a bit pointless, since the SSA representation isn't made use of,
    // but the plan is to use it in the future.
    { rewrite_lvalue_reg_assignments,              PH_SSA,   PH_NONE,  "Rewrite lvalue register assignments" },
    { analyze_dominance,                           PH_NONE,  PH_DOM,   "Analyzing dominance" },
    { make_globals_and_var_blocks,                 PH_NONE,  PH_NONE,  "Make globals and variable blocks" },
    { insert_phi_functions,                        PH_NONE,  PH_PHI,   "Insert phi functions" },
    { free_globals_and_var_blocks,                 PH_NONE,  PH_NONE,  NULL },
    { rename_phi_function_variables,               PH_NONE,  PH_NONE,  "Rename phi function variables" },
    { make_live_ranges,                            PH_NONE,  PH_NONE,  "Make live ranges" },
    { blast_vregs_with_live_ranges,                PH_NONE,  PH_NONE,  "Blast vregs with live ranges" },
    { free_phi_functions,                          PH_NONE,  PH_NONE,  NULL },
    { free_dominance,                              PH_NONE,  PH_NONE,  NULL },

    // The target x86_64 specific phase follows
    { convert_long_doubles_jz_and_jnz,             PH_NONE,  PH_NONE,  "Convert long double conditional jumps" },
    { transform_int128_instructions,               PH_NONE,  PH_NONE,  "Lower int128 code" },

    // Function param and arg processing
    { process_target_functions,                   PH_NONE,  PH_PARAM,  "Process target function calls, args, params and return values" },

    // Misc IR conversions
    { add_PIC_load_and_saves,                      PH_NONE,  PH_NONE,  "Adding PIC loads & saves" },
    { convert_functions_address_of,                PH_NONE,  PH_NONE,  "Converting & functions to loads" },
    { rewrite_lvalue_reg_assignments,              PH_NONE,  PH_NONE,  "Converting lvalue assignments" },

    // Coalesce live ranges
    { analyze_dominance,                           PH_NONE,  PH_NONE,  "Analyzing dominance" },
    { make_uevar_and_varkill,                      PH_NONE,  PH_NONE,  "Make uevar and varkill" },
    { coalesce_live_ranges,                        PH_NONE,  PH_LIVE,  "Coalesce live ranges" },

    // Instruction selection
    { check_instrsel_register_sanity,              PH_NONE,  PH_LIVE,  "Check registers are SSA-like for instrsel" },
    { free_interference_graph,                     PH_NONE,  PH_NONE,  NULL },
    { free_live_range_spill_cost,                  PH_NONE,  PH_NONE,  NULL },
    { free_vreg_preg_classes,                      PH_NONE,  PH_NONE,  NULL },
    { free_preferred_live_range_preg_indexes,      PH_NONE,  PH_NONE,  NULL },
    { select_instructions,                         PH_NONE,  PH_NONE,  "Instruction Selection" },
    { free_liveout,                                PH_NONE,  PH_NONE,  NULL },
    { free_uevar_and_varkill,                      PH_NONE,  PH_NONE,  NULL },
    { free_dominance,                              PH_NONE,  PH_NONE,  NULL },
    { compress_vregs,                              PH_NONE,  PH_NONE,  "Compress vregs" },

    // Coalesce live ranges again after instruction selection
    { analyze_dominance,                           PH_NONE,  PH_NONE,  "Analyzing dominance" },
    { make_uevar_and_varkill,                      PH_NONE,  PH_NONE,  "Make uevar and varkill" },
    { coalesce_live_ranges,                        PH_NONE,  PH_NONE,  "Coalesce live ranges" },
    { free_interference_graph,                     PH_NONE,  PH_NONE,  NULL },

    // Allocate registers
    { remove_vreg_self_moves,                      PH_NONE,  PH_NONE,  "Remove vreg self moves" },
    { write_rule_coverage_file_local,              PH_NONE,  PH_INSTR, "Write rule coverage file" },
    { make_interference_graph_local,               PH_NONE,  PH_NONE,  "Make interference graph" },
    { free_liveout,                                PH_NONE,  PH_NONE,  NULL },
    { free_uevar_and_varkill,                      PH_NONE,  PH_NONE,  NULL },
    { free_dominance,                              PH_NONE,  PH_NONE,  NULL },
    { allocate_registers,                          PH_NONE,  PH_NONE,  "Allocate registers" },
    { perform_peephole_optimization,               PH_NONE,  PH_NONE,  "Perform target peephole optimizations" },

    { free_interference_graph,                     PH_NONE,  PH_NONE,  NULL },
    { free_live_range_spill_cost,                  PH_NONE,  PH_NONE,  NULL },
    { free_vreg_preg_classes,                      PH_NONE,  PH_NONE,  NULL },
    { free_preferred_live_range_preg_indexes,      PH_NONE,  PH_NONE,  NULL },

    // Final x86 manipulations
    { add_spill_code,                              PH_NONE,  PH_SPILL, "Add spill code" },
    { make_stack_offsets,                          PH_NONE,  PH_NONE,  "Make stack offsets" },
    { add_final_x86_instructions,                  PH_NONE,  PH_NONE,  "Add final x86 instructions" },
    { remove_nops,                                 PH_NONE,  PH_NONE,  "Remove nops" },
    { merge_rsp_func_call_add_subs,                PH_NONE,  PH_END,   "Merge rsp function call adjustments" },
};

void run_compiler_phases(Function *function, char *function_name, int start_at, int stop_at) {
    if (log_compiler_phase_durations) {
        set_debug_logging_start_time();
        debug_log("Starting compiler phases for %s", function_name);
    }

    int started = 0;
    int stopped = 0;

    int phase_count = sizeof(compiler_phases) / sizeof(compiler_phases[0]);
    for (int i = 0; i < phase_count; i++) {
        if (sanity_check_ir) {
            sanity_test_ir_linkage(function);
            sanity_test_values(function);
        }

        CompilerPhase *phase = &compiler_phases[i];

        if (!started) {
            if (phase->start_tag != start_at) continue;
            started = 1;
        }

        if (phase->description && log_compiler_phase_durations) debug_log("%s", phase->description);

        phase->function(function);

        if (phase->stop_tag == stop_at) {
            stopped = 1;
            break;
        }
    }

    if (!started) panic("Unknown compiler phase start tag %d", start_at);
    if (!stopped) panic("Compiler phase stop tag %d was not reached after start tag %d", stop_at, start_at);
    if (stop_at != PH_END) return;

    if (log_compiler_phase_durations) debug_log("Finished compilation");
}

static void compile_internals(void) {
    init_lexer_from_string(internals());
    parse();
    free_lexer();
}

void compile(char *input, char *original_input_filename, char *output_filename) {
    init_parser();

    if (!debug_dont_compile_internals) compile_internals();

    memcpy_symbol = lookup_symbol("memcpy", global_scope, 0);
    memset_symbol = lookup_symbol("memset", global_scope, 0);

    compile_phase = CP_PARSING;
    init_lexer_from_string(input);
    parse();
    free_lexer();

    if (debug_exit_after_parser) goto exit_compile;

    compile_phase = CP_POST_PARSING;

    init_instrsel();
    init_codegen();

    // Compile all functions
    for (int i = 0; i < global_scope->symbol_list->length; i++) {
        Symbol *symbol = global_scope->symbol_list->elements[i];
        if (symbol->type->type == TYPE_FUNCTION && symbol->function->is_defined) {
            Function *function = symbol->function;
            if (print_ir1) print_ir(function, 0);

            run_compiler_phases(function, symbol->identifier, PH_BEGIN, PH_END);
            if (print_ir2) print_ir(function, 1);
        }
    }

    output_code(original_input_filename, output_filename);

    for (int i = 0; i < global_scope->symbol_list->length; i++) {
        Symbol *symbol = global_scope->symbol_list->elements[i];
        if (symbol->type->type == TYPE_FUNCTION && symbol->function->is_defined)
            free_function(symbol->function, 1);
    }

    free_instrsel();
    free_codegen();

exit_compile:;
    free_parser();
}
