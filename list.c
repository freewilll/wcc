#include <stdlib.h>

#include "wcc.h"

void free_circular_linked_list(CircularLinkedList *cll) {
    CircularLinkedList *l = cll->next;
    while (l != cll) {
        CircularLinkedList *next = l->next;
        free(l);
        l = next;
    }
}
