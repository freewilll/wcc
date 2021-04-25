#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "wcc.h"

void assert(int expected, int actual) {
    if (expected != actual) {
        printf("Expected %d, got %d\n", expected, actual);
        exit(1);
    }
}

int main() {
    GraphNode *n;
    Graph *g;

    int i;

    g = new_graph(10, 0);
    assert(10, g->node_count);
    assert(MAX_GRAPH_EDGE_COUNT, g->max_edge_count);

    g = new_graph(10, 20);
    assert(10, g->node_count);
    assert(20, g->max_edge_count);

    g = new_graph(3, 4);
    add_graph_edge(g, 0, 1);
    add_graph_edge(g, 0, 2);
    add_graph_edge(g, 1, 2);
    add_graph_edge(g, 2, 1);

    // Nodes:
    // n0  e0
    // n1  e2   e0
    // n2  e3   e1

    assert(0, g->nodes[0].succ->id); assert(0, (long) g->nodes[0].pred);
    assert(2, g->nodes[1].succ->id); assert(0, g->nodes[1].pred->id);
    assert(3, g->nodes[2].succ->id); assert(1, g->nodes[2].pred->id);

    assert(0, g->edges[0].from->id); assert(1, g->edges[0].to->id);
    assert(0, g->edges[1].from->id); assert(2, g->edges[1].to->id);
    assert(1, g->edges[2].from->id); assert(2, g->edges[2].to->id);
    assert(2, g->edges[3].from->id); assert(1, g->edges[3].to->id);

    // Edges:
    // e0   n0   n1   e1   e3
    // e1   n0   n2        e2
    // e2   n1   n2
    // e3   n2   n1

    assert(1,        g->edges[0].next_succ->id); assert(3,        g->edges[0].next_pred->id);
    assert(0, (long) g->edges[1].next_succ);     assert(2, (long) g->edges[1].next_pred->id);
    assert(0, (long) g->edges[2].next_succ);     assert(0, (long) g->edges[2].next_pred);
    assert(0, (long) g->edges[3].next_succ);     assert(0, (long) g->edges[3].next_pred);
}
