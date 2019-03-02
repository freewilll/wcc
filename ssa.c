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
    function->function_edges = edges;
}
