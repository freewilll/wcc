#include <stdio.h>

#include "wc4.h"

Tac *i(int label, int operation, Value *dst, Value *src1, Value *src2) {
    Tac *tac;

    tac = add_instruction(operation, dst, src1, src2);
    tac->label = label;
    return tac;
}

Value *v(int vreg) {
    Value *v;

    v = new_value();
    v->type = TYPE_INT;
    v->vreg = vreg;

    return v;
}

Value *l(int label) {
    Value *v;

    v = new_value();
    v->label = label;

    return v;
}

Value *c(int value) {
    return new_constant(TYPE_INT, value);
}

Value *s(int string_literal_index) {
    Value *v;

    v = new_value();
    v->type = TYPE_CHAR + TYPE_PTR;
    v->string_literal_index = string_literal_index;
    v->is_string_literal = 1;
    asprintf(&(string_literals[string_literal_index]), "Test SL %d", string_literal_index);

    return v;
}
