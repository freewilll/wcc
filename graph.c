#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "wcc.h"

static void allocate_graph_storage(Graph *g) {
    g->nodes = malloc(g->node_count * sizeof(GraphNode));
    memset(g->nodes, 0, g->node_count * sizeof(GraphNode));

    g->edges = malloc(g->max_edge_count * sizeof(GraphEdge));
    memset(g->edges, 0, g->max_edge_count * sizeof(GraphEdge));
}

static void init_graph(Graph *g, int node_count, int edge_count) {
    g->node_count = node_count;
    g->max_edge_count = edge_count ? edge_count : MAX_GRAPH_EDGE_COUNT;
    allocate_graph_storage(g);

    for (int i = 0; i < node_count; i++) g->nodes[i].id = i;
}

Graph *new_graph(int node_count, int edge_count) {
    Graph *g = malloc(sizeof(Graph));
    memset(g, 0, sizeof(Graph));
    init_graph(g, node_count, edge_count);

    return g;
}

void free_graph(Graph *g) {
    free(g->nodes);
    free(g->edges);
    free(g);
}

void dump_graph(Graph *g) {
    printf("Nodes:\n");
    for (int i = 0; i < g->node_count; i++) {
        GraphNode *n = &g->nodes[i];
        printf("n%-3d", n->id);
        if (n->succ) printf("e%-3d ", n->succ->id); else printf("     ");
        if (n->pred) printf("e%-3d ", n->pred->id); else printf("     ");
        printf("\n");
    }

    printf("\nEdges:\n");
    for (int i = 0; i < g->edge_count; i++) {
        GraphEdge *e = &g->edges[i];
        printf("e%-3d n%-3d n%-3d ", e->id, e->from->id, e->to->id);
        if (e->next_succ) printf("e%-3d ", e->next_succ->id); else printf("     ");
        if (e->next_pred) printf("e%-3d ", e->next_pred->id); else printf("     ");
        printf("\n");
    }

    printf("\n");
}

GraphEdge *add_graph_edge(Graph *g, int from, int to) {
    if (g->edge_count == g->max_edge_count) panic("Exceeded max graph edge count %d", g->max_edge_count);
    if (to < 0 || to >= g->node_count) panic("Node %d out of range %d", to, g->node_count);
    if (from < 0 || from >= g->node_count) panic("Node %d out of range %d", from, g->node_count);

    GraphEdge *e = &g->edges[g->edge_count];
    e->id = g->edge_count;
    e->from = &g->nodes[from];
    e->to = &g->nodes[to];

    if (!g->nodes[from].succ)
        g->nodes[from].succ = e;
    else {
        GraphEdge *t = g->nodes[from].succ;
        while (t->next_succ) t = t->next_succ;
        t->next_succ = e;
    }

    if (!g->nodes[to].pred)
        g->nodes[to].pred = e;
    else {
        GraphEdge *t = g->nodes[to].pred;
        while (t->next_pred) t = t->next_pred;
        t->next_pred = e;
    }

    g->edge_count++;

    return e;
}
