#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "wc4.h"

void index_tac(struct three_address_code *ir) {
    struct three_address_code *tac;
    int i;

    tac = ir;
    i = 0;
    while (tac) {
        tac->index = i;
        tac = tac->next;
        i++;
    }
}

void make_control_flow_graph(struct symbol *function) {
    struct three_address_code *tac;
    int i, j, k, block_count, edge_count, label;
    struct block *blocks;
    struct edge *edges;

    block_count = 0;

    blocks = malloc(MAX_BLOCKS * sizeof(struct block));
    memset(blocks, 0, MAX_BLOCKS * sizeof(struct block));
    blocks[0].start = function->function_ir;
    block_count = 1;

    tac = function->function_ir;
    while (tac) {
        if (tac->label) {
            blocks[block_count - 1].end = tac->prev;
            blocks[block_count++].start = tac;
        }

        // Start a new block after a conditional jump.
        // Check if a label is set so that we don't get a double block
        if (tac->next && !tac->next->label && (tac->operation == IR_JZ || tac->operation == IR_JNZ)) {
            blocks[block_count - 1].end = tac;
            blocks[block_count++].start = tac->next;
        }

        tac = tac->next;
    }

    tac = function->function_ir;
    while (tac->next) tac = tac->next;
    blocks[block_count - 1].end = tac;

    edges = malloc(MAX_BLOCK_EDGES * sizeof(struct edge));
    memset(edges, 0, MAX_BLOCK_EDGES * sizeof(struct edge));
    edge_count = 0;

    for (i = 0; i < block_count; i++) {
        tac = blocks[i].start;
        while (1) {
            if (tac->operation == IR_JMP || tac->operation == IR_JZ || tac->operation == IR_JNZ) {
                label = tac->operation == IR_JMP ? tac->src1->label : tac->src2->label;
                edges[edge_count].from = i;
                for (k = 0; k < block_count; k++)
                    if (blocks[k].start->label == label) edges[edge_count].to = k;
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

            if (tac == blocks[i].end) break;
            tac = tac->next;
        }
    }

    function->function_blocks = blocks;
    function->function_block_count = block_count;
    function->function_edges = edges;
    function->function_edge_count = edge_count;

    index_tac(function->function_ir);

    if (DEBUG_SSA) {
        printf("Blocks:\n");
        for (i = 0; i < block_count; i++) printf("%d: %d -> %d\n", i, blocks[i].start->index, blocks[i].end->index);

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
    struct three_address_code *tac;

    blocks = function->function_blocks;
    block_count = function->function_block_count;

    function->function_uevar = malloc(block_count * sizeof(struct intset *));
    memset(function->function_uevar, 0, block_count * sizeof(struct intset *));
    function->function_varkill = malloc(block_count * sizeof(struct intset *));
    memset(function->function_varkill, 0, block_count * sizeof(struct intset *));

    for (i = 0; i < block_count; i++) {
        uevar = new_intset();
        varkill = new_intset();
        function->function_uevar[i] = uevar;
        function->function_varkill[i] = varkill;

        tac = blocks[i].start;
        while (1) {
            if (tac->src1 && tac->src1->vreg && !in_set(varkill, tac->src1->vreg)) add_to_set(uevar, tac->src1->vreg);
            if (tac->src2 && tac->src2->vreg && !in_set(varkill, tac->src2->vreg)) add_to_set(uevar, tac->src2->vreg);
            if (tac->dst && tac->dst->vreg) add_to_set(varkill, tac->dst->vreg);

            if (tac == blocks[i].end) break;
            tac = tac->next;
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
    struct three_address_code *tac;

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
    all_vars = new_intset();
    for (i = 0; i < block_count; i++) {
        tac = blocks[i].start;
        while (1) {
            if (tac->src1 && tac->src1->vreg) add_to_set(all_vars, tac->src1->vreg);
            if (tac->src2 && tac->src2->vreg) add_to_set(all_vars, tac->src2->vreg);
            if (tac->dst && tac->dst->vreg) add_to_set(all_vars, tac->dst->vreg);

            if (tac == blocks[i].end) break;
            tac = tac->next;
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
    struct three_address_code *tac;

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

    globals = new_intset();

    for (i = 0; i < block_count; i++) {
        varkill = new_intset();

        tac = blocks[i].start;
        while (1) {
            if (tac->src1 && tac->src1->vreg && !in_set(varkill, tac->src1->vreg)) add_to_set(globals, tac->src1->vreg);
            if (tac->src2 && tac->src2->vreg && !in_set(varkill, tac->src2->vreg)) add_to_set(globals, tac->src2->vreg);
            if (tac->dst && tac->dst->vreg) {
                add_to_set(varkill, tac->dst->vreg);
                add_to_set(var_blocks[tac->dst->vreg], i);
            }

            if (tac == blocks[i].end) break;
            tac = tac->next;
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
    int i, v, block_count, global, b, d, label;
    struct intset **phi_functions, *vars;
    struct block *blocks;
    struct three_address_code *tac, *prev;

    blocks = function->function_blocks;
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

    if (DEBUG_SSA) printf("phi functions to add:\n");
    for (b = 0; b < block_count; b++) {
        if (DEBUG_SSA) {
            printf("%d: ", b);
            print_set(phi_functions[b]);
            printf("\n");
        }

        label = blocks[b].start->label;
        blocks[b].start->label = 0;

        vars = phi_functions[b];
        for (v = MAX_INT_SET_ELEMENTS - 1; v >= 0; v--) {
            if (!vars->elements[v]) continue;

            tac = new_instruction(IR_PHI_FUNCTION);
            tac->src1 = new_value(); tac->src1->type = TYPE_LONG; tac->src1->vreg = v;
            tac->src2 = new_value(); tac->src2->type = TYPE_LONG; tac->src2->vreg = v;
            tac->dst  = new_value(); tac->dst ->type = TYPE_LONG; tac->dst-> vreg = v;

            prev = blocks[b].start->prev;
            tac->prev = prev;
            tac->next = blocks[b].start;
            prev->next = tac;
            blocks[b].start = tac;
        }

        blocks[b].start->label = label;
    }

    if (DEBUG_SSA) {
        printf("\nIR with phi functions:\n");
        print_intermediate_representation(function);
    }
}

int new_subscript(struct stack **stack, int *counters, int n) {
    int i;

    i = counters[n]++;
    push_onto_stack(stack[n], i);

    return i;
}

void print_stack_and_counters(struct stack **stack, int *counters, int vreg_count) {
    int i;

    printf("         ");
    for (i = 1; i <= vreg_count; i++) printf("%-2d ", i);
    printf("\n");
    printf("counters ");
    for (i = 1; i <= vreg_count; i++) printf("%-2d ", counters[i]);
    printf("\n");
    printf("stack    ");
    for (i = 1; i <= vreg_count; i++) {
        if (stack[i]->pos == MAX_STACK_SIZE)
            printf("   ");
        else
            printf("%-2d ", stack_top(stack[i]));
    }
    printf("\n");
}

// Algorithm on page 506 of engineering a compiler
void rename_vars(struct symbol *function, struct stack **stack, int *counters, int block_number, int vreg_count) {
    struct three_address_code *tac, *tac2, *end;
    int i, x, block_count, edge_count;
    struct block *blocks;
    struct edge *edges;
    struct block *b;
    int *idoms;

    if (DEBUG_SSA_PHI_RENUMBERING) {
        printf("\n----------------------------------------\nrename_vars\n");
        print_stack_and_counters(stack, counters, vreg_count);
        printf("\n");
    }

    blocks = function->function_blocks;
    block_count = function->function_block_count;
    edges = function->function_edges;
    edge_count = function->function_edge_count;
    idoms = function->function_idom;

    b = &blocks[block_number];

    // Rewrite phi function dsts
    if (DEBUG_SSA_PHI_RENUMBERING) printf("Rewriting phi function dsts\n");
    tac = b->start;
    while (tac->operation == IR_PHI_FUNCTION) {
        // Rewrite x as new_subscript(x)
        if (DEBUG_SSA_PHI_RENUMBERING) printf("Renaming %d ", tac->dst->vreg);
        tac->dst->ssa_subscript = new_subscript(stack, counters, tac->dst->vreg);
        if (DEBUG_SSA_PHI_RENUMBERING) printf("to %d in phi function\n", tac->dst->vreg);

        if (tac == b->end) break;
        tac = tac->next;
    }

    // Rewrite operations
    if (DEBUG_SSA_PHI_RENUMBERING) printf("Rewriting operations\n");
    tac = b->start;
    while (1) {
        if (tac->operation != IR_PHI_FUNCTION) {
            // printf("checking rewrite "); print_instruction(stdout, tac);
            if (tac->src1 && tac->src1->vreg) {
                tac->src1->ssa_subscript = stack_top(stack[tac->src1->vreg]);
                if (DEBUG_SSA_PHI_RENUMBERING)  printf("rewrote src1 %d\n", tac->src1->vreg);
            }
            if (tac->src2 && tac->src2->vreg) {
                tac->src2->ssa_subscript = stack_top(stack[tac->src2->vreg]);
                if (DEBUG_SSA_PHI_RENUMBERING)  printf("rewrote src2 %d\n", tac->src2->vreg);
            }

            if (tac->dst && tac->dst->vreg) {
                tac->dst->ssa_subscript = new_subscript(stack, counters, tac->dst->vreg);
                if (DEBUG_SSA_PHI_RENUMBERING)  printf("got new name for dst %d\n", tac->dst->vreg);
            }
        }

        if (tac == b->end) break;
        tac = tac->next;
    }

    // Rewrite phi function parameters in successors
    if (DEBUG_SSA_PHI_RENUMBERING) printf("Rewriting successor function params\n");
    for (i = 0; i < edge_count; i++) {
        if (edges[i].from != block_number) continue;
        if (DEBUG_SSA_PHI_RENUMBERING) printf("Successor %d\n", edges[i].to);
        tac = function->function_blocks[edges[i].to].start;
        end = function->function_blocks[edges[i].to].end;
        while (1) {
            if (tac->operation == IR_PHI_FUNCTION) {
                if (tac->src1->ssa_subscript == -1) {
                    tac->src1->ssa_subscript = stack_top(stack[tac->src1->vreg]);
                    if (DEBUG_SSA_PHI_RENUMBERING) printf("  rw src1 to %d\n", tac->src1->vreg);
                }
                else {
                    tac->src2->ssa_subscript = stack_top(stack[tac->src2->vreg]);
                    if (DEBUG_SSA_PHI_RENUMBERING) printf("  rw src2 to %d\n", tac->src2->vreg);
                }
            }
            if (tac == end) break;
            tac = tac->next;
        }
    }

    // Recurse down the dominator tree
    for (i = 0; i < block_count; i++) {
        if (idoms[i] == block_number) {
            if (DEBUG_SSA_PHI_RENUMBERING) printf("going into idom successor %d\n", i);
            rename_vars(function, stack, counters, i, vreg_count);
        }
    }

    // Pop contents of current block assignments off of the stack
    if (DEBUG_SSA_PHI_RENUMBERING) printf("Block done. Cleaning up stack\n");
    tac = b->start;
    while (1) {
        if (tac->dst && tac->dst->vreg) {
            pop_from_stack(stack[tac->dst->vreg]);
        }
        if (tac == b->end) break;
        tac = tac->next;
    }
}

// Algorithm on page 506 of engineering a compiler
void rename_phi_function_variables_common_prep(struct symbol *function, struct stack ***stack, int **counters, int *vreg_count) {
    int i;
    struct three_address_code *tac;

    *vreg_count = 0;
    tac = function->function_ir;
    while (tac) {
        if (tac->dst  && tac->dst ->vreg && tac->dst ->vreg > *vreg_count) *vreg_count = tac->dst ->vreg;
        if (tac->src1 && tac->src1->vreg && tac->src1->vreg > *vreg_count) *vreg_count = tac->src1->vreg;
        if (tac->src2 && tac->src2->vreg && tac->src2->vreg > *vreg_count) *vreg_count = tac->src2->vreg;
        tac = tac->next;
    }

    *counters = malloc((*vreg_count + 1) * sizeof(int));
    memset(*counters, 0, (*vreg_count + 1) * sizeof(int));

    *stack = malloc((*vreg_count + 1) * sizeof(struct stack *));
    for (i = 1; i <= *vreg_count; i++) (*stack)[i] = new_stack();
}

// Algorithm on page 506 of engineering a compiler
void rename_phi_function_variables(struct symbol *function) {
    int vreg_count;
    int *counters;
    struct stack **stack;

    rename_phi_function_variables_common_prep(function, &stack, &counters, &vreg_count);
    rename_vars(function, stack, counters, 0, vreg_count);
}

void do_ssa_experiments_common_prep(struct symbol *function) {
    make_control_flow_graph(function);
    make_block_dominance(function);
    make_block_immediate_dominators(function);
    make_block_dominance_frontiers(function);
    make_uevar_and_varkill(function);
    make_liveout(function);
    make_globals_and_var_blocks(function);
    insert_phi_functions(function);
}

void do_ssa_experiments(struct symbol *function, int rename_vars) {
    do_ssa_experiments_common_prep(function);
    if (rename_vars) rename_phi_function_variables(function);
}
