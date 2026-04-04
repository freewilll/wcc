#include "wcc.h"

int print_ir1 = 0;                          // Print IR after parsing
int print_ir2 = 0;                          // Print IR after register allocation
int log_compiler_phase_durations = 0;       // Output logs of how long each compiler phase lasts

int opt_PIC = 0;                            // Make position independent code
int opt_debug_symbols = 0;                  // Add debug symbols
int opt_enable_vreg_renumbering = 0;        // Renumber vregs so the numbers are consecutive
int opt_enable_register_coalescing = 0;     // Merge registers that can be reused within the same operation
int opt_enable_live_range_coalescing = 0;   // Merge live ranges where possible
int opt_spill_furthest_liveness_end = 0;    // Prioritize spilling physical registers with furthest liveness end
int opt_short_lr_infinite_spill_costs = 0;  // Don't spill short live ranges
int opt_optimize_arithmetic_operations = 0; // Optimize arithmetic operations
int opt_enable_preferred_pregs = 0;         // Enable preferred preg selection in register allocator
int opt_enable_trigraphs = 0;               // Enable trigraph preprocessing
int opt_enable_common_symbols = 0;          // Enable .comm symbols
int opt_warnings_are_errors = 0;            // Treat all warnings as errors

int error_incomptatible_pointer_type = 0;
int error_int_conversion = 0;

int warn_integer_constant_too_large = 0;
int warn_assignment_types_incompatible = 0;

int debug_function_param_allocation = 0;
int debug_function_arg_mapping = 0;
int debug_function_param_mapping = 0;
int debug_ssa_mapping_local_stack_indexes = 0;
int debug_ssa = 0;
int debug_ssa_liveout = 0;
int debug_ssa_cfg = 0;
int debug_ssa_dominance = 0;
int debug_ssa_idom = 0;
int debug_ssa_dominance_frontiers = 0;
int debug_ssa_phi_insertion = 0;
int debug_ssa_phi_renumbering = 0;
int debug_ssa_live_range = 0;
int debug_ssa_vreg_renumbering = 0;
int debug_ssa_interference_graph = 0;
int debug_ssa_live_range_coalescing = 0;
int debug_ssa_spill_cost = 0;
int debug_register_allocation = 0;
int debug_graph_coloring = 0;
int debug_instsel_tree_merging = 0;
int debug_instsel_tree_merging_deep = 0;
int debug_instsel_igraph_simplification = 0;
int debug_instsel_tiling = 0;
int debug_instsel_cost_graph = 0;
int debug_instsel_spilling = 0;
int debug_stack_frame_layout = 0;
int debug_exit_after_parser = 0;
int debug_dont_compile_internals = 0;
