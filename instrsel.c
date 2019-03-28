#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "wc4.h"

typedef struct vreg_i_graph {
    int igraph_id;
    int count;
} VregIGraph;

typedef struct i_graph_node {
    Tac *tac;
    Value *value;
} IGraphNode;

typedef struct i_graph {
    IGraphNode *nodes;
    Graph *graph;
    int node_count;
} IGraph;

enum {
    MAX_INSTRUCTION_GRAPH_EDGE_COUNT = 32,
    NOISY_MESS = 0,
};

void recursive_dump_igraph(IGraph *ig, int node, int indent) {
    int i, operation;
    Graph *g;
    IGraphNode *ign;
    GraphEdge *e;
    Tac *t;

    ign = &(ig->nodes[node]);

    for (i = 0; i < indent; i++) printf("  ");

    if (ign->tac) {
        operation = ign->tac->operation;
        if (operation == IR_ASSIGN) printf("=\n");
        else if (operation == IR_ADD) printf("+\n");
        else if (operation == IR_SUB) printf("-\n");
        else if (operation == IR_MUL) printf("*\n");
        else if (operation == IR_DIV) printf("/\n");
        else if (operation == IR_INDIRECT) printf("indirect\n");
        else if (operation == IR_LOAD_CONSTANT) printf("load constant\n");
        else if (operation == IR_NOP) printf("noop\n");
        else printf("Operation %d\n", operation);
    }
    else {
        print_value(stdout, ign->value, 0);
        printf("\n");
    }

    e = ig->graph->nodes[node].succ;
    while (e) {
        recursive_dump_igraph(ig, e->to->id, indent + 1);
        e = e->next_succ;
    }
}

void dump_igraph(IGraph *ig) {
    if (ig->node_count == 0) printf("Empty igraph\n");
    recursive_dump_igraph(ig, 0, 0);
}

void copy_inode(IGraphNode *src, IGraphNode *dst) {
    dst->tac = src->tac;
    dst->value = src->value;
}

// Merge g2 into g1. The merge point is vreg
IGraph *merge_igraphs(IGraph *g1, IGraph *g2, int vreg) {
    int i, j, operation, node_count, d, join_from, join_to, from, to, replacement_vreg;
    IGraph *g;
    IGraphNode *inodes, *g1_inodes, *in, *inodes2, *in2;
    GraphNode *n, *n2;
    GraphEdge *e, *e2;
    Value *v;

    if (NOISY_MESS) {
        printf("g1:\n");
        dump_igraph(g1);
        printf("\ng2:\n");
        dump_igraph(g2);
        printf("\n");
    }

    if (g1->node_count == 2) {
        operation = g1->nodes[0].tac->operation;
        if (operation == IR_ASSIGN || operation == IR_LOAD_VARIABLE) {
            if (NOISY_MESS) {
                printf("\nreusing g2 for g:\n");
                dump_igraph(g2);
            }

            return g2;
        }
    }

    if (g2->node_count == 2) {
        operation = g2->nodes[0].tac->operation;
        if (operation == IR_ASSIGN || operation == IR_LOAD_VARIABLE) {
            replacement_vreg = g2->nodes[1].value->vreg;
            for (i = 0; i < g1->node_count; i++) {
                in = &(g1->nodes[i]);
                if (in->value && in->value->vreg == vreg) {
                    v = dup_value(in->value); // For no side effects
                    in->value = v;
                    in->value->vreg = replacement_vreg;

                    if (NOISY_MESS) {
                        printf("\nreusing tweaked g1 for g:\n");
                        dump_igraph(g1);
                    }

                    return g1;
                }
            }
            panic("Should never get here");

            exit(1);
            return g1;
        }
    }

    g = malloc(sizeof(IGraph));
    memset(g, 0, sizeof(IGraph));

    node_count = g1->node_count + g2->node_count - 1;
    inodes = malloc(node_count * sizeof(IGraphNode));
    memset(inodes, 0, node_count * sizeof(IGraphNode));

    g->nodes = inodes;
    g->graph = new_graph(node_count, MAX_INSTRUCTION_GRAPH_EDGE_COUNT);
    g->node_count = node_count;
    join_from = -1;
    join_to = -1;

    if (g1->node_count == 0) panic("Unexpectedly got 0 g1->node_count");

    if (NOISY_MESS) printf("g1->node_count=%d\n", g1->node_count);
    if (NOISY_MESS) printf("g2->node_count=%d\n", g2->node_count);

    for (i = 0; i < g1->node_count; i++) {
        if (NOISY_MESS) printf("Copying g1 %d to %d\n", i, i);
        copy_inode(&(g1->nodes[i]), &(inodes[i]));

        g1_inodes = g1->nodes;
        n = &(g1->graph->nodes[i]);

        e = n->succ;

        while (e) {
            in = &(g1_inodes[e->to->id]);
            if (in->value && in->value->vreg == vreg) {
                join_from = e->from->id;
                join_to = e->to->id;
                if (NOISY_MESS) printf("Adding join edge %d -> %d\n", join_from, join_to);
                add_graph_edge(g->graph, join_from, join_to);
            }
            else {
                if (NOISY_MESS) printf("Adding g1 edge %d -> %d\n", e->from->id, e->to->id);
                add_graph_edge(g->graph, e->from->id, e->to->id);
            }

            e = e->next_succ;
        }
    }

    if (join_from == -1 || join_to == -1) panic("Attempt to join two trees without a join node");

    if (NOISY_MESS) printf("\n");
    for (i = 0; i < g2->node_count; i++) {
        d = (i == 0) ? join_to : g1->node_count + i - 1;
        copy_inode(&(g2->nodes[i]), &(inodes[d]));

        n2 = &(g2->graph->nodes[i]);
        e = n2->succ;

        while (e) {
            from = e->from->id;
            to = e->to->id;

            if (from == 0)
                from = join_to;
            else
                from += g1->node_count - 1;

            to += g1->node_count - 1;

            if (NOISY_MESS) printf("Adding g2 edge %d -> %d\n", from, to);
            add_graph_edge(g->graph, from, to);
            e = e->next_succ;
        }
    }

    if (NOISY_MESS) {
        printf("\ng:\n");
        dump_igraph(g);
    }

    return g;
}

void experimental_instruction_selection(Function *function) {
    int instr_count, i, j, node_count, vreg_count;
    Tac *tac;
    IGraph *igraphs, *ig;
    IGraphNode *nodes;
    Graph *graph;
    VregIGraph* vreg_igraphs;
    int dst, src1, src2, g1_igraph_id;
    Value *v;

    do_oar1(function);
    do_oar2(function);
    do_oar3(function);

    if (NOISY_MESS) print_intermediate_representation(function, 0);

    vreg_count = function->vreg_count;

    // TODO loop over blocks
    instr_count = 0;
    tac = function->ir;
    while (tac) {
        instr_count++;
        tac = tac->next;
    }

    igraphs = malloc(instr_count * sizeof(IGraph));

    i = 0;
    tac = function->ir;
    while (tac) {
        node_count = 1;
        if (tac->src1) node_count++;
        if (tac->src2) node_count++;

        nodes = malloc(node_count * sizeof(IGraphNode));
        memset(nodes, 0, node_count * sizeof(IGraphNode));

        graph = new_graph(node_count, MAX_INSTRUCTION_GRAPH_EDGE_COUNT);

        nodes[0].tac = tac;
        node_count = 1;
        if (tac->src1) {nodes[node_count].value = tac->src1; add_graph_edge(graph, 0, node_count); node_count++;}
        if (tac->src2) {nodes[node_count].value = tac->src2; add_graph_edge(graph, 0, node_count); node_count++;}

        igraphs[i].graph = graph;
        igraphs[i].nodes = nodes;
        igraphs[i].node_count = node_count;

        tac = tac->next;
        i++;
    }

    vreg_igraphs = malloc((vreg_count + 1) * sizeof(VregIGraph));
    memset(vreg_igraphs, 0, (vreg_count + 1) * sizeof(VregIGraph));

    tac = function->ir;
    i = 0;
    while (tac->next) { tac = tac->next; i++; }

    while (tac) {
        // TODO one block at a time
        // TODO init vreg_igraphs with liveout

        dst = src1 = src2 = 0;

        if (tac->dst  && tac->dst ->vreg) dst  = tac->dst ->vreg;
        if (tac->src1 && tac->src1->vreg) src1 = tac->src1->vreg;
        if (tac->src2 && tac->src2->vreg) src2 = tac->src2->vreg;

        if (src1) {
            vreg_igraphs[src1].count++;
            vreg_igraphs[src1].igraph_id = i;
        }

        if (src2) {
            vreg_igraphs[src2].count++;
            vreg_igraphs[src2].igraph_id = i;
        }

        if (dst && (src1 && dst == src1) || (src2 && dst == src2)) {
            print_instruction(stdout, tac);
            panic("Illegal assignment of src1/src2 to dst");
        }

        if (src1 && src2 && src1 == src2) {
            print_instruction(stdout, tac);
            panic("src1 == src2 not handled");
        }

        if (vreg_igraphs[dst].count == 1) {
            // TODO free memory

            g1_igraph_id = vreg_igraphs[dst].igraph_id;
            if (NOISY_MESS) printf("\nMerging dst=%d src1=%d src2=%d ", dst, src1, src2);
            if (NOISY_MESS) printf("in locs %d and %d on vreg=%d\n----------------------------------------------------------\n", g1_igraph_id, i, dst);
            ig = merge_igraphs(&(igraphs[g1_igraph_id]), &(igraphs[i]), dst);

            igraphs[g1_igraph_id].nodes = ig->nodes;
            igraphs[g1_igraph_id].graph = ig->graph;
            igraphs[g1_igraph_id].node_count = ig->node_count;


            igraphs[i].node_count = 0; // Nuke it, should not be needed, but it triggers a panic in case of a bug

            if (src1) vreg_igraphs[src1].igraph_id = g1_igraph_id;
            if (src2) vreg_igraphs[src2].igraph_id = g1_igraph_id;
        }

        if (dst) vreg_igraphs[dst].count = 0;

        i--;
        tac = tac->prev;
    }

    if (NOISY_MESS) printf("\n=================================\n");

    for (i = 0; i < instr_count; i++) {
        if (igraphs[i].node_count) {
            tac = igraphs[i].nodes[0].tac;
            if (tac->operation != IR_NOP) {
                v = igraphs[i].nodes[0].tac->dst;
                if (v) {
                    print_value(stdout, v, 0);
                    printf(" = \n");
                }
                dump_igraph(&(igraphs[i]));
            }
        }
    }
}
