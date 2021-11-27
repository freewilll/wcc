#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "wcc.h"

void get_debug_env_value(char *key, int *val) {
    char *env_value;
    if ((env_value = getenv(key)) && !strcmp(env_value, "1")) *val = 1;
}

char *replace_extension(char *input, char *ext) {
    char *p  = strrchr(input,'.');
    if (!p) {
        char *result = malloc(strlen(input) + strlen(ext) + 2);
        sprintf(result, "%s.%s", input, ext);
        return result;
    }
    else {
        int len = p - input + strlen(ext) + 1;
        char *result = malloc(len + 1);
        memcpy(result, input, p - input);
        result[p  - input] = '.';
        memcpy(result + (p - input + 1), ext, strlen(ext));
        result[len] = 0;
        return result;
    }
}

static void run_preprocessor(char *input_filename, char *preprocessor_output_filename, char *builtin_include_path, int verbose) {
    char *command = malloc(1024 * 100);

    char *directives_str = malloc(1024 * 100);
    char *dptr = directives_str;

    for (StrMapIterator it = strmap_iterator(directives); !strmap_iterator_finished(&it); strmap_iterator_next(&it)) {
        char *key = strmap_iterator_key(&it);
        char *value = strmap_get(directives, key);
        if (strcmp(value, ""))
            dptr += sprintf(dptr, " -D %s=\"%s\"", key, value);
        else
            dptr += sprintf(dptr, " -D %s", key);
    }
    *dptr = 0;

    sprintf(command, "tcc -I %s%s -E %s -o %s", builtin_include_path, directives_str, input_filename, preprocessor_output_filename);
    if (verbose) {
        sprintf(command, "%s %s", command, "-v");
        printf("%s\n", command);
    }
    int result = system(command);
    if (result != 0) exit(result >> 8);

    free(command);
    free(directives_str);
}

static void builtin_preprocessor(char *input_filename) {
    preprocess(input_filename);
}

int main(int argc, char **argv) {
    print_ir1 = 0;
    print_ir2 = 0;

    opt_enable_vreg_renumbering = 1;
    opt_enable_register_coalescing = 1;
    opt_enable_preferred_pregs = 1;
    opt_enable_live_range_coalescing = 1;
    opt_spill_furthest_liveness_end = 0;
    opt_short_lr_infinite_spill_costs = 1;
    opt_optimize_arithmetic_operations = 1;
    warn_integer_constant_too_large = 1;
    warn_assignment_types_incompatible = 1;

    int verbose = 0;        // Print invoked program command lines
    int run_compiler = 1;   // Compile .c file
    int run_assembler = 1;  // Assemble .s file
    int run_linker = 1;     // Link .o file
    int run_builtin_preprocessor = 0; // Run built in preprocessor
    int target_is_object_file = 0;
    int target_is_assembly_file = 0;
    int help = 0;
    int print_stack_register_count = 0;
    int print_instr_rules = 0;
    int print_instr_precision_decrease_rules = 0;
    int print_symbols = 0;

    char *output_filename = 0;
    int input_filename_count = 0;
    char **input_filenames = malloc(sizeof(char *) * MAX_INPUT_FILENAMES);
    memset(input_filenames, 0, sizeof(char *) * MAX_INPUT_FILENAMES);
    char **linker_input_filenames = malloc(sizeof(char *) * MAX_INPUT_FILENAMES);
    memset(linker_input_filenames, 0, sizeof(char *) * MAX_INPUT_FILENAMES);

    init_allocate_registers();
    init_instruction_selection_rules();

    directives = new_strmap();

    // Determine path to the builtin include directory
    char *builtin_include_path;
    wasprintf(&builtin_include_path, "%sinclude", base_path(argv[0]));

    argc--;
    argv++;
    while (argc > 0) {
        if (*argv[0] == '-') {
                 if (argc > 0 && !strcmp(argv[0], "-h"                                )) { help = 1;                                 argc--; argv++; }
            else if (argc > 0 && !strcmp(argv[0], "-v"                                )) { verbose = 1;                              argc--; argv++; }
            else if (argc > 0 && !strcmp(argv[0], "-s"                                )) { print_symbols = 1;                        argc--; argv++; }
            else if (argc > 0 && !strcmp(argv[0], "--prc"                             )) { print_stack_register_count = 1;           argc--; argv++; }
            else if (argc > 0 && !strcmp(argv[0], "--ir1"                             )) { print_ir1 = 1;                            argc--; argv++; }
            else if (argc > 0 && !strcmp(argv[0], "--ir2"                             )) { print_ir2 = 1;                            argc--; argv++; }
            else if (argc > 0 && !strcmp(argv[0], "-fno-coalesce-live-range"          )) { opt_enable_live_range_coalescing = 0;     argc--; argv++; }
            else if (argc > 0 && !strcmp(argv[0], "-fspill-furthest-liveness-end"     )) { opt_spill_furthest_liveness_end = 1;      argc--; argv++; }
            else if (argc > 0 && !strcmp(argv[0], "-fno-dont-spill-short-live-ranges" )) { opt_short_lr_infinite_spill_costs = 0;    argc--; argv++; }
            else if (argc > 0 && !strcmp(argv[0], "-fno-optimize-arithmetic"          )) { opt_optimize_arithmetic_operations = 0;   argc--; argv++; }
            else if (argc > 0 && !strcmp(argv[0], "-fno-vreg-renumbering"             )) { opt_enable_vreg_renumbering = 0;          argc--; argv++; }
            else if (argc > 0 && !strcmp(argv[0], "--trigraphs"                       )) { opt_enable_trigraphs = 1;                 argc--; argv++; }
            else if (argc > 0 && !strcmp(argv[0], "--print-rules"                     )) { print_instr_rules = 1;                    argc--; argv++; }
            else if (argc > 0 && !strcmp(argv[0], "--print-precision-decrease-rules"  )) { print_instr_precision_decrease_rules = 1; argc--; argv++; }

            else if (argc > 0 && !strcmp(argv[0], "--debug-function-param-allocation"       )) { debug_function_param_allocation = 1;        argc--; argv++; }
            else if (argc > 0 && !strcmp(argv[0], "--debug-function-arg-mapping"            )) { debug_function_arg_mapping = 1;             argc--; argv++; }
            else if (argc > 0 && !strcmp(argv[0], "--debug-function-param-mapping"          )) { debug_function_param_mapping = 1;           argc--; argv++; }
            else if (argc > 0 && !strcmp(argv[0], "--debug-ssa-mapping-local-stack-indexes" )) { debug_ssa_mapping_local_stack_indexes = 1;  argc--; argv++; }
            else if (argc > 0 && !strcmp(argv[0], "--debug-ssa"                             )) { debug_ssa = 1;                              argc--; argv++; }
            else if (argc > 0 && !strcmp(argv[0], "--debug-ssa-liveout"                     )) { debug_ssa_liveout = 1;                      argc--; argv++; }
            else if (argc > 0 && !strcmp(argv[0], "--debug-ssa-cfg"                         )) { debug_ssa_cfg = 1;                          argc--; argv++; }
            else if (argc > 0 && !strcmp(argv[0], "--debug-ssa-idom"                        )) { debug_ssa_idom = 1;                         argc--; argv++; }
            else if (argc > 0 && !strcmp(argv[0], "--debug-ssa-phi-insertion"               )) { debug_ssa_phi_insertion = 1;                argc--; argv++; }
            else if (argc > 0 && !strcmp(argv[0], "--debug-ssa-phi-renumbering"             )) { debug_ssa_phi_renumbering = 1;              argc--; argv++; }
            else if (argc > 0 && !strcmp(argv[0], "--debug-ssa-live-range"                  )) { debug_ssa_live_range = 1;                   argc--; argv++; }
            else if (argc > 0 && !strcmp(argv[0], "--debug-ssa-vreg-renumbering"            )) { debug_ssa_vreg_renumbering = 1;             argc--; argv++; }
            else if (argc > 0 && !strcmp(argv[0], "--debug-ssa-interference-graph"          )) { debug_ssa_interference_graph = 1;           argc--; argv++; }
            else if (argc > 0 && !strcmp(argv[0], "--debug-ssa-live-range-coalescing"       )) { debug_ssa_live_range_coalescing = 1;        argc--; argv++; }
            else if (argc > 0 && !strcmp(argv[0], "--debug-ssa-spill-cost"                  )) { debug_ssa_spill_cost = 1;                   argc--; argv++; }
            else if (argc > 0 && !strcmp(argv[0], "--debug-register-allocation"             )) { debug_register_allocation = 1;              argc--; argv++; }
            else if (argc > 0 && !strcmp(argv[0], "--debug-graph-coloring"                  )) { debug_graph_coloring = 1;                   argc--; argv++; }
            else if (argc > 0 && !strcmp(argv[0], "--debug-instsel-tree-merging"            )) { debug_instsel_tree_merging = 1;             argc--; argv++; }
            else if (argc > 0 && !strcmp(argv[0], "--debug-instsel-tree-merging-deep"       )) { debug_instsel_tree_merging_deep = 1;        argc--; argv++; }
            else if (argc > 0 && !strcmp(argv[0], "--debug-instsel-igraph-simplification"   )) { debug_instsel_igraph_simplification = 1;    argc--; argv++; }
            else if (argc > 0 && !strcmp(argv[0], "--debug-instsel-tiling"                  )) { debug_instsel_tiling = 1;                   argc--; argv++; }
            else if (argc > 0 && !strcmp(argv[0], "--debug-instsel-cost-graph"              )) { debug_instsel_cost_graph = 1;               argc--; argv++; }
            else if (argc > 0 && !strcmp(argv[0], "--debug-instsel-spilling"                )) { debug_instsel_spilling = 1;                 argc--; argv++; }
            else if (argc > 0 && !strcmp(argv[0], "--debug-stack-frame-layout"              )) { debug_stack_frame_layout = 1;               argc--; argv++; }
            else if (argc > 0 && !strcmp(argv[0], "-Wno-integer-constant-too-large"         )) { warn_integer_constant_too_large = 0;        argc--; argv++; }
            else if (argc > 0 && !strcmp(argv[0], "-Wno-warn-assignment-types-incompatible" )) { warn_assignment_types_incompatible = 0;     argc--; argv++; }

            else if (argc > 0 && !strcmp(argv[0], "-S")) {
                run_assembler = 0;
                run_linker = 0;
                target_is_assembly_file = 1;
                argc--;
                argv++;
            }
            else if (argc > 0 && !memcmp(argv[0], "-c", 2)) {
                run_linker = 0;
                target_is_object_file = 1;
                argc--;
                argv++;
            }
            else if (argc > 0 && !memcmp(argv[0], "-E", 2)) {
                run_compiler = 0;
                run_assembler = 0;
                run_linker = 0;
                run_builtin_preprocessor = 1;
                argc--;
                argv++;
            }
            else if (argc > 1 && !memcmp(argv[0], "-o", 2)) {
                output_filename = argv[1];
                argc -= 2;
                argv += 2;
            }
            else if (argc > 1 && !memcmp(argv[0], "-D", 2)) {
                strmap_put(directives, argv[1], "");
                argc -= 2;
                argv += 2;
            }
            else if (argc > 1 && !strcmp(argv[0], "--rule-coverage-file")) {
                rule_coverage_file = argv[1];
                argc -= 2;
                argv += 2;
            }
            else {
                printf("Unknown parameter %s\n", argv[0]);
                exit(1);
            }
        }
        else {
            if (input_filename_count == MAX_INPUT_FILENAMES) panic("Exceeded max input filenames");
            input_filenames[input_filename_count++] = argv[0];
            argc--;
            argv++;
        }
    }

    if (help) {
        printf("Usage: wcc [-S -c -E -v -d -ir1 -ir2 -ir3 -s -frp -iir -h] [-o OUTPUT-FILE] INPUT-FILE\n\n");
        printf("Flags\n");
        printf("-h                                          Help\n");
        printf("-S                                          Compile only; do not assemble or link\n");
        printf("-c                                          Compile and assemble, but do not link\n");
        printf("-E                                          Run the preprocessor\n");
        printf("-o <file>                                   Output file. Use - for stdout. Defaults to the source file with extension .s\n");
        printf("-D <directive>                              Set a directive\n");
        printf("-v                                          Display the programs invoked by the compiler\n");
        printf("-d                                          Debug output\n");
        printf("-s                                          Output symbol table\n");
        printf("--prc                                       Output spilled register count\n");
        printf("--ir1                                       Output intermediate representation after parsing\n");
        printf("--ir2                                       Output intermediate representation after x86_64 rearrangements\n");
        printf("--print-rules                               Print instruction selection rules\n");
        printf("--print-precision-decrease-rules            Print instruction selection rules that decrease precision\n");
        printf("--rule-coverage-file <file>                 Append matched rules to file\n");
        printf("\n");
        printf("Optimization flags:\n");
        printf("-fno-coalesce-live-range                    Disable SSA live range coalescing\n");
        printf("-fspill-furthest-liveness-end               Spill liveness intervals that have the greatest end liveness interval\n");
        printf("-fno-dont-spill-short-live-ranges           Disable infinite spill costs for short live ranges\n");
        printf("-fno-optimize-arithmetic                    Disable arithmetic optimizations\n");
        printf("-fno-vreg-renumbering                       Disable renumbering of vregs before live range coalesces\n");
        printf("--trigraphs                                 Enable preprocessing of trigraphs\n");
        printf("\n");
        printf("Warning flags:\n");
        printf("-Wno-integer-constant-too-large             Disable too large integer constant warnings\n");
        printf("-Wno-warn-assignment-types-incompatible     Disable warnings about incopmatible type assignments\n");
        printf("\n");
        printf("Debug flags:\n");
        printf("--debug-function-param-allocation\n");
        printf("--debug-function-arg-mapping\n");
        printf("--debug-function-param-mapping\n");
        printf("--debug-ssa-mapping-local-stack-indexes\n");
        printf("--debug-ssa\n");
        printf("--debug-ssa-liveout\n");
        printf("--debug-ssa-cfg\n");
        printf("--debug-ssa-idom\n");
        printf("--debug-ssa-phi-insertion\n");
        printf("--debug-ssa-phi-renumbering\n");
        printf("--debug-ssa-live-range\n");
        printf("--debug-ssa-vreg-renumbering\n");
        printf("--debug-ssa-interference-graph\n");
        printf("--debug-ssa-live-range-coalescing\n");
        printf("--debug-ssa-spill-cost\n");
        printf("--debug-register-allocation\n");
        printf("--debug-graph-coloring\n");
        printf("--debug-instsel-tree-merging\n");
        printf("--debug-instsel-tree-merging-deep\n");
        printf("--debug-instsel-igraph-simplification\n");
        printf("--debug-instsel-tiling\n");
        printf("--debug-instsel-cost-graph\n");
        printf("--debug-instsel-spilling\n");
        printf("--debug-stack-frame-layout\n");

        exit(1);
    }

    get_debug_env_value("DEBUG_FUNCTION_PARAM_ALLOCATION", &debug_function_param_allocation);
    get_debug_env_value("DEBUG_FUNCTION_ARG_MAPPING", &debug_function_arg_mapping);
    get_debug_env_value("DEBUG_FUNCTION_PARAM_MAPPING", &debug_function_param_mapping);
    get_debug_env_value("DEBUG_SSA_MAPPING_LOCAL_STACK_INDEXES", &debug_ssa_mapping_local_stack_indexes);
    get_debug_env_value("DEBUG_SSA", &debug_ssa);
    get_debug_env_value("DEBUG_SSA_LIVEOUT", &debug_ssa_liveout);
    get_debug_env_value("DEBUG_SSA_CFG", &debug_ssa_cfg);
    get_debug_env_value("DEBUG_SSA_IDOM", &debug_ssa_idom);
    get_debug_env_value("DEBUG_SSA_PHI_INSERTION", &debug_ssa_phi_insertion);
    get_debug_env_value("DEBUG_SSA_PHI_RENUMBERING", &debug_ssa_phi_renumbering);
    get_debug_env_value("DEBUG_SSA_LIVE_RANGE", &debug_ssa_live_range);
    get_debug_env_value("DEBUG_SSA_VREG_RENUMBERING", &debug_ssa_vreg_renumbering);
    get_debug_env_value("DEBUG_SSA_INTERFERENCE_GRAPH", &debug_ssa_interference_graph);
    get_debug_env_value("DEBUG_SSA_LIVE_RANGE_COALESCING", &debug_ssa_live_range_coalescing);
    get_debug_env_value("DEBUG_SSA_SPILL_COST", &debug_ssa_spill_cost);
    get_debug_env_value("DEBUG_REGISTER_ALLOCATION", &debug_register_allocation);
    get_debug_env_value("DEBUG_GRAPH_COLORING", &debug_graph_coloring);
    get_debug_env_value("DEBUG_INSTSEL_TREE_MERGING", &debug_instsel_tree_merging);
    get_debug_env_value("DEBUG_INSTSEL_TREE_MERGING_DEEP", &debug_instsel_tree_merging_deep);
    get_debug_env_value("DEBUG_INSTSEL_IGRAPH_SIMPLIFICATION", &debug_instsel_igraph_simplification);
    get_debug_env_value("DEBUG_INSTSEL_TILING", &debug_instsel_tiling);
    get_debug_env_value("DEBUG_INSTSEL_COST_GRAPH", &debug_instsel_cost_graph);
    get_debug_env_value("DEBUG_INSTSEL_SPILLING", &debug_instsel_spilling);
    get_debug_env_value("DEBUG_STACK_FRAME_LAYOUT", &debug_stack_frame_layout);

    if (print_instr_rules) {
        print_rules();
        exit(0);
    }

    if (print_instr_precision_decrease_rules) {
        check_rules_dont_decrease_precision();
        exit(0);
    }

    if (!input_filename_count) {
        printf("Missing input filename\n");
        exit(1);
    }

    if (output_filename && input_filename_count > 1 && (target_is_object_file || target_is_assembly_file)) {
        panic("cannot specify -o with -c or -S with multiple files");
    }

    for (int i = 0; i < input_filename_count; i++) {
        char *input_filename = input_filenames[i];
        int filename_len = strlen(input_filename);

        if (filename_len > 2 && input_filename[filename_len - 2] == '.') {
            if (input_filename[filename_len - 1] == 'o') {
                run_compiler = 0;
                run_assembler = 0;
                run_linker = 1;
            }
            else if (input_filename[filename_len - 1] == 's') {
                run_compiler = 0;
                run_assembler = 1;
            }
        }
    }

    char *command = malloc(1024 * 100);

    for (int i = 0; i < input_filename_count; i++) {
        char *input_filename = input_filenames[i];

        char *assembler_input_filename, *assembler_output_filename;
        char *compiler_output_filename;

        if (run_builtin_preprocessor)
            builtin_preprocessor(input_filename);

        if (run_compiler) {
            char *local_output_filename;
            if (!output_filename) {
                if (run_linker)
                    output_filename = "a.out";
                else if (run_assembler)
                    local_output_filename = replace_extension(input_filename, "o");
                else
                    local_output_filename = replace_extension(input_filename, "s");
            }
            else
                local_output_filename = output_filename;

            char *preprocessor_output_filename = make_temp_filename("/tmp/XXXXXX.c");
            run_preprocessor(input_filename, preprocessor_output_filename, builtin_include_path, verbose);

            char *compiler_input_filename = input_filename;
            if (run_assembler)
                compiler_output_filename = make_temp_filename("/tmp/XXXXXX.s");
            else
                compiler_output_filename = strdup(local_output_filename);

            if (run_assembler) {
                if (run_compiler)
                    assembler_input_filename = compiler_output_filename;
                else
                    assembler_input_filename = input_filename;

                if (run_linker)
                    assembler_output_filename = make_temp_filename("/tmp/XXXXXX.o");
                else
                    assembler_output_filename = strdup(local_output_filename);
            }

            compile(preprocessor_output_filename, compiler_input_filename, compiler_output_filename);

            if (print_symbols) dump_symbols();
            if (print_stack_register_count) printf("stack_register_count=%d\n", total_stack_register_count);
        }

        if (run_assembler) {
            sprintf(command, "as -64 %s -o %s", assembler_input_filename, assembler_output_filename);
            if (verbose) {
                sprintf(command, "%s %s", command, "-v");
                printf("%s\n", command);
            }
            int result = system(command);
            if (result != 0) exit(result >> 8);
            linker_input_filenames[i] = assembler_output_filename;
        }
        else
            linker_input_filenames[i] = input_filename;
    }

    if (run_linker) {
        int len = 0;
        for (int i = 0; i < input_filename_count; i++) len += strlen(linker_input_filenames[i]);
        len += input_filename_count - 1; // For the spaces
        char *linker_input_filenames_str = malloc(len + 1);
        memset(linker_input_filenames_str, ' ', len + 1);
        linker_input_filenames_str[len] = 0;

        int k = 0;
        for (int i = 0; i < input_filename_count; i++) {
            char *input_filename = linker_input_filenames[i];
            len = strlen(linker_input_filenames[i]);
            for (int j = 0; j < len; j++) linker_input_filenames_str[k++] = input_filename[j];
            k++;
        }

        sprintf(command, "gcc %s -o %s", linker_input_filenames_str, output_filename);
        if (verbose) {
            sprintf(command, "%s %s", command, "-v");
            printf("%s\n", command);
        }
        int result = system(command);
        if (result != 0) exit(result >> 8);
    }

    exit(0);
}
