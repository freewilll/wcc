#include <stdlib.h>
#include <string.h>

#include "wcc.h"

Tac *ir_start, *ir;               // intermediate representation for currently parsed function
int label_count;                  // Global label count, always growing
int cur_loop;                     // Current loop being parsed
int loop_count;                   // Loop counter
int total_stack_register_count;   // Spilled register count for all functions

FloatingPointLiteral *floating_point_literals; // Each floating point literal has an index in this array
int floating_point_literal_count;              // Amount of floating point literals

StrMap *debug_strings;                      // Map of debug strings to identifiers
int debug_string_counter;                   // Counter to uniquely identify a debug string
int last_outputted_filename_id;             // Keep track of last printed .loc filename id
int last_outputted_filename_line_number;    // Keep track of last printed .loc line number

List *allocated_strings;

FILE *output_file; // Output file handle

int elf_section;

int fprintf_escaped_char(void *f, unsigned char c) {
         if (c == '"' ) return fprintf(f, "\\\"");
    else if (c == '\\') return fprintf(f, "\\\\");
    else if (c == '\b') return fprintf(f, "\\b");
    else if (c == '\f') return fprintf(f, "\\f");
    else if (c == '\n') return fprintf(f, "\\n");
    else if (c == '\r') return fprintf(f, "\\r");
    else if (c == '\t') return fprintf(f, "\\t");
    else if (c < 32 || c >= 128) return fprintf_octal_char(f, c);
    else return fprintf(f, "%c", c);
}

int fprintf_octal_char(void *f, char c) {
    if (c == 0)
        return fprintf(f, "\\0");

    int count = 0;
    count += fprintf(f, "\\");
    for (int i = 2; i >= 0; i--) {
        int value = ((unsigned char) c >> (i * 3)) & 7;
        count += fprintf(f, "%d", value);
    }
    return count;
}

int fprintf_escaped_string_literal(void *f, StringLiteral* sl, int for_assembly) {
    unsigned char *data = (unsigned char *) sl->data;
    int c = 0;

    if (for_assembly)
        c += fprintf(f, "    .string \"");
    else
        c += fprintf(f, "\"");

    int data_count = sl->is_wide_char ? sl->size * 4 : sl->size;
    for (int i = 0; i < data_count; i++) {
        if (for_assembly && data[i] == 0 && i != data_count - 1) {
            // Terminate the string & start a new one
            c += fprintf(f, "\"\n");
            c += fprintf(f, "    .string \"");

        } else if (!for_assembly || (for_assembly && data[i]))
            c += fprintf_escaped_char(f, data[i]);
    }
    c += fprintf(f, "\"");

    if (for_assembly) c += fprintf(f, "\n");

    return c;
}

// Add an instruction after ir and return ir of the new instruction
Tac *insert_target_instruction(Tac *ir, int operation, Value *dst, Value *src1, Value *src2, char *target_template) {
    Tac *tac = new_instruction(operation);
    tac->operation.id = operation;
    tac->dst = dst;
    tac->src1 = src1;
    tac->src2 = src2;
    tac->target_template = target_template;

    return insert_tac_after(ir, tac);
}

Value *new_preg_value(int preg) {
    Value *v = new_value();
    v->preg = preg;
    return v;
}

// Remove all possible IR_NOP instructions
void remove_nops(Function *function) {
    for (Tac *tac = function->ir; tac; tac = tac->next) {
        if (tac->operation.id != IR_NOP) continue;
        if (!tac->next) panic("Unexpected NOP as last instruction");
        if (!tac->prev) continue;
        if (tac->next->label) continue;

        delete_instruction(tac);
    }
}

int function_is_main(Function *function) {
    return !strcmp(function->identifier, "main");
}

int open_output_file(char *input_filename, char *output_filename) {
    if (!strcmp(output_filename, "-"))
        output_file = stdout;
    else {
        // Open output file for writing
        output_file = fopen(output_filename, "w");
        if (output_file == 0) {
            perror(output_filename);
            exit(1);
        }
    }

    fprintf(output_file, "    .file   \"%s\"\n", input_filename);

    // Indicate this object file doesn't need an executable stack
    // See https://man7.org/linux/man-pages/man5/elf.5.html
    fprintf(output_file, "    .section .note.GNU-stack,\"\",@progbits\n\n");
}

static void output_object_symbol(Symbol *symbol) {
    if (symbol->linkage == LINKAGE_INTERNAL && !symbol->initializers) {
        if (elf_section != SEC_TEXT) { fprintf(output_file, "    .text\n"); elf_section = SEC_TEXT; }
        fprintf(output_file, "    .local  %s\n", symbol->global_identifier);
    }

    if ((symbol->linkage == LINKAGE_INTERNAL || symbol->linkage == LINKAGE_EXTERNAL) && symbol->definition_status == DEFINITION_STATUS_TENTATIVE) {
        if (elf_section != SEC_TEXT) { fprintf(output_file, "    .text\n"); elf_section = SEC_TEXT; }

        // opt_enable_common_symbols applies to symbols with external linkage.
        // For symbols with internal linkage, a .comm section will do just fine.
        if (symbol->linkage == LINKAGE_INTERNAL || opt_enable_common_symbols) {
            fprintf(output_file, "    .comm   %s,%d,%d\n",
                symbol->global_identifier,
                get_type_size(symbol->type),
                get_type_alignment(symbol->type));
        }
        else {
            int size = get_type_size(symbol->type);

            if (symbol->linkage == LINKAGE_EXTERNAL)
                fprintf(output_file, "    .globl   %s\n", symbol->global_identifier);

            if (elf_section != SEC_BSS) { fprintf(output_file, "    .bss\n"); elf_section = SEC_BSS; }

            fprintf(output_file, "    .align   %d\n", get_type_alignment(symbol->type));
            fprintf(output_file, "    .type    %s, @object\n", symbol->global_identifier);
            fprintf(output_file, "    .size    %s, %d\n", symbol->global_identifier, size);
            fprintf(output_file, "%s:\n", symbol->global_identifier);
            fprintf(output_file, "    .zero    %d\n", size);
        }
    }

    else if (symbol->definition_status == DEFINITION_STATUS_DEFINED) {
        output_defined_object_symbol(symbol);
    }
}

void output_object_symbols(void) {
    // Output symbols
    elf_section = SEC_NONE;
    for (int i = 0; i < global_scope->symbol_list->length; i++) {
        Symbol *symbol = global_scope->symbol_list->elements[i];
        if (!symbol->scope->parent && symbol->type->type != TYPE_FUNCTION && symbol->type->type != TYPE_TYPEDEF && !symbol->is_enum_value)
            output_object_symbol(symbol);
    }

    // Output static local symbols
    for (int i = 0; i < global_scope->symbol_list->length; i++) {
        Symbol *symbol = global_scope->symbol_list->elements[i];
        if (symbol->type->type == TYPE_FUNCTION && symbol->function->is_defined) {
            Function *function = symbol->function;
            for (int j = 0; j < function->static_symbols->length; j++)
                output_object_symbol(function->static_symbols->elements[j]);
        }
        symbol++;
    }
}

// Allocate and make an array of callee saved physical registers used by the function.
int *make_saved_registers(Function *function, int preg_class) {
    // Make a sparse array of booleans
    int *saved_registers = wcalloc(sizeof(int), physical_register_count);

    Tac *tac = function->ir;

    while (tac) {
        if (tac->dst  && tac->dst ->preg != -1 && tac->dst ->preg_class == preg_class && callee_saved_registers[tac->dst ->preg]) saved_registers[tac->dst ->preg] = 1;
        if (tac->src1 && tac->src1->preg != -1 && tac->src1->preg_class == preg_class && callee_saved_registers[tac->src1->preg]) saved_registers[tac->src1->preg] = 1;
        if (tac->src2 && tac->src2->preg != -1 && tac->src2->preg_class == preg_class && callee_saved_registers[tac->src2->preg]) saved_registers[tac->src2->preg] = 1;
        tac = tac->next;
    }

    // Make a -1 terminated list of all the saved registers.
    // An extra padding has been added for convenience, since some archs require
    // two pushes at a time.
    const int padding = 16;
    int *saved_registers_list = wmalloc(sizeof(int) * (physical_register_count + padding));
    for (int i = 0; i < physical_register_count + padding; i++) saved_registers_list[i] = -1;

    int saved_register_count = 0;
    for (int i = 0; i < physical_register_count; i++)
        if (saved_registers[i]) saved_registers_list[saved_register_count++] = i;

    wfree(saved_registers);

    return saved_registers_list;
}

static void process_stack_offset(Value *value, int *stack_alignments, int *stack_sizes) {
    if (value && value->stack_index < 0) {
        // When in doubt, pick the biggest of size & alignment. There can be
        // mismatches during struct/union manipulation, where a bit of the stack
        // is loaded/saved into up 8 bytes.

        int value_alignment = get_type_alignment(value->type);
        if (value_alignment > stack_alignments[-value->stack_index]) stack_alignments[-value->stack_index] = value_alignment;
        int value_size = get_type_size(value->type);
        if (value_size > stack_sizes[-value->stack_index]) stack_sizes[-value->stack_index] = value_size;
    }
}

// Allocate stack offsets for variables on the stack (stack_index < 0). Go backwards
// in alignment, allocating the ones with the largest alignment first, in order of
// stack_index.
void make_stack_offsets(Function *function) {
    int count = function->stack_register_count;

    if (!count) {
        // Nothing is on the stack
        function->stack_size = 0;
        return;
    }

    // Determine size & alignments for all variables on the stack
    int *stack_alignments = wcalloc((count + 1), sizeof(int));
    int *stack_sizes = wcalloc((count + 1), sizeof(int));

    for (Tac *tac = function->ir; tac; tac = tac->next) {
        if (tac->dst)  process_stack_offset(tac->dst,  stack_alignments, stack_sizes);
        if (tac->src1) process_stack_offset(tac->src1, stack_alignments, stack_sizes);
        if (tac->src2) process_stack_offset(tac->src2, stack_alignments, stack_sizes);
    }

    // Determine stack offsets
    int *stack_offsets = wmalloc((count + 1) * sizeof(int));
    int offset = 0;
    int total_size = 0;
    for (int size = 4; size >= 0; size--) {
        int wanted_alignment = 1 << size;
        for (int i = 1; i <= count; i++) {
            int alignment = stack_alignments[i];
            int object_size = stack_sizes[i];
            if (wanted_alignment != alignment) continue;
            offset += object_size;
            stack_offsets[i] = offset;
            total_size += object_size;
        }
    }

    if (debug_stack_frame_layout) {
        printf("Stack frame for %s:\n", function->identifier);
        printf("Stack index   Offset   Size     Alignment\n");
        printf("--------------------------------------------\n");
        for (int i = 1; i <= count; i++) {
            printf("%-4d          %-8d %-8d %-4d\n", -i, stack_offsets[i], stack_sizes[i], stack_alignments[i]);
        }
        printf("\n");
    }

    // Align on 8 bytes, this is required by the function call stack alignment code.
    total_size = (total_size + (total_function_stack_size_alignment - 1)) & ~(total_function_stack_size_alignment - 1);
    function->stack_size = total_size;

    // Assign stack_offsets
    for (Tac *tac = function->ir; tac; tac = tac->next) {
        // Special case for functions with a va_list. A src1 with stack_index OVERFLOW_AREA_ADDRESS_MAGIC_STACK_INDEX
        // needs to be reassigned with address where the pushed varargs start
        if (tac->src1 && tac->src1->stack_index == OVERFLOW_AREA_ADDRESS_MAGIC_STACK_INDEX) {
            tac->src1->stack_index = 2 + (function->fpa->size >> 3);
        }
        else
            if (tac->src1 && tac->src1->stack_index < 0) tac->src1->stack_offset = stack_offsets[-tac->src1->stack_index];

        if (tac ->dst && tac ->dst->stack_index < 0) tac ->dst->stack_offset = stack_offsets[-tac ->dst->stack_index];
        if (tac->src2 && tac->src2->stack_index < 0) tac->src2->stack_offset = stack_offsets[-tac->src2->stack_index];
    }

    wfree(stack_alignments);
    wfree(stack_sizes);
    wfree(stack_offsets);
}

void check_floating_point_literal_max(void) {
    if (floating_point_literal_count >= MAX_FLOATING_POINT_LITERALS) panic("Exceeded max floating point literals %d", MAX_FLOATING_POINT_LITERALS);
}

int add_float_literal(Value *value) {
    check_floating_point_literal_max();
    floating_point_literals[floating_point_literal_count].f = value->fp_value;
    floating_point_literals[floating_point_literal_count].type = TYPE_FLOAT;
    return floating_point_literal_count++;
}

int add_double_literal(Value *value) {
    check_floating_point_literal_max();
    floating_point_literals[floating_point_literal_count].d = value->fp_value;
    floating_point_literals[floating_point_literal_count].type = TYPE_DOUBLE;
    return floating_point_literal_count++;
}

int add_long_double_literal(Value *value) {
    check_floating_point_literal_max();
    floating_point_literals[floating_point_literal_count].ld = value->fp_value;
    floating_point_literals[floating_point_literal_count].type = TYPE_LONG_DOUBLE;
    return floating_point_literal_count++;
}

void output_floating_point_literals(void) {
    // Output floating point literals
    if (floating_point_literal_count > 0) {
        for (int i = 0; i < floating_point_literal_count; i++) {
            // The zero and & is to be compatible with gcc
            fprintf(output_file, ".LFP%d:\n", i);

            if (floating_point_literals[i].type == TYPE_FLOAT) {
                float fl = floating_point_literals[i].f;
                fprintf(output_file, "    .long   %d\n", *((int *) &fl));
            }
            else if (floating_point_literals[i].type == TYPE_DOUBLE) {
                double d = floating_point_literals[i].d;
                fprintf(output_file, "    .long   %d\n", *((int *) &d));
                fprintf(output_file, "    .long   %d\n", *((int *) &d + 1));
            }
            else {
                long double ld = floating_point_literals[i].ld;
                fprintf(output_file, "    .long   %d\n", ((int *) &ld)[0]);
                fprintf(output_file, "    .long   %d\n", ((int *) &ld)[1]);
                fprintf(output_file, "    .long   %d\n", ((int *) &ld)[2] & 0xffff);
                fprintf(output_file, "    .long   0\n");
            }
        }
    }
}

void init_codegen(void) {
    floating_point_literals = wmalloc(sizeof(FloatingPointLiteral) * MAX_FLOATING_POINT_LITERALS);
    floating_point_literal_count = 0;

    debug_strings = new_strmap();
    debug_string_counter = 0;

    last_outputted_filename_id = 0;
    last_outputted_filename_line_number = 0;

    allocated_strings = new_list(128);
}

void free_codegen(void) {
    wfree(floating_point_literals);

    strmap_foreach(debug_strings, it) wfree(strmap_iterator_key(&it));
    free_strmap(debug_strings);

    for (int i = 0; i < allocated_strings->length; i++) wfree(allocated_strings->elements[i]);
    free_list(allocated_strings);
}
