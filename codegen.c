#include "wcc.h"

FloatingPointLiteral *floating_point_literals; // Each floating point literal has an index in this array
int floating_point_literal_count;              // Amount of floating point literals

StrMap *debug_strings;                      // Map of debug strings to identifiers
int debug_string_counter;                   // Counter to uniquely identify a debug string
int last_outputted_filename_id;             // Keep track of last printed .loc filename id
int last_outputted_filename_line_number;    // Keep track of last printed .loc line number

List *allocated_strings;

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
