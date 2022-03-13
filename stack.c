#include <stdlib.h>

#include "wcc.h"

Stack *new_stack(void) {
    Stack *result = wmalloc(sizeof(Stack));
    result->elements = wmalloc(MAX_STACK_SIZE * sizeof(int));
    result->pos = MAX_STACK_SIZE;

    return result;
}

void free_stack(Stack *s) {
    wfree(s->elements);
    wfree(s);
}

int stack_top(Stack *s) {
    if (s->pos == MAX_STACK_SIZE) panic("Attempt to read top of an empty stack");
    return s->elements[s->pos];
}

void push_onto_stack(Stack *s, int v) {
    if (s->pos == 0) panic("Ran out of stack space");
    s->pos--;
    s->elements[s->pos] = v;
}

int pop_from_stack(Stack *s) {
    if (s->pos == MAX_STACK_SIZE) panic("Attempt to pop off of an empty stack");
    return s->elements[s->pos++];
}
