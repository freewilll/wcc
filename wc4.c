#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "wc4.h"

// Add a builtin symbol
void add_builtin(char *identifier, int instruction, int type, int is_variadic) {
    struct symbol *s;

    s = new_symbol();
    s->type = type;
    s->identifier = identifier;
    s->is_function = 1;
    s->function = malloc(sizeof(struct function));
    memset(s->function, 0, sizeof(struct function));
    s->function->builtin = instruction;
    s->function->is_variadic = is_variadic;
}

void do_print_symbols() {
    long type, scope, value, stack_index;
    struct symbol *s;
    char *identifier;

    printf("Symbols:\n");
    s = symbol_table;
    while (s->identifier) {
        type = s->type;
        identifier = (char *) s->identifier;
        scope = s->scope;
        value = s->value;
        stack_index = s->stack_index;
        printf("%-20ld %-5ld %-3ld %-3ld %-20ld %s\n", (long) s, type, scope, stack_index, value, identifier);
        s++;
    }
    printf("\n");
}

char *replace_extension(char *input, char *ext) {
    char *p;
    char *result;
    int len;

    p  = strrchr(input,'.');
    if (!p) {
        result = malloc(strlen(input) + strlen(ext) + 2);
        sprintf(result, "%s.%s", input, ext);
        return result;
    }
    else {
        len = p - input + strlen(ext) + 1;
        result = malloc(len + 1);
        memcpy(result, input, p - input);
        result[p  - input] = '.';
        memcpy(result + (p - input + 1), ext, strlen(ext));
        result[len] = 0;
        return result;
    }
}

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

int main(int argc, char **argv) {
    int help, debug, print_symbols, print_code;
    int input_filename_count;
    char **input_filenames, *input_filename, *output_filename, *local_output_filename;
    char *compiler_input_filename, *compiler_output_filename;
    char *assembler_input_filename, *assembler_output_filename;
    char **linker_input_filenames, *linker_input_filenames_str;
    int filename_len;
    char *command, *linker_filenames;
    int i, j, k, len, result;

    verbose = 0;
    compile = 1;
    run_assembler = 1;
    run_linker = 1;
    target_is_object_file = 0;
    target_is_assembly_file = 0;
    help = 0;
    print_spilled_register_count = 0;
    print_ir1 = 0;
    print_ir2 = 0;
    print_ir3 = 0;
    print_symbols = 0;
    fake_register_pressure = 0;
    opt_enable_register_coalescing = 1;
    opt_enable_live_range_coalescing = 1;
    opt_use_registers_for_locals = 0;
    opt_merge_redundant_moves = 0;
    opt_spill_furthest_liveness_end = 0;
    opt_short_lr_infinite_spill_costs = 1;
    opt_optimize_arithmetic_operations = 1;
    output_inline_ir = 0;
    experimental_ssa = 0;
    ssa_physical_register_count = 12;

    output_filename = 0;
    input_filename_count = 0;
    input_filenames = malloc(sizeof(char *) * MAX_INPUT_FILENAMES);
    memset(input_filenames, 0, sizeof(char *) * MAX_INPUT_FILENAMES);
    linker_input_filenames = malloc(sizeof(char *) * MAX_INPUT_FILENAMES);
    memset(linker_input_filenames, 0, sizeof(char *) * MAX_INPUT_FILENAMES);

    debug_register_allocations = 0;

    argc--;
    argv++;
    while (argc > 0) {
        if (*argv[0] == '-') {
                 if (argc > 0 && !strcmp(argv[0], "-h"                                )) { help = 1;                               argc--; argv++; }
            else if (argc > 0 && !strcmp(argv[0], "-v"                                )) { verbose = 1;                            argc--; argv++; }
            else if (argc > 0 && !strcmp(argv[0], "-d"                                )) { debug = 1;                              argc--; argv++; }
            else if (argc > 0 && !strcmp(argv[0], "-s"                                )) { print_symbols = 1;                      argc--; argv++; }
            else if (argc > 0 && !strcmp(argv[0], "--prc"                             )) { print_spilled_register_count = 1;       argc--; argv++; }
            else if (argc > 0 && !strcmp(argv[0], "--ir1"                             )) { print_ir1 = 1;                          argc--; argv++; }
            else if (argc > 0 && !strcmp(argv[0], "--ir2"                             )) { print_ir2 = 1;                          argc--; argv++; }
            else if (argc > 0 && !strcmp(argv[0], "--ir3"                             )) { print_ir3 = 1;                          argc--; argv++; }
            else if (argc > 0 && !strcmp(argv[0], "--frp"                             )) { fake_register_pressure = 1;             argc--; argv++; }
            else if (argc > 0 && !strcmp(argv[0], "--iir"                             )) { output_inline_ir = 1;                   argc--; argv++; }
            else if (argc > 0 && !strcmp(argv[0], "--ssa"                             )) { experimental_ssa = 1;                   argc--; argv++; }
            else if (argc > 0 && !strcmp(argv[0], "-fno-coalesce-registers"           )) { opt_enable_register_coalescing = 0;     argc--; argv++; }
            else if (argc > 0 && !strcmp(argv[0], "-fno-coalesce-live-range"          )) { opt_enable_live_range_coalescing = 0;   argc--; argv++; }
            else if (argc > 0 && !strcmp(argv[0], "-fuse-registers-for-locals"        )) { opt_use_registers_for_locals = 1;       argc--; argv++; }
            else if (argc > 0 && !strcmp(argv[0], "-fmerge-redundant-moves"           )) { opt_merge_redundant_moves = 1;          argc--; argv++; }
            else if (argc > 0 && !strcmp(argv[0], "-fspill-furthest-liveness-end"     )) { opt_spill_furthest_liveness_end = 1;    argc--; argv++; }
            else if (argc > 0 && !strcmp(argv[0], "-fno-dont-spill-short-live-ranges" )) { opt_short_lr_infinite_spill_costs = 0;  argc--; argv++; }
            else if (argc > 0 && !strcmp(argv[0], "-fno-optimize-arithmetic"          )) { opt_optimize_arithmetic_operations = 0; argc--; argv++; }
            else if (argc > 0 && !strcmp(argv[0], "-S"                                )) {
                run_assembler = 0;
                run_linker = 0;
                target_is_assembly_file = 1;
                argc--;
                argv++;
            }
            else if (argc > 0 && !memcmp(argv[0], "-O1", 2)) {
                opt_enable_register_coalescing = 1;
                opt_use_registers_for_locals = 1;
                opt_merge_redundant_moves = 1;
                opt_spill_furthest_liveness_end = 1;
                argc--;
                argv++;
            }
            else if (argc > 0 && !memcmp(argv[0], "-c", 2)) {
                run_linker = 0;
                target_is_object_file = 1;
                argc--;
                argv++;
            }
            else if (argc > 1 && !memcmp(argv[0], "-o", 2)) {
                output_filename = argv[1];
                argc -= 2;
                argv += 2;
            }
            else if (argc > 1 && !memcmp(argv[0], "--ssa-regs", 10)) {
                ssa_physical_register_count = argv[1][0] - '0'; // Really pathetic int to str converstion
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
        printf("Usage: wc4 [-S -c -v -d -ir1 -ir2 -ir3 -s -frp -iir -h] [-o OUTPUT-FILE] INPUT-FILE\n\n");
        printf("Flags\n");
        printf("-S                             Compile only; do not assemble or link\n");
        printf("-c                             Compile and assemble, but do not link\n");
        printf("-o <file>                      Output file. Use - for stdout. Defaults to the source file with extension .s\n");
        printf("-v                             Display the programs invoked by the compiler\n");
        printf("-d                             Debug output\n");
        printf("-s                             Output symbol table\n");
        printf("--frp                          Fake register pressure, for testing spilling code\n");
        printf("--iir                          Output inline intermediate representation\n");
        printf("--ssa                          Enable experimental SSA code\n");
        printf("--ssa-regs <n>                 Limit physical register availability to n in experimental SSA code\n");
        printf("--prc                          Output spilled register count\n");
        printf("--ir1                          Output intermediate representation after parsing\n");
        printf("--ir2                          Output intermediate representation after x86_64 rearrangements\n");
        printf("--ir3                          Output intermediate representation after register allocation\n");
        printf("-h                             Help\n");
        printf("\n");
        printf("Optimization options:\n");
        printf("-fno-coalesce-registers            Disable register coalescing\n");
        printf("-fno-coalesce-live-range           Disable SSA live range coalescing\n");
        printf("-fuse-registers-for-locals         Allocate registers for locals instead of using the stack by default\n");
        printf("-fmerge-redundant-moves            Merge redundant register moves\n");
        printf("-fspill-furthest-liveness-end      Spill liveness intervals that have the greatest end liveness interval\n");
        printf("-fno-dont-spill-short-live-ranges  Disable infinite spill costs for short live ranges\n");
        printf("-fno-optimize-arithmetic           Disable arithmetic optimizations\n ");
        exit(1);
    }

    if (experimental_ssa) {
        opt_use_registers_for_locals = 1;
        opt_enable_register_coalescing = 0;
    }

    if (!input_filename_count) {
        printf("Missing input filename\n");
        exit(1);
    }

    if (output_filename && input_filename_count > 1 && (target_is_object_file || target_is_assembly_file)) {
        panic("cannot specify -o with -c or -S with multiple files");
    }

    for (i = 0; i < input_filename_count; i++) {
        input_filename = input_filenames[i];
        filename_len = strlen(input_filename);

        if (filename_len > 2 && input_filename[filename_len - 2] == '.') {
            if (input_filename[filename_len - 1] == 'o') {
                compile = 0;
                run_assembler = 0;
                run_linker = 1;
            }
            else if (input_filename[filename_len - 1] == 's') {
                compile = 0;
                run_assembler = 1;
            }
        }
    }

    init_callee_saved_registers();

    command = malloc(1024 * 100);

    for (i = 0; i < input_filename_count; i++) {
        input_filename = input_filenames[i];
        parsing_header = 0;

        if (compile) {
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

            if (compile) {
                compiler_input_filename = input_filename;
                if (run_assembler)
                    compiler_output_filename = make_temp_filename("/tmp/XXXXXX.s");
                else
                    compiler_output_filename = strdup(local_output_filename);
            }

            if (run_assembler) {
                if (compile)
                    assembler_input_filename = compiler_output_filename;
                else
                    assembler_input_filename = input_filename;

                if (run_linker)
                    assembler_output_filename = make_temp_filename("/tmp/XXXXXX.o");
                else
                    assembler_output_filename = strdup(local_output_filename);
            }

            symbol_table = malloc(SYMBOL_TABLE_SIZE);
            memset(symbol_table, 0, SYMBOL_TABLE_SIZE);
            next_symbol = symbol_table;

            string_literals = malloc(MAX_STRING_LITERALS);
            string_literal_count = 0;

            all_structs = malloc(sizeof(struct struct_desc *) * MAX_STRUCTS);
            all_structs_count = 0;

            add_builtin("exit",     IR_EXIT,     TYPE_VOID,            0);
            add_builtin("fopen",    IR_FOPEN,    TYPE_VOID + TYPE_PTR, 0);
            add_builtin("fread",    IR_FREAD,    TYPE_INT,             0);
            add_builtin("fwrite",   IR_FWRITE,   TYPE_INT,             0);
            add_builtin("fclose",   IR_FCLOSE,   TYPE_INT,             0);
            add_builtin("close",    IR_CLOSE,    TYPE_INT,             0);
            add_builtin("stdout",   IR_STDOUT,   TYPE_LONG,            0);
            add_builtin("printf",   IR_PRINTF,   TYPE_INT,             1);
            add_builtin("fprintf",  IR_FPRINTF,  TYPE_INT,             1);
            add_builtin("malloc",   IR_MALLOC,   TYPE_VOID + TYPE_PTR, 0);
            add_builtin("free",     IR_FREE,     TYPE_INT,             0);
            add_builtin("memset",   IR_MEMSET,   TYPE_INT,             0);
            add_builtin("memcmp",   IR_MEMCMP,   TYPE_INT,             0);
            add_builtin("strcmp",   IR_STRCMP,   TYPE_INT,             0);
            add_builtin("strlen",   IR_STRLEN,   TYPE_INT,             0);
            add_builtin("strcpy",   IR_STRCPY,   TYPE_INT,             0);
            add_builtin("strrchr",  IR_STRRCHR,  TYPE_CHAR + TYPE_PTR, 0);
            add_builtin("sprintf",  IR_SPRINTF,  TYPE_INT,             1);
            add_builtin("asprintf", IR_ASPRINTF, TYPE_INT,             1);
            add_builtin("strdup",   IR_STRDUP,   TYPE_CHAR + TYPE_PTR, 0);
            add_builtin("memcpy",   IR_MEMCPY,   TYPE_VOID + TYPE_PTR, 0);
            add_builtin("mkstemps", IR_MKTEMPS,  TYPE_INT,             0);
            add_builtin("perror",   IR_PERROR,   TYPE_VOID,            0);
            add_builtin("system",   IR_SYSTEM,   TYPE_INT,             0);

            init_lexer(compiler_input_filename);

            vs_start = malloc(sizeof(struct value *) * VALUE_STACK_SIZE);
            vs_start += VALUE_STACK_SIZE; // The stack traditionally grows downwards
            label_count = 0;
            parse();
            check_incomplete_structs();

            if (print_symbols) do_print_symbols();

            output_code(compiler_input_filename, compiler_output_filename);

            if (print_spilled_register_count) printf("spilled_register_count=%d\n", total_spilled_register_count);

        }

        if (run_assembler) {
            sprintf(command, "as -64 %s -o %s", assembler_input_filename, assembler_output_filename);
            if (verbose) {
                sprintf(command, "%s %s", command, "-v");
                printf("%s\n", command);
            }
            result = system(command);
            if (result != 0) exit(result);
            linker_input_filenames[i] = assembler_output_filename;
        }
        else
            linker_input_filenames[i] = input_filename;
    }

    if (run_linker) {
        len = 0;
        for (i = 0; i < input_filename_count; i++) len += strlen(linker_input_filenames[i]);
        len += input_filename_count - 1; // For the spaces
        linker_input_filenames_str = malloc(len + 1);
        memset(linker_input_filenames_str, ' ', len + 1);
        linker_input_filenames_str[len] = 0;

        k = 0;
        for (i = 0; i < input_filename_count; i++) {
            input_filename = linker_input_filenames[i];
            len = strlen(linker_input_filenames[i]);
            for (j = 0; j < len; j++) linker_input_filenames_str[k++] = input_filename[j];
            k++;
        }

        sprintf(command, "gcc %s -o %s", linker_input_filenames_str, output_filename);
        if (verbose) {
            sprintf(command, "%s %s", command, "-v");
            printf("%s\n", command);
        }
        result = system(command);
        if (result != 0) exit(result);
    }

    exit(0);
}
