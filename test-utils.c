#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
    v->type = TYPE_LONG;
    v->vreg = vreg;

    return v;
}

Value *vsz(int vreg, int type) {
    Value *v;

    v = new_value();
    v->type = type;
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
    return new_constant(TYPE_LONG, value);
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


Value *S(int stack_index) {
    Value *v;

    v = new_value();
    v->type = TYPE_LONG;
    v->stack_index = stack_index;

    return v;
}

Value *g(int index) {
    Value *v;
    Symbol *s;

    s = malloc(sizeof(Symbol));
    memset(s, 0, sizeof(Symbol));
    asprintf(&(s->identifier), "g%d", index);

    v = new_value();
    v->type = TYPE_LONG;
    v->global_symbol = s;
    v->is_lvalue = 1;

    return v;
}

Value *gsz(int index, int type) {
    Value *v;

    v = g(index);
    v->type = type;

    return v;
}
