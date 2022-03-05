#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "wcc.h"

void get_debug_env_value(char *key, int *val) {
    char *env_value;
    if ((env_value = getenv(key)) && !strcmp(env_value, "1")) *val = 1;
}

// Check if the filename ends in .c
static int is_c_source_file(const char *const filename) {
    int filename_len = strlen(filename);
    return filename_len > 2 && filename[filename_len - 2] == '.' && filename[filename_len - 1] == 'c';
}

// Check if the filename ends in .s
static int is_assembly_file(const char *const filename) {
    int filename_len = strlen(filename);
    return filename_len > 2 && filename[filename_len - 2] == '.' && filename[filename_len - 1] == 's';
}

// Check if the filename ends in .so or .o
static int is_object_file(const char *const filename) {
    int filename_len = strlen(filename);
    return
        (filename_len > 3 && filename[filename_len - 3] == '.' && filename[filename_len - 2] == 's' && filename[filename_len - 1] == 'o') ||
        (filename_len > 2 && filename[filename_len - 2] == '.' && filename[filename_len - 1] == 'o');
}

char *replace_extension(char *input, char *ext) {
    char *p  = strrchr(input,'.');
    if (!p) {
        char *result = wmalloc(strlen(input) + strlen(ext) + 2);
        sprintf(result, "%s.%s", input, ext);
        return result;
    }
    else {
        int len = p - input + strlen(ext) + 1;
        char *result = wmalloc(len + 1);
        memcpy(result, input, p - input);
        result[p  - input] = '.';
        memcpy(result + (p - input + 1), ext, strlen(ext));
        result[len] = 0;
        return result;
    }
}

static void add_include_path(char *path) {
    int len = strlen(path);
    if (!len) simple_error("Invalid include path");

    if (len > 1 && path[len - 1] == '/') path[len - 1] = 0; // Strip trailing /

    CliIncludePath *cli_include_path = wmalloc(sizeof(CliIncludePath));
    cli_include_path->path = path;
    cli_include_path->next = 0;

    if (!cli_include_paths) cli_include_paths = cli_include_path;
    else {
        CliIncludePath *cd = cli_include_paths;
        while (cd->next) cd = cd->next;
        cd->next = cli_include_path;
    }
}

static void add_library_path(char *path) {
    int len = strlen(path);
    if (!len) simple_error("Invalid library path");

    if (len > 1 && path[len - 1] == '/') path[len - 1] = 0; // Strip trailing /

    CliLibraryPath *cli_library_path = wmalloc(sizeof(CliLibraryPath));
    cli_library_path->path = path;
    cli_library_path->next = 0;

    if (!cli_library_paths) cli_library_paths = cli_library_path;
    else {
        CliLibraryPath *cd = cli_library_paths;
        while (cd->next) cd = cd->next;
        cd->next = cli_library_path;
    }
}

static void add_library(char *library) {
    int len = strlen(library);
    if (!len) simple_error("Invalid library");

    CliLibrary *cli_library = wmalloc(sizeof(CliLibrary));
    cli_library->library = library;
    cli_library->next = 0;

    if (!cli_libraries) cli_libraries = cli_library;
    else {
        CliLibrary *cd = cli_libraries;
        while (cd->next) cd = cd->next;
        cd->next = cli_library;
    }
}

int main(int argc, char **argv) {
    print_ir1 = 0;
    print_ir2 = 0;

    int print_filenames = 0;
    opt_enable_vreg_renumbering = 1;
    opt_enable_register_coalescing = 1;
    opt_enable_preferred_pregs = 1;
    opt_enable_live_range_coalescing = 1;
    opt_spill_furthest_liveness_end = 0;
    opt_short_lr_infinite_spill_costs = 1;
    opt_optimize_arithmetic_operations = 1;
    warn_integer_constant_too_large = 1;
    warn_assignment_types_incompatible = 1;

    int exit_code = 0;
    int verbose = 0;        // Print invoked program command lines
    int run_compiler = 1;   // Compile .c file
    int run_assembler = 1;  // Assemble .s file
    int run_linker = 1;     // Link .o file
    int single_target = 0;
    int help = 0;
    int print_stack_register_count = 0;
    int print_instr_rules = 0;
    int print_symbols = 0;
    int shared = 0;

    char *output_filename = 0;
    List *input_filenames = new_list(32);

    init_allocate_registers();
    init_instruction_selection_rules();

    cli_include_paths = 0;
    cli_library_paths = 0;
    cli_libraries = 0;

    List *extra_linker_args = new_list(0);

    List *directive_cli_strings = new_list(8);

    argc--;
    argv++;
    while (argc > 0) {
        if (*argv[0] == '-') {
                 if (argc > 0 && !strcmp(argv[0], "-h"                                )) { help = 1;                                 argc--; argv++; }
            else if (argc > 0 && !strcmp(argv[0], "-v"                                )) { verbose = 1;                              argc--; argv++; }
            else if (argc > 0 && !strcmp(argv[0], "-s"                                )) { print_symbols = 1;                        argc--; argv++; }
            else if (argc > 0 && !strcmp(argv[0], "-g"                                )) { opt_debug_symbols = 1;                    argc--; argv++; }
            else if (argc > 0 && !strcmp(argv[0], "-fPIC"                             )) { opt_PIC = 1;                              argc--; argv++; }
            else if (argc > 0 && !strcmp(argv[0], "-shared"                           )) { shared = 1;                               argc--; argv++; }
            else if (argc > 0 && !strcmp(argv[0], "--prc"                             )) { print_stack_register_count = 1;           argc--; argv++; }
            else if (argc > 0 && !strcmp(argv[0], "--log-compiler-phase-durations"    )) { log_compiler_phase_durations = 1;         argc--; argv++; }
            else if (argc > 0 && !strcmp(argv[0], "--ir1"                             )) { print_ir1 = 1;                            argc--; argv++; }
            else if (argc > 0 && !strcmp(argv[0], "--ir2"                             )) { print_ir2 = 1;                            argc--; argv++; }
            else if (argc > 0 && !strcmp(argv[0], "-fno-live-range-coalescing"        )) { opt_enable_live_range_coalescing = 0;     argc--; argv++; }
            else if (argc > 0 && !strcmp(argv[0], "-fspill-furthest-liveness-end"     )) { opt_spill_furthest_liveness_end = 1;      argc--; argv++; }
            else if (argc > 0 && !strcmp(argv[0], "-fno-dont-spill-short-live-ranges" )) { opt_short_lr_infinite_spill_costs = 0;    argc--; argv++; }
            else if (argc > 0 && !strcmp(argv[0], "-fno-optimize-arithmetic"          )) { opt_optimize_arithmetic_operations = 0;   argc--; argv++; }
            else if (argc > 0 && !strcmp(argv[0], "-fno-vreg-renumbering"             )) { opt_enable_vreg_renumbering = 0;          argc--; argv++; }
            else if (argc > 0 && !strcmp(argv[0], "--trigraphs"                       )) { opt_enable_trigraphs = 1;                 argc--; argv++; }
            else if (argc > 0 && !strcmp(argv[0], "--print-rules"                     )) { print_instr_rules = 1;                    argc--; argv++; }
            else if (argc > 0 && !strcmp(argv[0], "--print-filenames"                 )) { print_filenames = 1;                      argc--; argv++; }

            else if (argc > 0 && !strcmp(argv[0], "--debug-function-param-allocation"       )) { debug_function_param_allocation = 1;        argc--; argv++; }
            else if (argc > 0 && !strcmp(argv[0], "--debug-function-arg-mapping"            )) { debug_function_arg_mapping = 1;             argc--; argv++; }
            else if (argc > 0 && !strcmp(argv[0], "--debug-function-param-mapping"          )) { debug_function_param_mapping = 1;           argc--; argv++; }
            else if (argc > 0 && !strcmp(argv[0], "--debug-ssa-mapping-local-stack-indexes" )) { debug_ssa_mapping_local_stack_indexes = 1;  argc--; argv++; }
            else if (argc > 0 && !strcmp(argv[0], "--debug-ssa"                             )) { debug_ssa = 1;                              argc--; argv++; }
            else if (argc > 0 && !strcmp(argv[0], "--debug-ssa-liveout"                     )) { debug_ssa_liveout = 1;                      argc--; argv++; }
            else if (argc > 0 && !strcmp(argv[0], "--debug-ssa-cfg"                         )) { debug_ssa_cfg = 1;                          argc--; argv++; }
            else if (argc > 0 && !strcmp(argv[0], "--debug-ssa-dominance"                   )) { debug_ssa_dominance = 1;                    argc--; argv++; }
            else if (argc > 0 && !strcmp(argv[0], "--debug-ssa-idom"                        )) { debug_ssa_idom = 1;                         argc--; argv++; }
            else if (argc > 0 && !strcmp(argv[0], "--debug-ssa-dominance-frontiers"         )) { debug_ssa_dominance_frontiers = 1;          argc--; argv++; }
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
            else if (argc > 0 && !strcmp(argv[0], "--debug-exit-after-parser"               )) { debug_exit_after_parser = 1;                argc--; argv++; }
            else if (argc > 0 && !strcmp(argv[0], "--debug-dont-compile-internals"          )) { debug_dont_compile_internals = 1;           argc--; argv++; }

            else if (argc > 0 && !strcmp(argv[0], "-Werror"                                 )) { opt_warnings_are_errors = 1;                argc--; argv++; }
            else if (argc > 0 && !strcmp(argv[0], "-Wno-integer-constant-too-large"         )) { warn_integer_constant_too_large = 0;        argc--; argv++; }
            else if (argc > 0 && !strcmp(argv[0], "-Wno-warn-assignment-types-incompatible" )) { warn_assignment_types_incompatible = 0;     argc--; argv++; }

            else if (argc > 0 && !strncmp(argv[0], "-O", 2)) { argc--; argv++; } // Ignore -O


            else if (argc > 0 && !strcmp(argv[0], "-S")) {
                run_assembler = 0;
                run_linker = 0;
                single_target = 1;
                argc--;
                argv++;
            }
            else if (argc > 0 && !memcmp(argv[0], "-c", 2)) {
                run_linker = 0;
                single_target = 1;
                argc--;
                argv++;
            }
            else if (argc > 0 && !memcmp(argv[0], "-E", 2)) {
                run_compiler = 0;
                run_assembler = 0;
                run_linker = 0;
                single_target = 1;
                argc--;
                argv++;
            }
            else if (argc > 1 && !memcmp(argv[0], "-o", 2)) {
                output_filename = argv[1];
                argc -= 2;
                argv += 2;
            }
            else if (argc > 1 && !strcmp(argv[0], "-D")) {
                // -D ...
                append_to_list(directive_cli_strings, argv[1]);
                argc -= 2;
                argv += 2;
            }
            else if (argv[0][0] == '-' && argv[0][1] == 'D') {
                // -D...
                append_to_list(directive_cli_strings, &(argv[0][2]));
                argc--;
                argv++;
            }
            else if (argc > 1 && !strcmp(argv[0], "-I")) {
                // -I ...
                add_include_path(argv[1]);
                argc -= 2;
                argv += 2;
            }
            else if (argv[0][0] == '-' && argv[0][1] == 'I') {
                // -I...
                add_include_path(&(argv[0][2]));
                argc--;
                argv++;
            }
            else if (argc > 1 && !strcmp(argv[0], "-L")) {
                // -L ...
                add_library_path(argv[1]);
                argc -= 2;
                argv += 2;
            }
            else if (argv[0][0] == '-' && argv[0][1] == 'L') {
                // -L...
                add_library_path(&(argv[0][2]));
                argc--;
                argv++;
            }
            else if (argc > 1 && !strcmp(argv[0], "-l")) {
                // -l ...
                add_library(argv[1]);
                argc -= 2;
                argv += 2;
            }
            else if (argv[0][0] == '-' && argv[0][1] == 'l') {
                // -L...
                add_library(&(argv[0][2]));
                argc--;
                argv++;
            }
            else if (argc > 0 && !strncmp(argv[0], "-Wl,", 4)) {
                append_to_list(extra_linker_args, &(argv[0][4]));
                argc -= 1;
                argv += 1;
            }
            else if (argc > 1 && !strcmp(argv[0], "--rule-coverage-file")) {
                rule_coverage_file = argv[1];
                argc -= 2;
                argv += 2;
            }
            else if (argc > 0 && !strncmp(argv[0], "-print-prog-name=", 17)) {
                printf("%s\n", &(argv[0][17]));
                exit(0);
            }
            else if (argc > 0 && !strcmp(argv[0], "-dumpmachine")) {
                printf("x86_64-linux-gnu\n");
                exit(0);
            }
            else {
                printf("Unknown parameter %s\n", argv[0]);
                exit(1);
            }
        }
        else {
            append_to_list(input_filenames, argv[0]);
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
        printf("-D <directive>[=<value>]                    Set a directive\n");
        printf("-I <path>                                   Prepend an include path to to search path\n");
        printf("-L <path>                                   Pass library path to the linker\n");
        printf("-l <library>                                Pass library to the linker\n");
        printf("-v                                          Display the programs invoked by the compiler\n");
        printf("-g                                          Add debugging information\n");
        printf("-O<n>                                       Set optimization level (ignored)\n");
        printf("-s                                          Output symbol table\n");
        printf("-fPIC                                       Make position independent code\n");
        printf("-shared                                     Make a shared library\n");
        printf("--prc                                       Output spilled register count\n");
        printf("--log-compiler-phase-durations              Log durations of each compiler phase\n");
        printf("--ir1                                       Output intermediate representation after parsing\n");
        printf("--ir2                                       Output intermediate representation after x86_64 rearrangements\n");
        printf("--print-rules                               Print instruction selection rules\n");
        printf("--print-precision-decrease-rules            Print instruction selection rules that decrease precision\n");
        printf("--rule-coverage-file <file>                 Append matched rules to file\n");
        printf("\n");
        printf("-print-prog-name=<name>                     Print program name\n");
        printf("-dumpmachine                                Print x86_64-linux-gnu\n");
        printf("\n");
        printf("Optimization flags:\n");
        printf("-fno-live-range-coalescing                  Disable SSA live range coalescing\n");
        printf("-fspill-furthest-liveness-end               Spill liveness intervals that have the greatest end liveness interval\n");
        printf("-fno-dont-spill-short-live-ranges           Disable infinite spill costs for short live ranges\n");
        printf("-fno-optimize-arithmetic                    Disable arithmetic optimizations\n");
        printf("-fno-vreg-renumbering                       Disable renumbering of vregs before live range coalesces\n");
        printf("--trigraphs                                 Enable preprocessing of trigraphs\n");
        printf("\n");
        printf("Warning flags:\n");
        printf("-Werror                                     Treat all warnings as errors\n");
        printf("-Wno-integer-constant-too-large             Disable too large integer constant warnings\n");
        printf("-Wno-warn-assignment-types-incompatible     Disable warnings about incopmatible type assignments\n");
        printf("\n");
        printf("Debug flags:\n");
        printf("--print-filenames\n");
        printf("--debug-function-param-allocation\n");
        printf("--debug-function-arg-mapping\n");
        printf("--debug-function-param-mapping\n");
        printf("--debug-ssa-mapping-local-stack-indexes\n");
        printf("--debug-ssa\n");
        printf("--debug-ssa-liveout\n");
        printf("--debug-ssa-cfg\n");
        printf("--debug-ssa-dominance\n");
        printf("--debug-ssa-dominance-frontiers\n");
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
        printf("--debug-exit-after-parser\n");
        printf("--debug-dont-compile-internals\n");

        exit(1);
    }

    get_debug_env_value("DEBUG_FUNCTION_PARAM_ALLOCATION", &debug_function_param_allocation);
    get_debug_env_value("DEBUG_FUNCTION_ARG_MAPPING", &debug_function_arg_mapping);
    get_debug_env_value("DEBUG_FUNCTION_PARAM_MAPPING", &debug_function_param_mapping);
    get_debug_env_value("DEBUG_SSA_MAPPING_LOCAL_STACK_INDEXES", &debug_ssa_mapping_local_stack_indexes);
    get_debug_env_value("DEBUG_SSA", &debug_ssa);
    get_debug_env_value("DEBUG_SSA_LIVEOUT", &debug_ssa_liveout);
    get_debug_env_value("DEBUG_SSA_CFG", &debug_ssa_cfg);
    get_debug_env_value("DEBUG_SSA_DOMINANCE", &debug_ssa_dominance);
    get_debug_env_value("DEBUG_SSA_IDOM", &debug_ssa_idom);
    get_debug_env_value("DEBUG_SSA_DOMINANCE_FRONTIERS", &debug_ssa_dominance_frontiers);
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

    if (!input_filenames->length) {
        printf("Missing input filename\n");
        exit(1);
    }

    if (output_filename && input_filenames->length > 1 && single_target) {
        simple_error("cannot specify -o with -c or -S -E with multiple files");
        exit(1);
    }

    if (debug_exit_after_parser && input_filenames->length != 1) {
        simple_error("--debug-exit-after-parser can only be used with exactly one input file");
        exit(1);
    }

    if (debug_exit_after_parser && run_linker) {
        simple_error("--debug-exit-after-parser must be used with -c");
        exit(1);
    }

    char *command = wmalloc(1024 * 100);

    List *compiler_input_filenames = new_list(input_filenames->length);
    char **assembler_input_filenames = wcalloc(input_filenames->length, sizeof(char *));
    char **linker_input_filenames = wcalloc(input_filenames->length, sizeof(char *));

    // Only run preprocessor & preprocess with file/stdout output
    if (!run_compiler) {
        compile_phase = CP_PREPROCESSING;
        for (int i = 0; i < input_filenames->length; i++) {
            if (is_c_source_file(input_filenames->elements[i])) {
                init_memory_management_for_translation_unit();
                preprocess_to_file(input_filenames->elements[i], output_filename, directive_cli_strings);
                free_memory_for_translation_unit();
            }
        }
        goto exit_main;
    }

    // Preprocessing + compilation phase
    for (int i = 0; i < input_filenames->length; i++) {
        char *input_filename = input_filenames->elements[i];

        if (is_assembly_file(input_filename)) {
            assembler_input_filenames[i] = input_filename;
            continue;
        }

        // Object files need to be at the end
        if (is_object_file(input_filename)) continue;

        init_memory_management_for_translation_unit();
        char *preprocessor_output = preprocess(input_filename, directive_cli_strings);

        char *compiler_output_filename =
            !run_assembler && !run_linker ? (output_filename ? output_filename : replace_extension(input_filename, "s"))
            : make_temp_filename("/tmp/XXXXXX.s");

        if (print_filenames) printf("Compiling %s to %s\n", input_filename, compiler_output_filename);

        compile(preprocessor_output, input_filename, compiler_output_filename);

        if (run_assembler) assembler_input_filenames[i] = compiler_output_filename;

        if (print_symbols) dump_symbols();
        if (print_stack_register_count) printf("stack_register_count=%d\n", total_stack_register_count);

        free_memory_for_translation_unit();
        free(preprocessor_output);

        if (debug_exit_after_parser) goto exit_main;
    }

    // Assembler phase
    if (run_assembler) {
        for (int i = 0; i < input_filenames->length; i++) {
            char *input_filename = assembler_input_filenames[i];
            if (!input_filename) continue;

            char *assembler_output_filename =
                !run_linker ? (output_filename ? output_filename : replace_extension(single_target ? input_filenames->elements[0] : input_filename, "o"))
                : make_temp_filename("/tmp/XXXXXX.o");

            if (print_filenames) printf("Assembling %s to %s\n", input_filename, assembler_output_filename);

            sprintf(command, "as -64 %s -o %s", input_filename, assembler_output_filename);
            if (verbose) {
                sprintf(command, "%s %s", command, "-v");
                printf("%s\n", command);
            }
            int result = system(command);
            if (result != 0) exit(result >> 8);

            if (run_linker) linker_input_filenames[i] = assembler_output_filename;
        }
    }

    // Preprocessing + compilation phase
    for (int i = 0; i < input_filenames->length; i++) {
        char *input_filename = input_filenames->elements[i];
        if (is_object_file(input_filename)) linker_input_filenames[i] = input_filename;
    }

    // Linker phase
    if (run_linker) {
        char *filenames = wmalloc(1024 * 100);
        char *s = filenames;
        for (int i = 0; i < input_filenames->length; i++) {
            char *filename = linker_input_filenames[i];
            if (filename) s += sprintf(s, " %s", (char *) filename);
        }
        if (print_filenames) printf("Running linker on%s\n", filenames);

        s = command;
        s += sprintf(s, "gcc%s", filenames);
        free(filenames);

        for (CliLibraryPath *cli_library_path = cli_library_paths; cli_library_path; cli_library_path = cli_library_path->next)
            s += sprintf(s, " -L %s", cli_library_path->path);

        for (CliLibrary *cli_library = cli_libraries; cli_library; cli_library = cli_library->next)
            s += sprintf(s, " -l %s", cli_library->library);

        char *linker_output_filename = output_filename ? output_filename : "a.out";
        s += sprintf(s, " -o %s", linker_output_filename);

        if (shared) s += sprintf(s, " -shared");

        for (int i = 0; i < extra_linker_args->length; i++)
            s += sprintf(s, " -Wl,%s", (char *) extra_linker_args->elements[i]);

        if (verbose) {
            s += sprintf(s, " -v");
            *s = 0;
            printf("%s\n", command);
        }

        *s = 0;

        int result = system(command);
        if (result != 0) {
            exit_code = result >> 8;
            goto exit_main;
        }
    }


exit_main:
    free_instruction_selection_rules();

    free_cpp_allocated_garbage();
    free_list(input_filenames);
    free_list(compiler_input_filenames);
    free(assembler_input_filenames);
    free(linker_input_filenames);
    free(command);
    free_list(extra_linker_args);
    free_list(directive_cli_strings);

    exit(exit_code);
}
