#include <stdlib.h>

#include "wcc.h"

void free_circular_linked_list(CircularLinkedList *cll) {
    if (!cll) return;

    CircularLinkedList *head = cll->next;
    CircularLinkedList *l = head;
    do {
        CircularLinkedList *next = l->next;
        free(l);
        l = next;
    } while (l != head);
}
