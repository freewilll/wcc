#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "wc4.h"

struct three_address_code **make_flat_tac(struct three_address_code *ir) {
    struct three_address_code *tac, **result;
    int i, ir_len;

    tac = ir;
    ir_len = 0;
    while (tac) {
        tac = tac->next;
        ir_len++;
    }

    result = malloc(ir_len * sizeof(struct three_address_code *));
    tac = ir;
    i = 0;
    while (tac) {
        result[i] = tac;
        tac = tac->next;
        i++;
    }

    return result;
}

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

        // Start a new block after a conditional jump.
        // Check if a label is set so that we don't get a double block
        if (tac->next && !tac->next->label && (tac->operation == IR_JZ || tac->operation == IR_JNZ)) {
            blocks[block_count - 1].end = i;
            blocks[block_count++].start = i + 1;
        }

        i++;
        tac = tac->next;
    }
    ir_len = i;
    blocks[block_count - 1].end = ir_len - 1;

    flat_tac = make_flat_tac(function->function_ir);

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
            else if (tac->operation != IR_RETURN && tac->next && tac->next->label) {
                // For normal instructions, check if the next instruction is a label, if so it's an edge
                edges[edge_count].from = i;
                edges[edge_count].to = i + 1;
                edge_count++;
            }

            if (tac->operation == IR_JZ || tac->operation == IR_JNZ) {
                edges[edge_count].from = i;
                edges[edge_count].to = i + 1;
                edge_count++;
            }
        }
    }

    function->function_blocks = blocks;
    function->function_block_count = block_count;
    function->function_edges = edges;
    function->function_edge_count = edge_count;

    if (DEBUG_SSA) {
        printf("Blocks:\n");
        for (i = 0; i < block_count; i++) printf("%d: %d -> %d\n", i, blocks[i].start, blocks[i].end);

        printf("\nEdges:\n");
        for (i = 0; i < edge_count; i++) printf("%d: %d -> %d\n", i, edges[i].from, edges[i].to);
    }
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

    // dom[1 to n] = {0,1,2,..., n}, i.e. the set of all blocks
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
                    // Got a predecessor edge from edges[j].from -> i
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

void make_rpo(struct symbol *function, int *rpos, int *pos, int *visited, int block) {
    struct block *blocks;
    struct edge *edges;
    int i, block_count, edge_count;

    if (visited[block]) return;
    visited[block] = 1;

    blocks = function->function_blocks;
    block_count = function->function_block_count;
    edges = function->function_edges;
    edge_count = function->function_edge_count;

    for (i = 0; i < edge_count; i++) {
        if (edges[i].from == block) {
            make_rpo(function, rpos, pos, visited, edges[i].to);
        }
    }

    rpos[block] = *pos;
    (*pos)--;
}

int intersect(int *rpos, int *idoms, int i, int j) {
    int f1, f2;

    f1 = i;
    f2 = j;

    while (f1 != f2) {
        while (rpos[f1] > rpos[f2]) f1 = idoms[f1];
        while (rpos[f2] > rpos[f1]) f2 = idoms[f2];
    }

    return f1;
}

// Algorithm on page 532 of engineering a compiler
void make_block_immediate_dominators(struct symbol *function) {
    struct block *blocks;
    struct edge *edges;
    int block_count, edge_count, i, j, changed;
    int *rpos, pos, *visited, *idoms, *rpos_order, b, p, new_idom;

    blocks = function->function_blocks;
    block_count = function->function_block_count;
    edges = function->function_edges;
    edge_count = function->function_edge_count;

    rpos = malloc(block_count * sizeof(int));
    memset(rpos, 0, block_count * sizeof(int));

    visited = malloc(block_count * sizeof(int));
    memset(visited, 0, block_count * sizeof(int));

    pos = block_count - 1;
    make_rpo(function, rpos, &pos, visited, 0);

    rpos_order = malloc(block_count * sizeof(int));
    memset(rpos_order, 0, block_count * sizeof(int));
    for (i = 0; i < block_count; i++) rpos_order[rpos[i]] = i;

    idoms = malloc(block_count * sizeof(int));
    memset(idoms, 0, block_count * sizeof(int));

    for (i = 0; i < block_count; i++) idoms[i] = -1;
    idoms[0] = 0;

    changed = 1;
    while (changed) {
        changed = 0;

        for (i = 0; i < block_count; i++) {
            b = rpos_order[i];
            if (b == 0) continue;

            new_idom = -1;
            for (j = 0; j < edge_count; j++) {
                if (edges[j].to != b) continue;
                p = edges[j].from;

                if (idoms[p] != -1) {
                    if (new_idom == -1)
                        new_idom = p;
                    else
                        new_idom = intersect(rpos, idoms, p, new_idom);
                }
            }

            if (idoms[b] != new_idom) {
                idoms[b] = new_idom;
                changed = 1;
            }
        }
    }

    idoms[0] = -1;

    if (DEBUG_SSA) {
        printf("\nIdoms:\n");
        for (i = 0; i < block_count; i++) printf("%d: %d\n", i, idoms[i]);
    }

    function->function_idom = malloc(block_count * sizeof(int));
    memset(function->function_idom, 0, block_count * sizeof(int));

    for (i = 0; i < block_count; i++) function->function_idom[i] = idoms[i];
}

// Algorithm on page 499 of engineering a compiler
// Walk the dominator tree defined by the idom (immediate dominator)s.

void make_block_dominance_frontiers(struct symbol *function) {
    struct block *blocks;
    struct edge *edges;
    int block_count, edge_count, i, j;
    struct intset **df;
    int *predecessors, predecessor_count, p, runner, *idom;

    blocks = function->function_blocks;
    block_count = function->function_block_count;
    edges = function->function_edges;
    edge_count = function->function_edge_count;

    df = malloc(block_count * sizeof(struct intset));
    memset(df, 0, block_count * sizeof(struct intset));

    for (i = 0; i < block_count; i++) df[i] = new_intset();

    predecessors = malloc(block_count * sizeof(int));

    idom = function->function_idom;

    for (i = 0; i < block_count; i++) {
        predecessor_count = 0;
        for (j = 0; j < edge_count; j++) {
            if (edges[j].to == i)
                predecessors[predecessor_count++] = edges[j].from;
        }

        if (predecessor_count > 1) {
            for (j = 0; j < predecessor_count; j++) {
                p = predecessors[j];
                runner = p;
                while (runner != idom[i]) {
                    add_to_set(df[runner], i);
                    runner = idom[runner];
                }
            }
        }
    }

    function->function_dominance_frontiers = malloc(block_count * sizeof(struct intset *));
    for (i = 0; i < block_count; i++) function->function_dominance_frontiers[i] = df[i];

    if (DEBUG_SSA) {
        printf("\nDominance frontiers:\n");
        for (i = 0; i < block_count; i++) {
            printf("%d: ", i);
            print_set(df[i]);
            printf("\n");
        }
    }
}

void make_uevar_and_varkill(struct symbol *function) {
    struct block *blocks;
    int i, j, block_count;
    struct intset *uevar, *varkill;
    struct three_address_code *tac, **flat_tac;

    blocks = function->function_blocks;
    block_count = function->function_block_count;

    function->function_uevar = malloc(block_count * sizeof(struct intset *));
    memset(function->function_uevar, 0, block_count * sizeof(struct intset *));
    function->function_varkill = malloc(block_count * sizeof(struct intset *));
    memset(function->function_varkill, 0, block_count * sizeof(struct intset *));

    flat_tac = make_flat_tac(function->function_ir);

    for (i = 0; i < block_count; i++) {
        uevar = new_intset();
        varkill = new_intset();
        function->function_uevar[i] = uevar;
        function->function_varkill[i] = varkill;

        for (j = blocks[i].start; j <= blocks[i].end; j++) {
            tac = flat_tac[j];
            if (tac->src1 && tac->src1->vreg && !in_set(varkill, tac->src1->vreg)) add_to_set(uevar, tac->src1->vreg);
            if (tac->src2 && tac->src2->vreg && !in_set(varkill, tac->src2->vreg)) add_to_set(uevar, tac->src2->vreg);
            if (tac->dst && tac->dst->vreg) add_to_set(varkill, tac->dst->vreg);
        }
    }

    if (DEBUG_SSA) {
        printf("\nuevar & varkills:\n");
        for (i = 0; i < block_count; i++) {
            printf("%d: uevar=", i);
            print_set(function->function_uevar[i]);
            printf(" varkill=");
            print_set(function->function_varkill[i]);
            printf("\n");
        }
    }
}

void make_liveout(struct symbol *function) {
    struct block *blocks;
    struct edge *edges;
    int i, j, block_count, edge_count, changed, successor_block;
    struct intset *unions, **liveout, *all_vars, *inv_varkill, *is;
    struct three_address_code *tac, **flat_tac;

    blocks = function->function_blocks;
    block_count = function->function_block_count;
    edges = function->function_edges;
    edge_count = function->function_edge_count;

    function->function_liveout = malloc(block_count * sizeof(struct intset *));
    memset(function->function_liveout, 0, block_count * sizeof(struct intset *));

    // Set all liveouts to {0}
    for (i = 0; i < block_count; i++)
        function->function_liveout[i] = new_intset();

    // Set all_vars to {0, 1, 2, ... n}, i.e. the set of all vars in a block
    flat_tac = make_flat_tac(function->function_ir);

    all_vars = new_intset();
    for (i = 0; i < block_count; i++) {
        for (j = blocks[i].start; j <= blocks[i].end; j++) {
            tac = flat_tac[j];
            if (tac->src1 && tac->src1->vreg) add_to_set(all_vars, tac->src1->vreg);
            if (tac->src2 && tac->src2->vreg) add_to_set(all_vars, tac->src2->vreg);
            if (tac->dst && tac->dst->vreg) add_to_set(all_vars, tac->dst->vreg);
        }
    }

    changed = 1;
    while (changed) {
        changed = 0;

        for (i = 0; i < block_count; i++) {
            unions = new_intset();

            for (j = 0; j < edge_count; j++) {
                if (edges[j].from != i) continue;
                // Got a successor edge from i -> successor_block
                successor_block = edges[j].to;

                inv_varkill = set_difference(all_vars, function->function_varkill[successor_block]);
                is = set_intersection(function->function_liveout[successor_block], inv_varkill);
                is = set_union(is, function->function_uevar[successor_block]);
                unions = set_union(unions, is);
            }

            if (!set_eq(unions, function->function_liveout[i])) {
                function->function_liveout[i] = unions;
                changed = 1;
            }
        }
    }

    if (DEBUG_SSA) {
        printf("\nLiveouts:\n");
        for (i = 0; i < block_count; i++) {
            printf("%d: ", i);
            print_set(function->function_liveout[i]);
            printf("\n");
        }
    }
}

// Algorithm on page 501 of engineering a compiler
void make_globals_and_var_blocks(struct symbol *function) {
    struct block *blocks;
    int i, j, block_count, vreg_count;
    struct intset *globals, **var_blocks, *varkill;
    struct three_address_code *tac, **flat_tac;

    blocks = function->function_blocks;
    block_count = function->function_block_count;

    globals = new_intset();

    vreg_count = 0;
    tac = function->function_ir;
    while (tac) {
        if (tac->src1 && tac->src1->vreg && tac->src1->vreg > vreg_count) vreg_count = tac->src1->vreg;
        if (tac->src2 && tac->src2->vreg && tac->src2->vreg > vreg_count) vreg_count = tac->src2->vreg;
        if (tac->dst  && tac->dst ->vreg && tac->dst ->vreg > vreg_count) vreg_count = tac->dst ->vreg;
        tac = tac->next;
    }

    var_blocks = malloc((vreg_count + 1) * sizeof(struct intset *));
    memset(var_blocks, 0, (vreg_count + 1) * sizeof(struct intset *));
    for (i = 1; i <= vreg_count; i++) var_blocks[i] = new_intset();

    flat_tac = make_flat_tac(function->function_ir);
    globals = new_intset();

    for (i = 0; i < block_count; i++) {
        varkill = new_intset();

        for (j = blocks[i].start; j <= blocks[i].end; j++) {
            tac = flat_tac[j];
            if (tac->src1 && tac->src1->vreg && !in_set(varkill, tac->src1->vreg)) add_to_set(globals, tac->src1->vreg);
            if (tac->src2 && tac->src2->vreg && !in_set(varkill, tac->src2->vreg)) add_to_set(globals, tac->src2->vreg);
            if (tac->dst && tac->dst->vreg) {
                add_to_set(varkill, tac->dst->vreg);
                add_to_set(var_blocks[tac->dst->vreg], i);
            }
        }
    }

    function->function_var_blocks = var_blocks;

    if (DEBUG_SSA) {
        printf("\nvar write blocks:\n");
        for (i = 1; i <= vreg_count; i++) {
            printf("%d: ", i);
            print_set(var_blocks[i]);
            printf("\n");
        }

        printf("\nFunction globals: ");
        print_set(globals);
        printf("\n");
    }

    function->function_globals = globals;
}

// Algorithm on page 501 of engineering a compiler
void insert_phi_functions(struct symbol *function) {
    struct intset *globals, *global_blocks, *work_list, *df;
    int i, block_count, global, b, d;
    struct intset **phi_functions;

    block_count = function->function_block_count;
    globals = function->function_globals;

    phi_functions = malloc(block_count * sizeof(struct intset *));
    for (i = 0; i < block_count; i++) phi_functions[i] = new_intset();

    for (global = 0; global < MAX_INT_SET_ELEMENTS; global++) {
        if (!globals->elements[global]) continue;

        work_list = copy_intset(function->function_var_blocks[global]);

        while (set_len(work_list)) {
            for (b = 0; b < MAX_INT_SET_ELEMENTS; b++) {
                if (!work_list->elements[b]) continue;
                delete_from_set(work_list, b);

                df = function->function_dominance_frontiers[b];
                for (d = 0; d < MAX_INT_SET_ELEMENTS; d++) {
                    if (!df->elements[d]) continue;

                    if (!in_set(phi_functions[d], global)) {
                        add_to_set(phi_functions[d], global);
                        add_to_set(work_list, d);
                    }
                }
            }
        }
    }

    function->function_phi_functions = phi_functions;

    if (DEBUG_SSA) {
        printf("phi functions to add:\n");
        for (b = 0; b < block_count; b++) {
            printf("%d: ", b);
            print_set(phi_functions[b]);
            printf("\n");
        }
    }
}

void do_ssa_experiments(struct symbol *function) {
    make_control_flow_graph(function);
    make_block_dominance(function);
    make_block_immediate_dominators(function);
    make_block_dominance_frontiers(function);
    make_uevar_and_varkill(function);
    make_liveout(function);
    make_globals_and_var_blocks(function);
    insert_phi_functions(function);
}
