#include <stdlib.h>

#include "wc4.h"

struct stack *new_stack() {
    struct stack *result;

    result = malloc(sizeof(struct stack));
    result->elements = malloc(MAX_STACK_SIZE * sizeof(int));
    result->pos = MAX_STACK_SIZE;

    return result;
}

int stack_top(struct stack *s) {
    if (s->pos == MAX_STACK_SIZE) panic("Attempt to read top of an empty stack");
    return s->elements[s->pos];
}

void push_onto_stack(struct stack *s, int v) {
    if (s->pos == 0) panic("Ran out of stack space");
    s->pos--;
    s->elements[s->pos] = v;
}

int pop_from_stack(struct stack *s) {
    if (s->pos == MAX_STACK_SIZE) panic("Attempt to pop off of an empty stack");
    return s->elements[s->pos++];
}
