#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "wc4.h"

void make_control_flow_graph(struct symbol *function) {
    struct three_address_code *tac, **flat_tac;
    int i, j, k, ir_len, block_count, edge_count, label;
    struct block *blocks;
    struct edge *edges;

    block_count = 0;

    blocks = malloc(MAX_BLOCKS * sizeof(struct block));
    memset(blocks, 0, MAX_BLOCKS * sizeof(struct block));
    blocks[0].start = 0;
    block_count = 1;

    i = 0;
    tac = function->function_ir;
    while (tac) {
        if (tac->label) {
            blocks[block_count - 1].end = i - 1;
            blocks[block_count++].start = i;
        }
        i++;
        tac = tac->next;
    }
    ir_len = i;
    blocks[block_count - 1].end = ir_len - 1;

    flat_tac = malloc(ir_len * sizeof(struct three_address_code *));
    tac = function->function_ir;
    for (i = 0; i < ir_len; i++) {
        flat_tac[i] = tac;
        tac = tac->next;
    }

    edges = malloc(MAX_BLOCK_EDGES * sizeof(struct edge));
    memset(edges, 0, MAX_BLOCK_EDGES * sizeof(struct edge));
    edge_count = 0;
    for (i = 0; i < block_count; i++) {
        for (j = blocks[i].start; j <= blocks[i].end; j++) {
            tac = flat_tac[j];

            if (tac->operation == IR_JMP || tac->operation == IR_JZ || tac->operation == IR_JNZ) {
                label = tac->operation == IR_JMP ? tac->src1->label : tac->src2->label;
                edges[edge_count].from = i;
                for (k = 0; k < block_count; k++)
                    if (flat_tac[blocks[k].start]->label == label) edges[edge_count].to = k;
                edge_count++;
            }
        }
    }

    function->function_blocks = blocks;
    function->function_block_count = block_count;
    function->function_edges = edges;
    function->function_edge_count = edge_count;
}

// Algorithm from page 503 of Engineering a compiler
void make_block_dominance(struct symbol *function) {
    struct block *blocks;
    struct edge *edges;
    int i, j, block_count, edge_count, changed, got_predecessors;
    struct intset **dom, *is, *pred_intersections;

    blocks = function->function_blocks;
    block_count = function->function_block_count;
    edges = function->function_edges;
    edge_count = function->function_edge_count;

    dom = malloc(block_count * sizeof(struct intset));
    memset(dom, 0, block_count * sizeof(struct intset));

    // dom[0] = {0}
    dom[0] = new_intset();
    add_to_set(dom[0], 0);

    // dom[1 to n] = {0,1,2,..., n}, i.e. the set of all nodes
    for (i = 1; i < block_count; i++) {
        dom[i] = new_intset();
        for (j = 0; j < block_count; j++) add_to_set(dom[i], j);
    }

    // Recalculate dom by looking at each node's predecessors until nothing changes
    // Dom(n) = {n} union (intersection of all predecessor's doms)
    changed = 1;
    while (changed) {
        changed = 0;

        for (i = 0; i < block_count; i++) {
            pred_intersections = new_intset();
            for (j = 0; j < block_count; j++) add_to_set(pred_intersections, j);
            got_predecessors = 0;

            for (j = 0; j < edge_count; j++) {
                if (edges[j].to == i) {
                    // Got a predicate edge from edges[j].from -> i
                    pred_intersections = set_intersection(pred_intersections, dom[edges[j].from]);
                    got_predecessors = 1;
                }
            }

            if (!got_predecessors) pred_intersections = new_intset();

            // Union with {i}
            is = new_intset();
            add_to_set(is, i);
            is = set_union(is, pred_intersections);

            // Update if changed & keep looping
            if (!set_eq(is, dom[i])) {
                dom[i] = is;
                changed = 1;
            }
        }
    }

    function->function_dominance = malloc(block_count * sizeof(struct intset));
    memset(function->function_dominance, 0, block_count * sizeof(struct intset));

    for (i = 0; i < block_count; i++) function->function_dominance[i] = dom[i];
}
