#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include "wc4.h"

// Indexes used to reference the physically reserved registers, starting at 0
int SSA_PREG_REG_RAX;
int SSA_PREG_REG_RCX;
int SSA_PREG_REG_RDX;
int SSA_PREG_REG_RSI;
int SSA_PREG_REG_RDI;
int SSA_PREG_REG_R8;
int SSA_PREG_REG_R9;

struct vreg_cost {
    int vreg;
    int cost;
};

// dst is an lvalue in a register. The difference with the regular IS_ASSIGN is
// that src1 is the destination and src2 is the src. The reason for that is
// that it makes the SSA calulation easier since both dst and src1 are values
// in registers that are read but not written to in this instruction.
void rewrite_lvalue_reg_assignments(struct function *function) {
    struct three_address_code *tac;

    tac = function->ir;
    while (tac) {
        if (tac->operation == IR_ASSIGN && tac->dst->vreg && tac->dst->is_lvalue) {
            tac->operation = IR_ASSIGN_TO_REG_LVALUE;
            tac->src2 = tac->src1;
            tac->src1 = tac->dst;
            tac->dst = 0;
        }
        tac = tac->next;
    }
}

// If any vregs have &, then they are mapped to a stack_index and not
// mapped to a vreg by the existing code. They need to be mapped
// to the new approach of using a stack_index. stack_index_map is the
// map from -stack_index => spilled_stack_index.
// The original stack_index remains, since downstream codegen code needs
// it.
void map_stack_index_to_spilled_stack_index(struct function *function) {
    int spilled_register_count;
    struct three_address_code *tac;
    int *stack_index_map;

    spilled_register_count = 0;

    stack_index_map = malloc((function->local_symbol_count + 1) * sizeof(int));
    memset(stack_index_map, -1, (function->local_symbol_count + 1) * sizeof(int));

    if (DEBUG_SSA_MAPPING_LOCAL_STACK_INDEXES) print_intermediate_representation(function, 0);

    // The -1 in stack_index assignment below is due to conventions in the old
    // register allocation code. In the legacy code, stack_index starts at -1
    // and grows downwards. In the SSA code, the stack index starts at 0
    // and grows upwards.

    tac = function->ir;
    while (tac) {
        if (tac->dst && tac->dst->stack_index < 0) {
            if (stack_index_map[-tac->dst->stack_index] == -1)
                stack_index_map[-tac->dst->stack_index] = spilled_register_count++;

            tac->dst->spilled_stack_index = stack_index_map[-tac->dst->stack_index];
            tac->dst->stack_index = -tac->dst->spilled_stack_index - 1;
        }

        if (tac->src1 && tac->src1->stack_index < 0) {
            if (stack_index_map[-tac->src1->stack_index] == -1)
                stack_index_map[-tac->src1->stack_index] = spilled_register_count++;

            tac->src1->spilled_stack_index = stack_index_map[-tac->src1->stack_index];
            tac->src1->stack_index = -tac->src1->spilled_stack_index - 1;
        }

        if (tac->src2 && tac->src2->stack_index < 0) {
            if (stack_index_map[-tac->src2->stack_index] == -1)
                stack_index_map[-tac->src2->stack_index] = spilled_register_count++;

            tac->src2->spilled_stack_index = stack_index_map[-tac->src2->stack_index];
            tac->src2->stack_index = -tac->src2->spilled_stack_index - 1;
        }

        tac = tac ->next;
    }

    function->spilled_register_count = spilled_register_count;

    if (DEBUG_SSA_MAPPING_LOCAL_STACK_INDEXES)
        printf("Spilled %d registers due to & use\n", spilled_register_count);

    if (DEBUG_SSA_MAPPING_LOCAL_STACK_INDEXES) print_intermediate_representation(function, 0);
}

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

void make_control_flow_graph(struct function *function) {
    struct three_address_code *tac;
    int i, j, k, block_count, edge_count, label;
    struct block *blocks;
    struct edge *edges;

    block_count = 0;

    blocks = malloc(MAX_BLOCKS * sizeof(struct block));
    memset(blocks, 0, MAX_BLOCKS * sizeof(struct block));
    blocks[0].start = function->ir;
    block_count = 1;

    tac = function->ir;
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

    tac = function->ir;
    while (tac->next) tac = tac->next;
    blocks[block_count - 1].end = tac;

    // Truncate blocks with JMP operations in them since the instructions
    // afterwards will never get executed. Furthermore, the instructions later
    // on will mess with the liveness analysis, leading to incorrect live
    // ranges for the code that _is_ executed, so they need to get excluded.
    for (i = 0; i < block_count; i++) {
        tac = blocks[i].start;
        while (1) {
            if (tac->operation == IR_JMP) {
                blocks[i].end = tac;
                break;
            }

            if (tac == blocks[i].end) break;
            tac = tac->next;
        }
    }

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

    function->blocks = blocks;
    function->block_count = block_count;
    function->edges = edges;
    function->edge_count = edge_count;

    index_tac(function->ir);

    if (DEBUG_SSA_CFG) {
        print_intermediate_representation(function, 0);

        printf("Blocks:\n");
        for (i = 0; i < block_count; i++) printf("%d: %d -> %d\n", i, blocks[i].start->index, blocks[i].end->index);

        printf("\nEdges:\n");
        for (i = 0; i < edge_count; i++) printf("%d: %d -> %d\n", i, edges[i].from, edges[i].to);
    }
}

// Algorithm from page 503 of Engineering a compiler
void make_block_dominance(struct function *function) {
    struct block *blocks;
    struct edge *edges;
    int i, j, block_count, edge_count, changed, got_predecessors;
    struct set **dom, *is1, *is2, *pred_intersections;

    blocks = function->blocks;
    block_count = function->block_count;
    edges = function->edges;
    edge_count = function->edge_count;

    dom = malloc(block_count * sizeof(struct set));
    memset(dom, 0, block_count * sizeof(struct set));

    // dom[0] = {0}
    dom[0] = new_set(block_count);
    add_to_set(dom[0], 0);

    // dom[1 to n] = {0,1,2,..., n}, i.e. the set of all blocks
    for (i = 1; i < block_count; i++) {
        dom[i] = new_set(block_count);
        for (j = 0; j < block_count; j++) add_to_set(dom[i], j);
    }

    is1 = new_set(block_count);
    is2 = new_set(block_count);

    // Recalculate dom by looking at each node's predecessors until nothing changes
    // Dom(n) = {n} union (intersection of all predecessor's doms)
    changed = 1;
    while (changed) {
        changed = 0;

        for (i = 0; i < block_count; i++) {
            pred_intersections = new_set(block_count);
            for (j = 0; j < block_count; j++) add_to_set(pred_intersections, j);
            got_predecessors = 0;

            for (j = 0; j < edge_count; j++) {
                if (edges[j].to == i) {
                    // Got a predecessor edge from edges[j].from -> i
                    pred_intersections = set_intersection(pred_intersections, dom[edges[j].from]);
                    got_predecessors = 1;
                }
            }

            if (!got_predecessors) pred_intersections = new_set(block_count);

            // Union with {i}
            empty_set(is1);
            add_to_set(is1, i);
            set_union_to(is2, is1, pred_intersections);

            // Update if changed & keep looping
            if (!set_eq(is2, dom[i])) {
                dom[i] = copy_set(is2);
                changed = 1;
            }
        }
    }

    free_set(is1);
    free_set(is2);

    function->dominance = malloc(block_count * sizeof(struct set));
    memset(function->dominance, 0, block_count * sizeof(struct set));

    for (i = 0; i < block_count; i++) function->dominance[i] = dom[i];
}

void make_rpo(struct function *function, int *rpos, int *pos, int *visited, int block) {
    struct block *blocks;
    struct edge *edges;
    int i, block_count, edge_count;

    if (visited[block]) return;
    visited[block] = 1;

    blocks = function->blocks;
    block_count = function->block_count;
    edges = function->edges;
    edge_count = function->edge_count;

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
void make_block_immediate_dominators(struct function *function) {
    struct block *blocks;
    struct edge *edges;
    int block_count, edge_count, i, j, changed;
    int *rpos, pos, *visited, *idoms, *rpos_order, b, p, new_idom;

    blocks = function->blocks;
    block_count = function->block_count;
    edges = function->edges;
    edge_count = function->edge_count;

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

    function->idom = malloc(block_count * sizeof(int));
    memset(function->idom, 0, block_count * sizeof(int));

    for (i = 0; i < block_count; i++) function->idom[i] = idoms[i];
}

// Algorithm on page 499 of engineering a compiler
// Walk the dominator tree defined by the idom (immediate dominator)s.

void make_block_dominance_frontiers(struct function *function) {
    struct block *blocks;
    struct edge *edges;
    int block_count, edge_count, i, j;
    struct set **df;
    int *predecessors, predecessor_count, p, runner, *idom;

    blocks = function->blocks;
    block_count = function->block_count;
    edges = function->edges;
    edge_count = function->edge_count;

    df = malloc(block_count * sizeof(struct set));
    memset(df, 0, block_count * sizeof(struct set));

    for (i = 0; i < block_count; i++) df[i] = new_set(block_count);

    predecessors = malloc(block_count * sizeof(int));

    idom = function->idom;

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

    function->dominance_frontiers = malloc(block_count * sizeof(struct set *));
    for (i = 0; i < block_count; i++) function->dominance_frontiers[i] = df[i];

    if (DEBUG_SSA) {
        printf("\nDominance frontiers:\n");
        for (i = 0; i < block_count; i++) {
            printf("%d: ", i);
            print_set(df[i]);
            printf("\n");
        }
    }
}

int make_vreg_count(struct function *function, int starting_count) {
    int vreg_count;
    struct three_address_code *tac;

    vreg_count = starting_count;
    tac = function->ir;
    while (tac) {
        if (tac->src1 && tac->src1->vreg && tac->src1->vreg > vreg_count) vreg_count = tac->src1->vreg;
        if (tac->src2 && tac->src2->vreg && tac->src2->vreg > vreg_count) vreg_count = tac->src2->vreg;
        if (tac->dst  && tac->dst ->vreg && tac->dst ->vreg > vreg_count) vreg_count = tac->dst ->vreg;
        tac = tac->next;
    }
    function->vreg_count = vreg_count;

    return vreg_count;
}

void make_uevar_and_varkill(struct function *function) {
    struct block *blocks;
    int i, j, block_count, vreg_count;
    struct set *uevar, *varkill;
    struct three_address_code *tac;

    blocks = function->blocks;
    block_count = function->block_count;

    function->uevar = malloc(block_count * sizeof(struct set *));
    memset(function->uevar, 0, block_count * sizeof(struct set *));
    function->varkill = malloc(block_count * sizeof(struct set *));
    memset(function->varkill, 0, block_count * sizeof(struct set *));

    vreg_count = function->vreg_count;

    for (i = 0; i < block_count; i++) {
        uevar = new_set(vreg_count);
        varkill = new_set(vreg_count);
        function->uevar[i] = uevar;
        function->varkill[i] = varkill;

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
            print_set(function->uevar[i]);
            printf(" varkill=");
            print_set(function->varkill[i]);
            printf("\n");
        }
    }
}

// Page 447 of Engineering a compiler
void make_liveout(struct function *function) {
    struct block *blocks;
    struct edge *edges;
    int i, j, k, vreg, block_count, edge_count, vreg_count, changed, successor_block;
    struct set *unions, **liveout, *all_vars, *inv_varkill, *is1, *is2;
    struct three_address_code *tac;

    blocks = function->blocks;
    block_count = function->block_count;
    edges = function->edges;
    edge_count = function->edge_count;

    function->liveout = malloc(block_count * sizeof(struct set *));
    memset(function->liveout, 0, block_count * sizeof(struct set *));

    vreg_count = function->vreg_count;

    // Set all liveouts to {0}
    for (i = 0; i < block_count; i++)
        function->liveout[i] = new_set(vreg_count);

    // Set all_vars to {0, 1, 2, ... n}, i.e. the set of all vars in a block
    all_vars = new_set(vreg_count);
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

    unions = new_set(vreg_count);
    inv_varkill = new_set(vreg_count);
    is1 = new_set(vreg_count);
    is2 = new_set(vreg_count);

    if (DEBUG_SSA_LIVEOUT) printf("Doing liveout on %d blocks\n", block_count);

    changed = 1;
    while (changed) {
        changed = 0;

        for (i = 0; i < block_count; i++) {
            empty_set(unions);

            for (j = 0; j < edge_count; j++) {
                if (edges[j].from != i) continue;
                // Got a successor edge from i -> successor_block
                successor_block = edges[j].to;

                for (k = 1; k <= vreg_count; k++) {
                    unions->elements[k] =
                        (function->liveout[successor_block]->elements[k] &&                             // intersection
                            (all_vars->elements[k] && !function->varkill[successor_block]->elements[k]) // difference
                        ) ||                                                                            // union
                        function->uevar[successor_block]->elements[k] ||                                // union
                        unions->elements[k];
                }
            }

            if (!set_eq(unions, function->liveout[i])) {
                function->liveout[i] = copy_set(unions);
                changed = 1;
            }
        }
    }

    free_set(unions);
    free_set(inv_varkill);
    free_set(is1);
    free_set(is2);

    if (DEBUG_SSA_LIVEOUT) {
        printf("\nLiveouts:\n");
        for (i = 0; i < block_count; i++) {
            printf("%d: ", i);
            print_set(function->liveout[i]);
            printf("\n");
        }
    }
}

// Algorithm on page 501 of engineering a compiler
void make_globals_and_var_blocks(struct function *function) {
    struct block *blocks;
    int i, j, block_count, vreg_count;
    struct set *globals, **var_blocks, *varkill;
    struct three_address_code *tac;

    blocks = function->blocks;
    block_count = function->block_count;

    vreg_count = function->vreg_count;

    var_blocks = malloc((vreg_count + 1) * sizeof(struct set *));
    memset(var_blocks, 0, (vreg_count + 1) * sizeof(struct set *));
    for (i = 1; i <= vreg_count; i++) var_blocks[i] = new_set(block_count);

    make_vreg_count(function, 0);
    globals = new_set(vreg_count);

    for (i = 0; i < block_count; i++) {
        varkill = new_set(vreg_count);

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

        free_set(varkill);
    }

    function->var_blocks = var_blocks;

    if (DEBUG_SSA) {
        printf("\nVar write blocks:\n");
        for (i = 1; i <= vreg_count; i++) {
            printf("%d: ", i);
            print_set(var_blocks[i]);
            printf("\n");
        }

        printf("\nFunction globals: ");
        print_set(globals);
        printf("\n");
    }

    function->globals = globals;
}

// Algorithm on page 501 of engineering a compiler
void insert_phi_functions(struct function *function) {
    struct set *globals, *global_blocks, *work_list, *df;
    int i, v, block_count, edge_count, vreg_count, global, b, d, label, predecessor_count;
    struct set **phi_functions, *vars;
    struct block *blocks;
    struct edge *edges;
    struct three_address_code *tac, *prev;
    struct value *phi_values;

    blocks = function->blocks;
    block_count = function->block_count;
    edges = function->edges;
    edge_count = function->edge_count;
    globals = function->globals;
    vreg_count = function->vreg_count;

    phi_functions = malloc(block_count * sizeof(struct set *));
    for (i = 0; i < block_count; i++) phi_functions[i] = new_set(vreg_count);

    for (global = 0; global <= globals->max_value; global++) {
        if (!globals->elements[global]) continue;

        work_list = copy_set(function->var_blocks[global]);

        while (set_len(work_list)) {
            for (b = 0; b <= work_list->max_value; b++) {
                if (!work_list->elements[b]) continue;
                delete_from_set(work_list, b);

                df = function->dominance_frontiers[b];
                for (d = 0; d <= df->max_value; d++) {
                    if (!df->elements[d]) continue;

                    if (!in_set(phi_functions[d], global)) {
                        add_to_set(phi_functions[d], global);
                        add_to_set(work_list, d);
                    }
                }
            }
        }
    }

    function->phi_functions = phi_functions;

    if (DEBUG_SSA_PHI_INSERTION) printf("phi functions to add:\n");
    for (b = 0; b < block_count; b++) {
        if (DEBUG_SSA_PHI_INSERTION) {
            printf("%d: ", b);
            print_set(phi_functions[b]);
            printf("\n");
        }

        label = blocks[b].start->label;
        blocks[b].start->label = 0;

        vars = phi_functions[b];
        for (v = vreg_count; v >= 0; v--) {
            if (!vars->elements[v]) continue;

            tac = new_instruction(IR_PHI_FUNCTION);
            tac->dst  = new_value();
            tac->dst ->type = TYPE_LONG;
            tac->dst-> vreg = v;

            predecessor_count = 0;
            for (i = 0; i < edge_count; i++)
                if (edges[i].to == b) predecessor_count++;

            phi_values = malloc((predecessor_count + 1) * sizeof(struct value));
            memset(phi_values, 0, (predecessor_count + 1) * sizeof(struct value));
            for (i = 0; i < predecessor_count; i++) {
                init_value(&phi_values[i]);
                phi_values[i].type = TYPE_LONG;
                phi_values[i].vreg = v;
            }
            tac->phi_values = phi_values;

            prev = blocks[b].start->prev;
            tac->prev = prev;
            tac->next = blocks[b].start;
            prev->next = tac;
            blocks[b].start = tac;
        }

        blocks[b].start->label = label;
    }

    if (DEBUG_SSA_PHI_INSERTION) {
        printf("\nIR with phi functions:\n");
        print_intermediate_representation(function, 0);
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

// If nothing is on the stack, push one. This is to deal with undefined
// variables being used.
int safe_stack_top(struct stack **stack, int *counters, int n) {

    if (stack[n]->pos == MAX_STACK_SIZE) new_subscript(stack, counters, n);
    return stack_top(stack[n]);
}

// Algorithm on page 506 of engineering a compiler
void rename_vars(struct function *function, struct stack **stack, int *counters, int block_number, int vreg_count) {
    struct three_address_code *tac, *tac2, *end;
    int i, x, block_count, edge_count;
    struct block *blocks;
    struct edge *edges;
    struct block *b;
    int *idoms;
    struct value *v;

    if (DEBUG_SSA_PHI_RENUMBERING) {
        printf("\n----------------------------------------\nrename_vars\n");
        print_stack_and_counters(stack, counters, vreg_count);
        printf("\n");
    }

    blocks = function->blocks;
    block_count = function->block_count;
    edges = function->edges;
    edge_count = function->edge_count;
    idoms = function->idom;

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
            if (tac->src1 && tac->src1->vreg) {
                tac->src1->ssa_subscript = safe_stack_top(stack, counters, tac->src1->vreg);
                if (DEBUG_SSA_PHI_RENUMBERING) printf("rewrote src1 %d\n", tac->src1->vreg);
            }

            if (tac->src2 && tac->src2->vreg) {
                tac->src2->ssa_subscript = safe_stack_top(stack, counters, tac->src2->vreg);
                if (DEBUG_SSA_PHI_RENUMBERING) printf("rewrote src2 %d\n", tac->src2->vreg);
            }

            if (tac->dst && tac->dst->vreg) {
                tac->dst->ssa_subscript = new_subscript(stack, counters, tac->dst->vreg);
                if (DEBUG_SSA_PHI_RENUMBERING) printf("got new name for dst %d\n", tac->dst->vreg);
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
        tac = function->blocks[edges[i].to].start;
        end = function->blocks[edges[i].to].end;
        while (1) {
            if (tac->operation == IR_PHI_FUNCTION) {
                v = tac->phi_values;
                while (v->type) {
                    if (v->ssa_subscript == -1) {
                        v->ssa_subscript = safe_stack_top(stack, counters, v->vreg);
                        if (DEBUG_SSA_PHI_RENUMBERING) printf("  rewrote arg to %d\n", v->vreg);
                        break;
                    }
                    v++;
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
void rename_phi_function_variables(struct function *function) {
    int i, vreg_count, *counters;
    struct stack **stack;
    struct three_address_code *tac;

    vreg_count = 0;
    tac = function->ir;
    while (tac) {
        if (tac->dst  && tac->dst ->vreg && tac->dst ->vreg > vreg_count) vreg_count = tac->dst ->vreg;
        if (tac->src1 && tac->src1->vreg && tac->src1->vreg > vreg_count) vreg_count = tac->src1->vreg;
        if (tac->src2 && tac->src2->vreg && tac->src2->vreg > vreg_count) vreg_count = tac->src2->vreg;
        tac = tac->next;
    }

    counters = malloc((vreg_count + 1) * sizeof(int));
    memset(counters, 0, (vreg_count + 1) * sizeof(int));

    stack = malloc((vreg_count + 1) * sizeof(struct stack *));
    for (i = 1; i <= vreg_count; i++) stack[i] = new_stack();

    rename_vars(function, stack, counters, 0, vreg_count);

    if (DEBUG_SSA_PHI_RENUMBERING) print_intermediate_representation(function, 0);
}

// Page 696 engineering a compiler
// To build live ranges from ssa form, the allocator uses the disjoint-set union- find algorithm.
void make_live_ranges(struct function *function) {
    int i, j, live_range_count, vreg_count, ssa_subscript_count, max_ssa_var, block_count;
    int dst, src1, src2;
    struct three_address_code *tac, *prev;
    int *map, first;
    struct set *ssa_vars, **live_ranges;
    struct set *dst_set, *src1_set, *src2_set, *s;
    int dst_set_index, src1_set_index, src2_set_index;
    struct block *blocks;
    struct value *v;
    int *src_ssa_vars, *src_set_indexes;
    int value_count, set_index;

    live_range_reserved_pregs_offset = RESERVED_PHYSICAL_REGISTER_COUNT; // See the list at the top of the file

    if (DEBUG_SSA_LIVE_RANGE) print_intermediate_representation(function, 0);

    vreg_count = function->vreg_count;
    ssa_subscript_count = 0;
    tac = function->ir;
    while (tac) {
        if (tac->dst  && tac->dst ->vreg && tac->dst ->ssa_subscript > ssa_subscript_count) ssa_subscript_count = tac->dst ->ssa_subscript;
        if (tac->src1 && tac->src1->vreg && tac->src1->ssa_subscript > ssa_subscript_count) ssa_subscript_count = tac->src1->ssa_subscript;
        if (tac->src2 && tac->src2->vreg && tac->src2->ssa_subscript > ssa_subscript_count) ssa_subscript_count = tac->src2->ssa_subscript;

        if (tac->operation == IR_PHI_FUNCTION) {
            v = tac->phi_values;
            while (v->type) {
                if (v->ssa_subscript > ssa_subscript_count) ssa_subscript_count = v->ssa_subscript;
                v++;
            }
        }

        tac = tac->next;
    }

    ssa_subscript_count += 1; // Starts at zero, so the count is one more

    max_ssa_var = (vreg_count + 1) * ssa_subscript_count;
    ssa_vars = new_set(max_ssa_var);

    // Poor mans 2D array. 2d[vreg][subscript] => 1d[vreg * ssa_subscript_count + ssa_subscript]
    tac = function->ir;
    while (tac) {
        if (tac->dst  && tac->dst ->vreg) add_to_set(ssa_vars, tac->dst ->vreg * ssa_subscript_count + tac->dst ->ssa_subscript);
        if (tac->src1 && tac->src1->vreg) add_to_set(ssa_vars, tac->src1->vreg * ssa_subscript_count + tac->src1->ssa_subscript);
        if (tac->src2 && tac->src2->vreg) add_to_set(ssa_vars, tac->src2->vreg * ssa_subscript_count + tac->src2->ssa_subscript);

        if (tac->operation == IR_PHI_FUNCTION) {
            v = tac->phi_values;
            while (v->type) {
                add_to_set(ssa_vars, v->vreg * ssa_subscript_count + v->ssa_subscript);
                v++;
            }
        }

        tac = tac->next;
    }

    // Create live ranges sets for all variables, each set with the variable itself in it.
    // Allocate all the memory we need. live_range_count
    live_ranges = malloc((vreg_count + 1) * ssa_subscript_count * sizeof(struct set *));
    for (i = 0; i <= max_ssa_var; i++) {
        if (!ssa_vars->elements[i]) continue;
        live_ranges[i] = new_set(max_ssa_var);
        add_to_set(live_ranges[i], i);
    }

    s = new_set(max_ssa_var);

    src_ssa_vars = malloc(MAX_BLOCK_PREDECESSOR_COUNT * sizeof(int));
    src_set_indexes = malloc(MAX_BLOCK_PREDECESSOR_COUNT * sizeof(int));

    // Make live ranges out of SSA variables in phi functions
    tac = function->ir;
    while (tac) {
        if (tac->operation == IR_PHI_FUNCTION) {
            value_count = 0;
            v = tac->phi_values;
            while (v->type) {
                if (value_count == MAX_BLOCK_PREDECESSOR_COUNT) panic("Exceeded MAX_BLOCK_PREDECESSOR_COUNT");
                src_ssa_vars[value_count++] = v->vreg * ssa_subscript_count + v->ssa_subscript;
                v++;
            }

            dst = tac->dst->vreg * ssa_subscript_count + tac->dst->ssa_subscript;

            for (i = 0; i <= max_ssa_var; i++) {
                if (!ssa_vars->elements[i]) continue;

                if (in_set(live_ranges[i], dst)) dst_set_index = i;

                for (j = 0; j < value_count; j++)
                    if (in_set(live_ranges[i], src_ssa_vars[j])) src_set_indexes[j] = i;
            }

            dst_set = live_ranges[dst_set_index];

            copy_set_to(s, dst_set);
            for (j = 0; j < value_count; j++) {
                set_index = src_set_indexes[j];
                set_union_to(s, s, live_ranges[set_index]);
            }
            copy_set_to(live_ranges[dst_set_index], s);

            for (j = 0; j < value_count; j++) {
                set_index = src_set_indexes[j];
                if (set_index != dst_set_index) empty_set(live_ranges[set_index]);
            }
        }
        tac = tac->next;
    }

    free_set(s);

    // Remove empty live ranges
    live_range_count = 0;
    for (i = 0; i <= max_ssa_var; i++)
        if (ssa_vars->elements[i] && set_len(live_ranges[i]))
            live_ranges[live_range_count++] = live_ranges[i];

    // From here on, live ranges start at live_range_reserved_pregs_offset + 1
    if (DEBUG_SSA_LIVE_RANGE) {
        printf("Live ranges:\n");
        for (i = 0; i < live_range_count; i++) {
            printf("%d: ", i + live_range_reserved_pregs_offset + 1);
            printf("{");
            first = 1;
            for (j = 0; j <= live_ranges[i]->max_value; j++) {
                if (!live_ranges[i]->elements[j]) continue;
                if (!first)
                    printf(", ");
                else
                    first = 0;
                printf("%d_%d", j / ssa_subscript_count, j % ssa_subscript_count);
            }

            printf("}\n");
        }

        printf("\n");
    }

    // Make a map of variable names to live range
    map = malloc((vreg_count + 1) * ssa_subscript_count * sizeof(int));
    memset(map, -1, (vreg_count + 1) * ssa_subscript_count * sizeof(int));

    for (i = 0; i < live_range_count; i++) {
        s = live_ranges[i];
        for (j = 0; j <= s->max_value; j++) {
            if (!s->elements[j]) continue;
            map[j] = i;
        }
    }

    // Assign live ranges to TAC & build live_ranges set
    tac = function->ir;
    while (tac) {
        if (tac->dst && tac->dst->vreg)
            tac->dst->live_range = map[tac->dst->vreg * ssa_subscript_count + tac->dst->ssa_subscript] + live_range_reserved_pregs_offset + 1;

        if (tac->src1 && tac->src1->vreg)
            tac->src1->live_range = map[tac->src1->vreg * ssa_subscript_count + tac->src1->ssa_subscript] + live_range_reserved_pregs_offset + 1;

        if (tac->src2 && tac->src2->vreg)
            tac->src2->live_range = map[tac->src2->vreg * ssa_subscript_count + tac->src2->ssa_subscript] + live_range_reserved_pregs_offset + 1;

        tac = tac->next;
    }

    // Remove phi functions
    blocks = function->blocks;
    block_count = function->block_count;

    for (i = 0; i < block_count; i++) {
        tac = blocks[i].start;
        while (tac->operation == IR_PHI_FUNCTION) {
            tac->next->label = tac->label;
            tac->next->prev = tac->prev;
            tac->prev->next = tac->next;
            tac = tac->next;
        }
        blocks[i].start = tac;
    }

    if (DEBUG_SSA_LIVE_RANGE) print_intermediate_representation(function, 0);
}

// Having vreg & live_range separately isn't particularly useful, since most
// downstream code traditionally deals with vregs. So do a vreg=live_range
// for all values
void blast_vregs_with_live_ranges(struct function *function) {
    struct three_address_code *tac;

    tac = function->ir;
    while (tac) {
        if (tac->src1 && tac->src1->vreg) tac->src1->vreg = tac->src1->live_range;
        if (tac->src2 && tac->src2->vreg) tac->src2->vreg = tac->src2->live_range;
        if (tac->dst  && tac->dst-> vreg) tac->dst-> vreg = tac->dst-> live_range;
        tac = tac->next;
    }
}

void add_interference_edge(int *edge_matrix, int vreg_count, int to, int from) {
    int t, index;

    if (from > to) {
        t = from;
        from = to;
        to = t;
    }

    index = from * vreg_count + to;
    if (!edge_matrix[index]) edge_matrix[index] = 1;
}

void add_interference_edge_for_reserved_register(int *edge_matrix, int vreg_count, struct set *livenow, struct three_address_code *tac, int preg_reg_index) {
    int i;

    for (i = 0; i <= livenow->max_value; i++)
        if (livenow->elements[i]) add_interference_edge(edge_matrix, vreg_count, preg_reg_index, i);

    if (tac->dst  && tac->dst ->vreg) add_interference_edge(edge_matrix, vreg_count, preg_reg_index, tac->dst->vreg );
    if (tac->src1 && tac->src1->vreg) add_interference_edge(edge_matrix, vreg_count, preg_reg_index, tac->src1->vreg);
    if (tac->src2 && tac->src2->vreg) add_interference_edge(edge_matrix, vreg_count, preg_reg_index, tac->src2->vreg);
}

// Page 701 of engineering a compiler
void make_interference_graph(struct function *function) {
    int i, j, vreg_count, block_count, edge_count, from, to, index;
    struct block *blocks;
    struct edge *edges;
    struct set *livenow;
    struct three_address_code *tac;
    int *edge_matrix; // Triangular matrix of edges
    int function_call_depth;

    vreg_count = function->vreg_count;

    edge_matrix = malloc((vreg_count + 1) * (vreg_count + 1) * sizeof(int));
    memset(edge_matrix, 0, (vreg_count + 1) * (vreg_count + 1) * sizeof(int));

    blocks = function->blocks;
    block_count = function->block_count;

    function_call_depth = 0;
    for (i = block_count - 1; i >= 0; i--) {
        livenow = copy_set(function->liveout[i]);

        tac = blocks[i].end;
        while (tac) {
            if (tac->operation == IR_END_CALL) function_call_depth++;
            else if (tac->operation == IR_START_CALL) function_call_depth--;

            if (function_call_depth > 0) {
                add_interference_edge_for_reserved_register(edge_matrix, vreg_count, livenow, tac, LIVE_RANGE_PREG_RAX_INDEX);
                add_interference_edge_for_reserved_register(edge_matrix, vreg_count, livenow, tac, LIVE_RANGE_PREG_RDI_INDEX);
                add_interference_edge_for_reserved_register(edge_matrix, vreg_count, livenow, tac, LIVE_RANGE_PREG_RSI_INDEX);
                add_interference_edge_for_reserved_register(edge_matrix, vreg_count, livenow, tac, LIVE_RANGE_PREG_RDX_INDEX);
                add_interference_edge_for_reserved_register(edge_matrix, vreg_count, livenow, tac, LIVE_RANGE_PREG_RCX_INDEX);
                add_interference_edge_for_reserved_register(edge_matrix, vreg_count, livenow, tac, LIVE_RANGE_PREG_R8_INDEX);
                add_interference_edge_for_reserved_register(edge_matrix, vreg_count, livenow, tac, LIVE_RANGE_PREG_R9_INDEX);
            }

            if (tac->operation == IR_DIV || tac->operation == IR_MOD) {
                add_interference_edge_for_reserved_register(edge_matrix, vreg_count, livenow, tac, LIVE_RANGE_PREG_RAX_INDEX);
                add_interference_edge_for_reserved_register(edge_matrix, vreg_count, livenow, tac, LIVE_RANGE_PREG_RDX_INDEX);
            }

            if (tac->operation == IR_BSHL || tac->operation == IR_BSHR)
                add_interference_edge_for_reserved_register(edge_matrix, vreg_count, livenow, tac, LIVE_RANGE_PREG_RCX_INDEX);

            if (tac->dst && tac->dst->vreg) {
                if (tac->operation == IR_RSUB && tac->src1->vreg) {
                    // Ensure that dst and src1 don't reside in the same preg.
                    // This allowes codegen to generate code with just one operation while
                    // ensuring the other registers preserve their values.
                    add_interference_edge(edge_matrix, vreg_count, tac->src1->vreg, tac->dst->vreg);
                }

                for (j = 0; j <= livenow->max_value; j++) {
                    if (!livenow->elements[j]) continue;

                    if (j == tac->dst->vreg) continue; // Ignore self assignment

                    // Don't add an edge for register copies
                    if (tac->operation == IR_ASSIGN && tac->src1->vreg && tac->src1->vreg == j) continue;
                    add_interference_edge(edge_matrix, vreg_count, tac->dst->vreg, j);
                }
            }

            if (tac->dst && tac->dst->vreg) delete_from_set(livenow, tac->dst->vreg);
            if (tac->src1 && tac->src1->vreg) add_to_set(livenow, tac->src1->vreg);
            if (tac->src2 && tac->src2->vreg) add_to_set(livenow, tac->src2->vreg);

            if (tac == blocks[i].start) break;
            tac = tac->prev;
        }

        free_set(livenow);
    }

    // Convert the triangular matrix into an array of edges
    edges = malloc(MAX_INTERFERENCE_GRAPH_EDGES * sizeof(struct edge));
    memset(edges, 0, MAX_INTERFERENCE_GRAPH_EDGES * sizeof(struct edge));
    edge_count = 0;
    for (from = 1; from <= vreg_count; from++) {
        for (to = 1; to <= vreg_count; to++) {
            if (edge_matrix[from * vreg_count + to]) {
                edges[edge_count].from = from;
                edges[edge_count].to = to;
                edge_count++;
                if (edge_count >= MAX_INTERFERENCE_GRAPH_EDGES) panic("Exceeded max MAX_INTERFERENCE_GRAPH_EDGES");
            }
        }
    }

    function->interference_graph = edges;
    function->interference_graph_edge_count = edge_count;

    if (DEBUG_SSA_INTERFERENCE_GRAPH) {
        printf("\nInterference graph edges:\n");
        for (i = 0; i < edge_count; i++)
            printf("%d - %d\n", edges[i].from, edges[i].to);
        printf("\n");
    }
}

// Delete src, merging it into dst
void coalesce_live_range(struct function *function, int src, int dst) {
    struct three_address_code *tac;
    struct block *blocks;
    struct edge *edges;
    struct set *l;
    int i, j, block_count, edge_count, changed;

    // Rewrite IR src => dst
    tac = function->ir;
    while (tac) {
        if (tac->dst  && tac->dst ->vreg == src) { tac->dst ->vreg = dst; tac->dst ->live_range = dst; }
        if (tac->src1 && tac->src1->vreg == src) { tac->src1->vreg = dst; tac->src1->live_range = dst; }
        if (tac->src2 && tac->src2->vreg == src) { tac->src2->vreg = dst; tac->src2->live_range = dst; }

        if (tac->operation == IR_ASSIGN && tac->dst && tac->dst->vreg && tac->src1 && tac->src1->vreg && tac->dst->vreg == tac->src1->vreg) {
            tac->operation = IR_NOP;
            tac->dst = 0;
            tac->src1 = 0;
            tac->src2 = 0;
        }

        tac = tac->next;
    }

    // Move src edges to dst
    edges = function->interference_graph;
    edge_count = function->interference_graph_edge_count;

    for (i = 0; i < edge_count; i++) {
        changed = 0;
        if (edges[i].from == src) { edges[i].from = dst; changed = 1; }
        if (edges[i].to   == src) { edges[i].to   = dst; changed = 1; }
    }

    // Migrate liveouts
    blocks = function->blocks;
    block_count = function->block_count;
    for (i = 0; i < block_count; i++) {
        l = function->liveout[i];
        if (in_set(l, src)) {
            delete_from_set(l, src);
            add_to_set(l, dst);
        }
    }
}

// Page 706 of engineering a compiler
void coalesce_live_ranges(struct function *function) {
    int vreg_count;
    char *merge_candidates;
    struct three_address_code *tac;
    int i, j, k, dst, src, edge_count, changed, intersects;
    struct edge *edges;

    make_vreg_count(function, RESERVED_PHYSICAL_REGISTER_COUNT);
    vreg_count = function->vreg_count;
    merge_candidates = malloc((vreg_count + 1) * (vreg_count + 1) * sizeof(char));

    make_uevar_and_varkill(function);
    make_liveout(function);

    changed = 1;
    while (changed) {
        changed = 0;

        make_live_range_spill_cost(function);
        make_interference_graph(function);

        if (disable_live_ranges_coalesce) return;

        edges = function->interference_graph;
        edge_count = function->interference_graph_edge_count;

        // A lower triangular matrix of all register copy operations
        memset(merge_candidates, 0, (vreg_count + 1) * (vreg_count + 1) * sizeof(char));

        tac = function->ir;
        while (tac) {
            // print_instruction(stdout, tac);
            if (tac->operation == IR_ASSIGN && tac->dst->vreg && tac->src1->vreg) {
                // printf("added candidate %d -> %d \n", tac->dst->vreg, tac->src1->vreg );
                merge_candidates[tac->dst->vreg * vreg_count + tac->src1->vreg]++;
            }
            tac = tac->next;
        }

        if (DEBUG_SSA_LIVE_RANGE_COALESCING) {
            print_intermediate_representation(function, 0);
            printf("Live range coalesces:\n");
        }

        for (dst = 1; dst <= vreg_count; dst++) {
            for (src = 1; src <= vreg_count; src++) {
                if (merge_candidates[dst * vreg_count + src] == 1) {
                    intersects = 0;
                    for (k = 0; k < edge_count; k++) {
                        if ((edges[k].from == dst && edges[k].to == src) || (edges[k].from == src && edges[k].to == dst)){
                            intersects = 1;
                            break;
                        }
                    }

                    if (!intersects) {
                        if (DEBUG_SSA_LIVE_RANGE_COALESCING) printf("%d -> %d\n", dst, src);
                        coalesce_live_range(function, dst, src);
                        changed = 1;
                    }
                }
            }
        }
    }

    free(merge_candidates);

    if (DEBUG_SSA_LIVE_RANGE_COALESCING) {
        printf("\n");
        print_intermediate_representation(function, 0);
    }
}

// 10^p
int ten_power(int p) {
    int i, result;

    result = 1;
    for (i = 0; i < p; i++) result = result * 10;

    return result;
}

// Page 699 of engineering a compiler
// The algorithm is incomplete though. It only looks ad adjacent instructions
// That have a store and a load. A more general version should check for
// any live ranges that have no other live ranges ending between their store
// and loads.
void add_infinite_spill_costs(struct function *function) {
    int i, vreg_count, block_count, edge_count, *spill_cost;
    struct block *blocks;
    struct set *livenow;
    struct three_address_code *tac;

    spill_cost = function->spill_cost;
    vreg_count = function->vreg_count;

    blocks = function->blocks;
    block_count = function->block_count;

    for (i = block_count - 1; i >= 0; i--) {
        livenow = copy_set(function->liveout[i]);

        tac = blocks[i].end;
        while (tac) {

            if (tac->prev && tac->prev->dst && tac->prev->dst->vreg) {
                if ((tac->src1 && tac->src1->vreg && tac->src1->vreg == tac->prev->dst->vreg) ||
                    (tac->src2 && tac->src2->vreg && tac->src2->vreg == tac->prev->dst->vreg)) {

                    if (!in_set(livenow, tac->prev->dst->vreg)) {
                        spill_cost[tac->prev->dst->vreg] = 2 << 15;
                    }
                }
            }
            if (tac->dst && tac->dst->vreg) delete_from_set(livenow, tac->dst->vreg);
            if (tac->src1 && tac->src1->vreg) add_to_set(livenow, tac->src1->vreg);
            if (tac->src2 && tac->src2->vreg) add_to_set(livenow, tac->src2->vreg);

            if (tac == blocks[i].start) break;
            tac = tac->prev;
        }

        free_set(livenow);
    }
}

void make_live_range_spill_cost(struct function *function) {
    struct three_address_code *tac;
    int i, vreg_count, for_loop_depth, *spill_cost;

    vreg_count = function->vreg_count;
    spill_cost = malloc((vreg_count + 1) * sizeof(int));
    memset(spill_cost, 0, (vreg_count + 1) * sizeof(int));

    for_loop_depth = 0;
    tac = function->ir;
    while (tac) {
        if (tac->operation == IR_START_LOOP) for_loop_depth++;
        if (tac->operation == IR_END_LOOP) for_loop_depth--;

        if (tac->dst  && tac->dst ->vreg) spill_cost[tac->dst ->vreg] += ten_power(for_loop_depth);
        if (tac->src1 && tac->src1->vreg) spill_cost[tac->src1->vreg] += ten_power(for_loop_depth);
        if (tac->src2 && tac->src2->vreg) spill_cost[tac->src2->vreg] += ten_power(for_loop_depth);

        tac = tac->next;
    }

    function->spill_cost = spill_cost;

    if (opt_short_lr_infinite_spill_costs) add_infinite_spill_costs(function);

    if (DEBUG_SSA_SPILL_COST) {
        printf("Spill costs:\n");
        for (i = 1; i <= vreg_count; i++)
            printf("%d: %d\n", i, spill_cost[i]);

    }
}

void quicksort_vreg_cost(struct vreg_cost *vreg_cost, int left, int right) {
    int i, j, pivot;
    int tmp_vreg, tmp_cost;

    if (left >= right) return;

    i = left;
    j = right;
    pivot = vreg_cost[i].cost;

    while (1) {
        while (vreg_cost[i].cost > pivot) i++;
        while (pivot > vreg_cost[j].cost) j--;
        if (i >= j) break;

        tmp_vreg = vreg_cost[i].vreg;
        tmp_cost = vreg_cost[i].cost;
        vreg_cost[i].vreg = vreg_cost[j].vreg;
        vreg_cost[i].cost = vreg_cost[j].cost;
        vreg_cost[j].vreg = tmp_vreg;
        vreg_cost[j].cost = tmp_cost;

        i++;
        j--;
    }

    quicksort_vreg_cost(vreg_cost, left, i - 1);
    quicksort_vreg_cost(vreg_cost, j + 1, right);
}

int graph_node_degree(struct edge *edges, int edge_count, int node) {
    int i, result;

    result = 0;
    for (i = 0; i < edge_count; i++)
        if (node == edges[i].from || node == edges[i].to) result++;

    return result;
}

void color_vreg(struct edge *edges, int edge_count, struct vreg_location *vreg_locations, int physical_register_count, int *spilled_register_count, int vreg) {
    int i, j, neighbor, preg;
    struct set *neighbor_colors;

    neighbor_colors = new_set(physical_register_count);
    for (i = 0; i < edge_count; i++) {
        neighbor = -1;
        if (edges[i].from == vreg) neighbor = edges[i].to;
        else if (edges[i].to == vreg) neighbor = edges[i].from;
        if (neighbor == -1) continue;

        preg = vreg_locations[neighbor].preg;
        if (preg != -1) add_to_set(neighbor_colors, preg);
    }

    if (set_len(neighbor_colors) == physical_register_count) {
        vreg_locations[vreg].spilled_index = *spilled_register_count;
        (*spilled_register_count)++;
    }
    else {
        // Find first free physical register
        for (j = 0; j < physical_register_count; j++) {
            if (!in_set(neighbor_colors, j)) {
                vreg_locations[vreg].preg = j;
                return;
            }
        }

        panic("Should not get here");
    }
}

void allocate_registers_top_down(struct function *function, int physical_register_count) {
    int i, vreg_count, *spill_cost, edge_count, degree, spilled_register_count, vreg;
    struct vreg_location *vreg_locations;
    struct set *constrained, *unconstrained;
    struct vreg_cost *ordered_nodes;
    struct edge *edges;

    vreg_count = function->vreg_count;
    spill_cost = function->spill_cost;

    edges = function->interference_graph;
    edge_count = function->interference_graph_edge_count;

    ordered_nodes = malloc((vreg_count + 1) * sizeof(struct vreg_cost));
    for (i = 1; i <= vreg_count; i++) {
        ordered_nodes[i].vreg = i;
        ordered_nodes[i].cost = spill_cost[i];
    }

    quicksort_vreg_cost(ordered_nodes, 1, vreg_count);

    constrained = new_set(vreg_count);
    unconstrained = new_set(vreg_count);

    for (i = 1; i <= vreg_count; i++) {
        degree = graph_node_degree(edges, edge_count, ordered_nodes[i].vreg);
        if (degree < physical_register_count)
            add_to_set(unconstrained, ordered_nodes[i].vreg);
        else
            add_to_set(constrained, ordered_nodes[i].vreg);
    }

    if (DEBUG_SSA_TOP_DOWN_REGISTER_ALLOCATOR) {
        printf("Nodes in order of decreasing cost:\n");
        for (i = 1; i <= vreg_count; i++)
            printf("%d: cost=%d degree=%d\n", ordered_nodes[i].vreg, ordered_nodes[i].cost, graph_node_degree(edges, edge_count, ordered_nodes[i].vreg));

        printf("\nPriority sets:\n");
        printf("constrained:   "); print_set(constrained); printf("\n");
        printf("unconstrained: "); print_set(unconstrained); printf("\n\n");
    }

    vreg_locations = malloc((vreg_count + 1) * sizeof(struct vreg_location));
    memset(vreg_locations, -1, (vreg_count + 1) * sizeof(struct vreg_location));
    spilled_register_count = function->spilled_register_count;

    // Pre-color reserved registers
    if (live_range_reserved_pregs_offset > 0) {
        vreg_locations[LIVE_RANGE_PREG_RAX_INDEX].preg = SSA_PREG_REG_RAX;
        vreg_locations[LIVE_RANGE_PREG_RCX_INDEX].preg = SSA_PREG_REG_RCX;
        vreg_locations[LIVE_RANGE_PREG_RDX_INDEX].preg = SSA_PREG_REG_RDX;
        vreg_locations[LIVE_RANGE_PREG_RSI_INDEX].preg = SSA_PREG_REG_RSI;
        vreg_locations[LIVE_RANGE_PREG_RDI_INDEX].preg = SSA_PREG_REG_RDI;
        vreg_locations[LIVE_RANGE_PREG_R8_INDEX ].preg = SSA_PREG_REG_R8;
        vreg_locations[LIVE_RANGE_PREG_R9_INDEX ].preg = SSA_PREG_REG_R9;
    }

    // Color constrained nodes first
    for (i = 1; i <= vreg_count; i++) {
        vreg = ordered_nodes[i].vreg;
        if (!constrained->elements[vreg]) continue;
        if (live_range_reserved_pregs_offset > 0 && vreg <= RESERVED_PHYSICAL_REGISTER_COUNT) continue;
        color_vreg(edges, edge_count, vreg_locations, physical_register_count, &spilled_register_count, vreg);
    }

    // Color unconstrained nodes
    for (i = 1; i <= vreg_count; i++) {
        vreg = ordered_nodes[i].vreg;
        if (!unconstrained->elements[vreg]) continue;
        if (live_range_reserved_pregs_offset > 0 && vreg <= RESERVED_PHYSICAL_REGISTER_COUNT) continue;
        color_vreg(edges, edge_count, vreg_locations, physical_register_count, &spilled_register_count, vreg);
    }

    if (DEBUG_SSA_TOP_DOWN_REGISTER_ALLOCATOR) {
        printf("Assigned physical registers and stack indexes:\n");

        for (i = 1; i <= vreg_count; i++) {
            printf("%d: ", i);
            if (vreg_locations[i].preg == -1) printf("    "); else printf("%3d", vreg_locations[i].preg);
            if (vreg_locations[i].spilled_index == -1) printf("    "); else printf("%3d", vreg_locations[i].spilled_index);
            printf("\n");
        }
    }

    function->vreg_locations = vreg_locations;
    function->spilled_register_count = spilled_register_count;

    free_set(constrained);
    free_set(unconstrained);
}

void ssa_allocate_registers(struct function *function) {
    int i, *preg_map, preg_count, vreg_count;
    struct vreg_location *function_vl, *vl;

    preg_map = malloc(sizeof(int) * PHYSICAL_REGISTER_COUNT);
    memset(preg_map, 0, sizeof(int) * PHYSICAL_REGISTER_COUNT);

    // Determine amount of free physical registers
    make_available_phyical_register_list(function->ir);

    physical_registers[REG_RAX] = 0;
    physical_registers[REG_RCX] = 0;
    physical_registers[REG_RDX] = 0;
    physical_registers[REG_RSI] = 0;
    physical_registers[REG_RDI] = 0;
    physical_registers[REG_R8]  = 0;
    physical_registers[REG_R9]  = 0;

    preg_count = 0;
    for (i = 0; i < PHYSICAL_REGISTER_COUNT; i++)
        if (physical_registers[i] != -1) {
            if (i == REG_RAX) SSA_PREG_REG_RAX = preg_count;
            if (i == REG_RCX) SSA_PREG_REG_RCX = preg_count;
            if (i == REG_RDX) SSA_PREG_REG_RDX = preg_count;
            if (i == REG_RSI) SSA_PREG_REG_RSI = preg_count;
            if (i == REG_RDI) SSA_PREG_REG_RDI = preg_count;
            if (i == REG_R8)  SSA_PREG_REG_R8  = preg_count;
            if (i == REG_R9)  SSA_PREG_REG_R9  = preg_count;

            preg_map[preg_count++] = i;
        }

    // Reduce the amount of the command line option has used to reduce it
    if (preg_count > ssa_physical_register_count) preg_count = ssa_physical_register_count;

    allocate_registers_top_down(function, preg_count);

    // Remap SSA pregs which run from 0 to preg_count -1 to the actual
    // x86_64 physical register numbers.
    vreg_count = function->vreg_count;
    for (i = 1; i <= vreg_count; i++) {
        if (function->vreg_locations[i].preg != -1)
            function->vreg_locations[i].preg = preg_map[function->vreg_locations[i].preg];
    }

    total_spilled_register_count += function->spilled_register_count;
}

void assign_vreg_locations(struct function *function) {
    struct three_address_code *tac;
    struct vreg_location *function_vl, *vl;

    function_vl = function->vreg_locations;

    tac = function->ir;
    while (tac) {
        if (tac->dst && tac->dst->vreg) {
            vl = &function_vl[tac->dst->vreg];
            if (vl->spilled_index != -1)
                tac->dst->spilled_stack_index = vl->spilled_index;
            else
                tac->dst->preg = vl->preg;
        }

        if (tac->src1 && tac->src1->vreg) {
            vl = &function_vl[tac->src1->vreg];
            if (vl->spilled_index != -1)
                tac->src1->spilled_stack_index = vl->spilled_index;
            else
                tac->src1->preg = vl->preg;
        }

        if (tac->src2 && tac->src2->vreg) {
            vl = &function_vl[tac->src2->vreg];
            if (vl->spilled_index != -1)
                tac->src2->spilled_stack_index = vl->spilled_index;
            else
                tac->src2->preg = vl->preg;
        }

        tac = tac->next;
    }

    function->local_symbol_count = 0; // This nukes ancient code that assumes local vars are on the stack
}

void remove_self_moves(struct function *function) {
    // This removes instructions that copy a physical register to itself by replacing them with noops.

    struct three_address_code *tac;

    tac = function->ir;
    while (tac) {
        if (tac->operation == IR_ASSIGN && tac->dst && tac->dst->preg != -1 && tac->src1 && tac->src1->preg != -1 && tac->dst->preg == tac->src1->preg) {
            tac->operation = IR_NOP;
            tac->dst = 0;
            tac->src1 = 0;
        }

        tac = tac->next;
    }
}

void do_ssa_experiments1(struct function *function) {
    disable_live_ranges_coalesce = !opt_enable_live_range_coalescing;

    sanity_test_ir_linkage(function->ir);
    map_stack_index_to_spilled_stack_index(function);
    rewrite_lvalue_reg_assignments(function);
    make_vreg_count(function, 0);
    make_control_flow_graph(function);
    make_block_dominance(function);
    make_block_immediate_dominators(function);
    make_block_dominance_frontiers(function);
}

void do_ssa_experiments2(struct function *function) {
    make_globals_and_var_blocks(function);
    insert_phi_functions(function);
}

void do_ssa_experiments3(struct function *function) {
    rename_phi_function_variables(function);
    make_live_ranges(function);
    blast_vregs_with_live_ranges(function);
    coalesce_live_ranges(function);
}

void do_ssa_experiments4(struct function *function) {
    ssa_allocate_registers(function);
    assign_vreg_locations(function);
    remove_self_moves(function);
}
