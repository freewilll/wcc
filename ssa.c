#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include "wcc.h"

void optimize_arithmetic_operations(Function *function) {
    Tac *tac;
    Value *v, *cv;
    long c, l;

    if (!opt_optimize_arithmetic_operations) return;

    tac = function->ir;
    while (tac) {
        cv = 0;

        if (tac->src1 && tac->src1->is_constant) {
            cv = tac->src1;
            v = tac->src2;
        }
        else if (tac->src2 && tac->src2->is_constant) {
            v = tac->src1;
            cv = tac->src2;
        }
        if (cv) c = cv->value;

        if (tac->operation == IR_MUL && cv) {
            if (c == 0) {
                tac->operation = IR_MOVE;
                tac->src1 = new_constant(tac->dst->type, 0);
                tac->src2 = 0;
            }

            else if (c == 1) {
                tac->operation = IR_MOVE;
                tac->src1 = v;
                tac->src2 = 0;
            }

            else if ((c & (c - 1)) == 0) {
                l = -1;
                while (c > 0) { c = c >> 1; l++; }
                tac->operation = IR_BSHL;
                tac->src1 = v;
                tac->src2 = new_constant(TYPE_INT, l);
            }
        }

        else if (tac->operation == IR_DIV && cv && tac->src2->is_constant) {
            if (c == 0)
                panic("Illegal division by zero");

            else if (c == 1) {
                tac->operation = IR_MOVE;
                tac->src1 = v;
                tac->src2 = 0;
            }

            else if ((c & (c - 1)) == 0) {
                l = -1;
                while (c > 0) { c = c >> 1; l++; }
                tac->operation = IR_BSHR;
                tac->src1 = v;
                tac->src2 = new_constant(TYPE_INT, l);
            }
        }

        else if (tac->operation == IR_MOD && cv && tac->src2->is_constant) {
            if (c == 0)
                panic("Illegal modulus by zero");

            else if (c == 1) {
                tac->operation = IR_MOVE;
                tac->src1 = new_constant(tac->dst->type, 0);
                tac->src2 = 0;
            }

            else if ((c & (c - 1)) == 0) {
                l = 0;
                while (c > 1) { c = c >> 1; l = (l << 1) | 1; }
                tac->operation = IR_BAND;
                tac->src1 = v;
                tac->src2 = new_constant(TYPE_INT, l);
            }
        }

        tac = tac->next;
    }
}

// Convert an IR_MOVE with an dst is an lvalue in a register into IR_MOVE_TO_PTR.
// The difference with the regular IR_MOVE is that src1 is the destination and src2 is
// the src. The reason for that is that it makes the SSA calulation easier since both
// src1 and src2 are values in registers that are read but not written to in this
// instruction.
void rewrite_lvalue_reg_assignments(Function *function) {
    Tac *tac;

    tac = function->ir;
    while (tac) {
        if (tac->operation == IR_MOVE && tac->dst->vreg && tac->dst->is_lvalue) {
            tac->operation = IR_MOVE_TO_PTR;
            tac->src2 = tac->src1;
            tac->src1 = tac->dst;
            tac->src1->is_lvalue_in_register = 1;
            tac->dst = 0;
        }
        tac = tac->next;
    }
}

void index_tac(Tac *ir) {
    Tac *tac;
    int i;

    tac = ir;
    i = 0;
    while (tac) {
        tac->index = i;
        tac = tac->next;
        i++;
    }
}

void make_control_flow_graph(Function *function) {
    int i, j, k, block_count, label;
    Tac *tac;
    Block *blocks;
    Graph *cfg;

    block_count = 0;

    blocks = malloc(MAX_BLOCKS * sizeof(Block));
    memset(blocks, 0, MAX_BLOCKS * sizeof(Block));
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
        if (tac->next && !tac->next->label && (tac->operation == IR_JZ || tac->operation == IR_JNZ || tac->operation == X_JZ || tac->operation == X_JNZ || tac->operation == X_JE || tac->operation == X_JNE || tac->operation == X_JGT || tac->operation == X_JLT || tac->operation == X_JGE || tac->operation == X_JLE)) {
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
            if (tac->operation == IR_JMP || tac->operation == X_JMP) {
                blocks[i].end = tac;
                break;
            }

            if (tac == blocks[i].end) break;
            tac = tac->next;
        }
    }

    cfg = new_graph(block_count, 0);

    for (i = 0; i < block_count; i++) {
        tac = blocks[i].start;
        while (1) {
            if (tac->operation == IR_JMP || tac->operation == IR_JZ || tac->operation == IR_JNZ || tac->operation == X_JMP || tac->operation == X_JZ || tac->operation == X_JNZ || tac->operation == X_JE || tac->operation == X_JNE || tac->operation == X_JGT || tac->operation == X_JLT || tac->operation == X_JGE || tac->operation == X_JLE) {
                label = tac->operation == IR_JMP ||  tac->operation == X_JMP || tac->operation == X_JZ || tac->operation == X_JNZ || tac->operation == X_JE || tac->operation == X_JNE || tac->operation == X_JGT || tac->operation == X_JLT || tac->operation == X_JGE || tac->operation == X_JLE
                    ? tac->src1->label
                    : tac->src2->label;
                for (k = 0; k < block_count; k++)
                    if (blocks[k].start->label == label)
                        add_graph_edge(cfg, i, k);
            }
            else if (tac->operation != IR_RETURN && tac->next && tac->next->label)
                // For normal instructions, check if the next instruction is a label, if so it's an edge
                add_graph_edge(cfg, i, i + 1);

            if (tac->operation == IR_JZ || tac->operation == IR_JNZ || tac->operation == X_JZ || tac->operation == X_JNZ || tac->operation == X_JE || tac->operation == X_JNE || tac->operation == X_JGT || tac->operation == X_JLT || tac->operation == X_JGE || tac->operation == X_JLE)
                add_graph_edge(cfg, i, i + 1);

            if (tac == blocks[i].end) break;
            tac = tac->next;
        }
    }

    function->blocks = blocks;
    function->cfg = cfg;

    index_tac(function->ir);

    if (debug_ssa_cfg) {
        print_ir(function, 0);

        printf("Blocks:\n");
        for (i = 0; i < block_count; i++) printf("%d: %d -> %d\n", i, blocks[i].start->index, blocks[i].end->index);

        printf("\nEdges:\n");
        for (i = 0; i < cfg->edge_count; i++) printf("%d: %d -> %d\n", i, cfg->edges[i].from->id, cfg->edges[i].to->id);
    }
}

// Algorithm from page 503 of Engineering a compiler
void make_block_dominance(Function *function) {
    int i, j, changed, block_count, got_predecessors;
    Block *blocks;
    Graph *cfg;
    GraphEdge *e;
    Set **dom, *is1, *is2, *pred_intersections;

    blocks = function->blocks;
    cfg = function->cfg;
    block_count = cfg->node_count;

    dom = malloc(block_count * sizeof(Set));
    memset(dom, 0, block_count * sizeof(Set));

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

            e = cfg->nodes[i].pred;
            while (e) {
                pred_intersections = set_intersection(pred_intersections, dom[e->from->id]);
                got_predecessors = 1;
                e = e->next_pred;
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

    function->dominance = malloc(block_count * sizeof(Set));
    memset(function->dominance, 0, block_count * sizeof(Set));

    for (i = 0; i < block_count; i++) function->dominance[i] = dom[i];
}

void make_rpo(Function *function, int *rpos, int *pos, int *visited, int block) {
    Block *blocks;
    Graph *cfg;
    GraphEdge *e;

    if (visited[block]) return;
    visited[block] = 1;

    blocks = function->blocks;
    cfg = function->cfg;

    e = cfg->nodes[block].succ;
    while (e) {
        make_rpo(function, rpos, pos, visited, e->to->id);
        e = e->next_succ;
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
void make_block_immediate_dominators(Function *function) {
    int block_count, edge_count, i, changed;
    int *rpos, pos, *visited, *idoms, *rpos_order, b, p, new_idom;
    Graph *cfg;
    GraphEdge *e;

    cfg = function->cfg;
    block_count = function->cfg->node_count;

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
            e = cfg->nodes[b].pred;
            while (e) {
                p = e->from->id;

                if (idoms[p] != -1) {
                    if (new_idom == -1)
                        new_idom = p;
                    else
                        new_idom = intersect(rpos, idoms, p, new_idom);
                }

                e = e->next_pred;
            }

            if (idoms[b] != new_idom) {
                idoms[b] = new_idom;
                changed = 1;
            }
        }
    }

    idoms[0] = -1;

    if (debug_ssa) {
        printf("\nIdoms:\n");
        for (i = 0; i < block_count; i++) printf("%d: %d\n", i, idoms[i]);
    }

    function->idom = malloc(block_count * sizeof(int));
    memset(function->idom, 0, block_count * sizeof(int));

    for (i = 0; i < block_count; i++) function->idom[i] = idoms[i];
}

// Algorithm on page 499 of engineering a compiler
// Walk the dominator tree defined by the idom (immediate dominator)s.
void make_block_dominance_frontiers(Function *function) {
    int block_count, i, j, *predecessors, predecessor_count, p, runner, *idom;
    Block *blocks;
    Graph *cfg;
    GraphEdge *e;
    Set **df;

    cfg = function->cfg;
    block_count = function->cfg->node_count;

    df = malloc(block_count * sizeof(Set));
    memset(df, 0, block_count * sizeof(Set));

    for (i = 0; i < block_count; i++) df[i] = new_set(block_count);

    predecessors = malloc(block_count * sizeof(int));

    idom = function->idom;

    for (i = 0; i < block_count; i++) {
        predecessor_count = 0;
        e = cfg->nodes[i].pred;
        while (e) {
            predecessors[predecessor_count++] = e->from->id;
            e = e ->next_pred;
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

    function->dominance_frontiers = malloc(block_count * sizeof(Set *));
    for (i = 0; i < block_count; i++) function->dominance_frontiers[i] = df[i];

    if (debug_ssa) {
        printf("\nDominance frontiers:\n");
        for (i = 0; i < block_count; i++) {
            printf("%d: ", i);
            print_set(df[i]);
            printf("\n");
        }
    }
}

void analyze_dominance(Function *function) {
    sanity_test_ir_linkage(function);
    make_vreg_count(function, 0);
    make_control_flow_graph(function);
    make_block_dominance(function);
    make_block_immediate_dominators(function);
    make_block_dominance_frontiers(function);
}

int make_vreg_count(Function *function, int starting_count) {
    int vreg_count;
    Tac *tac;

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

void make_uevar_and_varkill(Function *function) {
    Block *blocks;
    int i, j, block_count, vreg_count;
    Set *uevar, *varkill;
    Tac *tac;

    blocks = function->blocks;
    block_count = function->cfg->node_count;

    function->uevar = malloc(block_count * sizeof(Set *));
    memset(function->uevar, 0, block_count * sizeof(Set *));
    function->varkill = malloc(block_count * sizeof(Set *));
    memset(function->varkill, 0, block_count * sizeof(Set *));

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

    if (debug_ssa) {
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
void make_liveout(Function *function) {
    int i, j, k, vreg, block_count, vreg_count, changed, successor_block;
    Block *blocks;
    Set *unions, **liveout, *all_vars, *inv_varkill, *is1, *is2;
    Tac *tac;
    Graph *cfg;
    GraphEdge *e;
    char *all_vars_elements, *unions_elements, *successor_block_liveout_elements, *successor_block_varkill_elements, *successor_block_uevar_elements;

    blocks = function->blocks;
    cfg = function->cfg;
    block_count = cfg->node_count;

    function->liveout = malloc(block_count * sizeof(Set *));
    memset(function->liveout, 0, block_count * sizeof(Set *));

    vreg_count = function->vreg_count;

    // Set all liveouts to {0}
    for (i = 0; i < block_count; i++)
        function->liveout[i] = new_set(vreg_count);

    // Set all_vars to {0, 1, 2, ... n}, i.e. the set of all vars in a block
    all_vars = new_set(vreg_count);
    all_vars_elements = all_vars->elements;

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
    unions_elements = unions->elements;
    inv_varkill = new_set(vreg_count);
    is1 = new_set(vreg_count);
    is2 = new_set(vreg_count);

    if (debug_ssa_liveout) printf("Doing liveout on %d blocks\n", block_count);

    changed = 1;
    while (changed) {
        changed = 0;

        for (i = 0; i < block_count; i++) {
            empty_set(unions);

            e = cfg->nodes[i].succ;
            while (e) {
                // Got a successor edge from i -> successor_block
                successor_block = e->to->id;
                successor_block_liveout_elements = function->liveout[successor_block]->elements;
                successor_block_varkill_elements = function->varkill[successor_block]->elements;
                successor_block_uevar_elements = function->uevar[successor_block]->elements;

                for (k = 1; k <= vreg_count; k++) {
                    unions->elements[k] =
                        successor_block_uevar_elements[k] ||                               // union
                        unions_elements[k] ||                                              // union
                        (successor_block_liveout_elements[k] &&                            // intersection
                            (all_vars_elements[k] && !successor_block_varkill_elements[k]) // difference
                        );
                }

                e = e->next_succ;
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

    if (debug_ssa_liveout) {
        printf("\nLiveouts:\n");
        for (i = 0; i < block_count; i++) {
            printf("%d: ", i);
            print_set(function->liveout[i]);
            printf("\n");
        }
    }
}

// Algorithm on page 501 of engineering a compiler
void make_globals_and_var_blocks(Function *function) {
    Block *blocks;
    int i, j, block_count, vreg_count;
    Set *globals, **var_blocks, *varkill;
    Tac *tac;

    blocks = function->blocks;
    block_count = function->cfg->node_count;

    vreg_count = function->vreg_count;

    var_blocks = malloc((vreg_count + 1) * sizeof(Set *));
    memset(var_blocks, 0, (vreg_count + 1) * sizeof(Set *));
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
                // printf("vreg_count=%d, tac->dst->vreg=%d\n", vreg_count, tac->dst->vreg);
                add_to_set(var_blocks[tac->dst->vreg], i);
            }

            if (tac == blocks[i].end) break;
            tac = tac->next;
        }

        free_set(varkill);
    }

    function->var_blocks = var_blocks;

    if (debug_ssa) {
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
void insert_phi_functions(Function *function) {
    Set *globals, *global_blocks, *work_list, *df;
    int i, v, block_count, edge_count, vreg_count, global, b, d, label, predecessor_count;
    Set **phi_functions, *vars;
    Block *blocks;
    Graph *cfg;
    GraphEdge *e;
    Tac *tac, *prev;
    Value *phi_values;

    blocks = function->blocks;
    cfg = function->cfg;
    block_count = cfg->node_count;
    globals = function->globals;
    vreg_count = function->vreg_count;

    phi_functions = malloc(block_count * sizeof(Set *));
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

    if (debug_ssa_phi_insertion) printf("phi functions to add:\n");
    for (b = 0; b < block_count; b++) {
        if (debug_ssa_phi_insertion) {
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
            e = cfg->nodes[b].pred;
            while (e) { predecessor_count++; e = e->next_pred; }

            phi_values = malloc((predecessor_count + 1) * sizeof(Value));
            memset(phi_values, 0, (predecessor_count + 1) * sizeof(Value));
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
            blocks[b].start->prev = tac;
            blocks[b].start = tac;
        }

        blocks[b].start->label = label;
    }

    if (debug_ssa_phi_insertion) {
        printf("\nIR with phi functions:\n");
        print_ir(function, 0);
    }
}

int new_subscript(Stack **stack, int *counters, int n) {
    int i;

    i = counters[n]++;
    push_onto_stack(stack[n], i);

    return i;
}

void print_stack_and_counters(Stack **stack, int *counters, int vreg_count) {
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
int safe_stack_top(Stack **stack, int *counters, int n) {

    if (stack[n]->pos == MAX_STACK_SIZE) new_subscript(stack, counters, n);
    return stack_top(stack[n]);
}

// Algorithm on page 506 of engineering a compiler
void rename_vars(Function *function, Stack **stack, int *counters, int block_number, int vreg_count) {
    int i, x, block_count, edge_count, *idoms;;
    Tac *tac, *tac2, *end;
    Block *blocks;
    Block *b;
    Graph *cfg;
    GraphEdge *e;
    Value *v;

    if (debug_ssa_phi_renumbering) {
        printf("\n----------------------------------------\nrename_vars\n");
        print_stack_and_counters(stack, counters, vreg_count);
        printf("\n");
    }

    blocks = function->blocks;
    cfg = function->cfg;
    block_count = cfg->node_count;
    idoms = function->idom;

    b = &blocks[block_number];

    // Rewrite phi function dsts
    if (debug_ssa_phi_renumbering) printf("Rewriting phi function dsts\n");
    tac = b->start;
    while (tac->operation == IR_PHI_FUNCTION) {
        // Rewrite x as new_subscript(x)
        if (debug_ssa_phi_renumbering) printf("Renaming %d ", tac->dst->vreg);
        tac->dst->ssa_subscript = new_subscript(stack, counters, tac->dst->vreg);
        if (debug_ssa_phi_renumbering) printf("to %d in phi function\n", tac->dst->vreg);

        if (tac == b->end) break;
        tac = tac->next;
    }

    // Rewrite operations
    if (debug_ssa_phi_renumbering) printf("Rewriting operations\n");
    tac = b->start;
    while (1) {
        if (tac->operation != IR_PHI_FUNCTION) {
            if (tac->src1 && tac->src1->vreg) {
                tac->src1->ssa_subscript = safe_stack_top(stack, counters, tac->src1->vreg);
                if (debug_ssa_phi_renumbering) printf("rewrote src1 %d\n", tac->src1->vreg);
            }

            if (tac->src2 && tac->src2->vreg) {
                tac->src2->ssa_subscript = safe_stack_top(stack, counters, tac->src2->vreg);
                if (debug_ssa_phi_renumbering) printf("rewrote src2 %d\n", tac->src2->vreg);
            }

            if (tac->dst && tac->dst->vreg) {
                tac->dst->ssa_subscript = new_subscript(stack, counters, tac->dst->vreg);
                if (debug_ssa_phi_renumbering) printf("got new name for dst %d\n", tac->dst->vreg);
            }
        }

        if (tac == b->end) break;
        tac = tac->next;
    }

    // Rewrite phi function parameters in successors
    if (debug_ssa_phi_renumbering) printf("Rewriting successor function params\n");
    e = cfg->nodes[block_number].succ;
    while (e) {
        if (debug_ssa_phi_renumbering) printf("Successor %d\n", e->to->id);
        tac = function->blocks[e->to->id].start;
        end = function->blocks[e->to->id].end;
        while (1) {
            if (tac->operation == IR_PHI_FUNCTION) {
                v = tac->phi_values;
                while (v->type) {
                    if (v->ssa_subscript == -1) {
                        v->ssa_subscript = safe_stack_top(stack, counters, v->vreg);
                        if (debug_ssa_phi_renumbering) printf("  rewrote arg to %d\n", v->vreg);
                        break;
                    }
                    v++;
                }
            }
            if (tac == end) break;
            tac = tac->next;
        }

        e = e->next_succ;
    }

    // Recurse down the dominator tree
    for (i = 0; i < block_count; i++) {
        if (idoms[i] == block_number) {
            if (debug_ssa_phi_renumbering) printf("going into idom successor %d\n", i);
            rename_vars(function, stack, counters, i, vreg_count);
        }
    }

    // Pop contents of current block assignments off of the stack
    if (debug_ssa_phi_renumbering) printf("Block done. Cleaning up stack\n");
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
void rename_phi_function_variables(Function *function) {
    int i, vreg_count, *counters;
    Stack **stack;
    Tac *tac;

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

    stack = malloc((vreg_count + 1) * sizeof(Stack *));
    for (i = 1; i <= vreg_count; i++) stack[i] = new_stack();

    rename_vars(function, stack, counters, 0, vreg_count);

    if (debug_ssa_phi_renumbering) print_ir(function, 0);
}

// Page 696 engineering a compiler
// To build live ranges from ssa form, the allocator uses the disjoint-set union- find algorithm.
void make_live_ranges(Function *function) {
    int i, j, live_range_count, vreg_count, ssa_subscript_count, max_ssa_var, block_count;
    int dst, src1, src2;
    Tac *tac, *prev;
    int *map, first;
    Set *ssa_vars, **live_ranges;
    Set *dst_set, *src1_set, *src2_set, *s;
    int dst_set_index, src1_set_index, src2_set_index;
    Block *blocks;
    Value *v;
    int *src_ssa_vars, *src_set_indexes;
    int value_count, set_index;

    live_range_reserved_pregs_offset = RESERVED_PHYSICAL_REGISTER_COUNT; // See the list at the top of the file

    if (debug_ssa_live_range) print_ir(function, 0);

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
    live_ranges = malloc((vreg_count + 1) * ssa_subscript_count * sizeof(Set *));
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
    if (debug_ssa_live_range) {
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
    block_count = function->cfg->node_count;

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

    if (debug_ssa_live_range) print_ir(function, 0);
}

// Having vreg & live_range separately isn't particularly useful, since most
// downstream code traditionally deals with vregs. So do a vreg=live_range
// for all values
void blast_vregs_with_live_ranges(Function *function) {
    Tac *tac;

    tac = function->ir;
    while (tac) {
        if (tac->src1 && tac->src1->vreg) tac->src1->vreg = tac->src1->live_range;
        if (tac->src2 && tac->src2->vreg) tac->src2->vreg = tac->src2->live_range;
        if (tac->dst  && tac->dst-> vreg) tac->dst-> vreg = tac->dst-> live_range;
        tac = tac->next;
    }

    // Nuke the live ranges, they should not be used downstream and this guarantees a horrible failure if they are.
    // Also, remove ssa_subscript, since this is no longer used. Mostly so that print_ir
    // doesn't show them.
    tac = function->ir;
    while (tac) {
        if (tac->src1 && tac->src1->vreg) { tac->src1->live_range = -100000; tac->src1->ssa_subscript = -1; }
        if (tac->src2 && tac->src2->vreg) { tac->src2->live_range = -100000; tac->src2->ssa_subscript = -1; }
        if (tac->dst  && tac->dst-> vreg) { tac->dst-> live_range = -100000; tac->dst-> ssa_subscript = -1; }
        tac = tac->next;
    }
}

void add_ig_edge(char *ig, int vreg_count, int to, int from) {
    int index;

    if (from > to)
        index = to * vreg_count + from;
    else
        index = from * vreg_count + to;

    if (!ig[index]) ig[index] = 1;
}

void add_ig_edge_for_reserved_register(char *ig, int vreg_count, Set *livenow, Tac *tac, int preg_reg_index) {
    int i;

    for (i = 0; i <= livenow->max_value; i++)
        if (livenow->elements[i]) add_ig_edge(ig, vreg_count, preg_reg_index, i);

    if (tac->dst  && tac->dst ->vreg) add_ig_edge(ig, vreg_count, preg_reg_index, tac->dst->vreg );
    if (tac->src1 && tac->src1->vreg) add_ig_edge(ig, vreg_count, preg_reg_index, tac->src1->vreg);
    if (tac->src2 && tac->src2->vreg) add_ig_edge(ig, vreg_count, preg_reg_index, tac->src2->vreg);
}

// Page 701 of engineering a compiler
void make_interference_graph(Function *function) {
    int i, j, vreg_count, block_count, edge_count, from, to, index, from_offset;
    Block *blocks;
    Set *livenow;
    Tac *tac;
    char *interference_graph; // Triangular matrix of edges
    int function_call_depth;

    if (debug_ssa_interference_graph) print_ir(function, 0);

    vreg_count = function->vreg_count;

    interference_graph = malloc((vreg_count + 1) * (vreg_count + 1) * sizeof(int));
    memset(interference_graph, 0, (vreg_count + 1) * (vreg_count + 1) * sizeof(int));

    blocks = function->blocks;
    block_count = function->cfg->node_count;

    function_call_depth = 0;
    for (i = block_count - 1; i >= 0; i--) {
        livenow = copy_set(function->liveout[i]);

        tac = blocks[i].end;
        while (tac) {
            if (debug_ssa_interference_graph) print_instruction(stdout, tac);

            if (tac->operation == IR_END_CALL) function_call_depth++;
            else if (tac->operation == IR_START_CALL) function_call_depth--;

            if (function_call_depth > 0) {
                add_ig_edge_for_reserved_register(interference_graph, vreg_count, livenow, tac, LIVE_RANGE_PREG_RAX_INDEX);
                add_ig_edge_for_reserved_register(interference_graph, vreg_count, livenow, tac, LIVE_RANGE_PREG_RDI_INDEX);
                add_ig_edge_for_reserved_register(interference_graph, vreg_count, livenow, tac, LIVE_RANGE_PREG_RSI_INDEX);
                add_ig_edge_for_reserved_register(interference_graph, vreg_count, livenow, tac, LIVE_RANGE_PREG_RDX_INDEX);
                add_ig_edge_for_reserved_register(interference_graph, vreg_count, livenow, tac, LIVE_RANGE_PREG_RCX_INDEX);
                add_ig_edge_for_reserved_register(interference_graph, vreg_count, livenow, tac, LIVE_RANGE_PREG_R8_INDEX);
                add_ig_edge_for_reserved_register(interference_graph, vreg_count, livenow, tac, LIVE_RANGE_PREG_R9_INDEX);
            }

            if (tac->operation == IR_DIV || tac->operation == IR_MOD || tac->operation == X_IDIV) {
                add_ig_edge_for_reserved_register(interference_graph, vreg_count, livenow, tac, LIVE_RANGE_PREG_RAX_INDEX);
                add_ig_edge_for_reserved_register(interference_graph, vreg_count, livenow, tac, LIVE_RANGE_PREG_RDX_INDEX);

                // Works together with the instruction rules
                if (tac->operation == X_IDIV)
                    add_ig_edge(interference_graph, vreg_count, tac->src2->vreg, tac->dst->vreg);
            }

            if (tac->operation == IR_BSHL || tac->operation == IR_BSHR)
                add_ig_edge_for_reserved_register(interference_graph, vreg_count, livenow, tac, LIVE_RANGE_PREG_RCX_INDEX);

            if (tac->dst && tac->dst->vreg) {
                if (tac->operation == IR_RSUB && tac->src1->vreg) {
                    // Ensure that dst and src1 don't reside in the same preg.
                    // This allows codegen to generate code with just one operation while
                    // ensuring the other registers preserve their values.
                    add_ig_edge(interference_graph, vreg_count, tac->src1->vreg, tac->dst->vreg);
                    if (debug_ssa_interference_graph) printf("added src1 <-> dst %d <-> %d\n", tac->src1->vreg, tac->dst->vreg);
                }

                if (tac->operation == X_SUB && tac->src2->vreg) {
                    // Ensure that dst and src2 don't reside in the same preg.
                    // This allows codegen to generate code with just one mov and sub while
                    // ensuring the other registers preserve their values.
                    add_ig_edge(interference_graph, vreg_count, tac->src2->vreg, tac->dst->vreg);
                    if (debug_ssa_interference_graph) printf("added src2 <-> dst %d <-> %d\n", tac->src2->vreg, tac->dst->vreg);
                }

                for (j = 0; j <= livenow->max_value; j++) {
                    if (!livenow->elements[j]) continue;

                    if (j == tac->dst->vreg) continue; // Ignore self assignment

                    // Don't add an edge for register copies
                    if ((
                        tac->operation == IR_MOVE ||
                        tac->operation == X_MOV ||
                        tac->operation == X_MOVZBW ||
                        tac->operation == X_MOVZBL ||
                        tac->operation == X_MOVZBQ ||
                        tac->operation == X_MOVSBW ||
                        tac->operation == X_MOVSBL ||
                        tac->operation == X_MOVSBQ ||
                        tac->operation == X_MOVSWL ||
                        tac->operation == X_MOVSWQ ||
                        tac->operation == X_MOVSLQ
                       ) && tac->src1 && tac->src1->vreg && tac->src1->vreg == j) continue;
                    add_ig_edge(interference_graph, vreg_count, tac->dst->vreg, j);
                    if (debug_ssa_interference_graph) printf("added dst <-> lr %d <-> %d\n", tac->dst->vreg, j);
                }
            }

            if (tac->dst && tac->dst->vreg) {
                if (debug_ssa_interference_graph)
                    printf("livenow: -= %d -> ", tac->dst->vreg);
                delete_from_set(livenow, tac->dst->vreg);
                if (debug_ssa_interference_graph) { print_set(livenow); printf("\n"); }
            }

            if (tac->src1 && tac->src1->vreg) {
                if (debug_ssa_interference_graph)
                    printf("livenow: += (src1) %d -> ", tac->src1->vreg);
                add_to_set(livenow, tac->src1->vreg);
                if (debug_ssa_interference_graph) { print_set(livenow); printf("\n"); }
            }

            if (tac->src2 && tac->src2->vreg) {
                if (debug_ssa_interference_graph)
                    printf("livenow: += (src2) %d -> ", tac->src2->vreg);
                add_to_set(livenow, tac->src2->vreg);
                if (debug_ssa_interference_graph) { print_set(livenow); printf("\n"); }
            }

            if (tac == blocks[i].start) break;
            tac = tac->prev;
        }

        free_set(livenow);
    }

    function->interference_graph = interference_graph;

    if (debug_ssa_interference_graph) {
        for (from = 1; from <= vreg_count; from++) {
            from_offset = from * vreg_count;
            for (to = from + 1; to <= vreg_count; to++) {
                if (interference_graph[from_offset + to])
                    printf("%d - %d\n", to, from);
            }
        }
    }
}

void copy_interference_graph_edges(char *interference_graph, int vreg_count, int src, int dst) {
    // Copy all edges in the lower triangular interference graph matrics from src to dst
    int i;

    // Fun with lower triangular matrices follows ...
    for (i = 1; i <= vreg_count; i++) {
        // src < i case
        if (interference_graph[src * vreg_count + i] == 1) {
            if (dst < i)
                interference_graph[dst * vreg_count + i] = 1;
            else
                interference_graph[i * vreg_count + dst] = 1;
        }

        // i < src case
        if (interference_graph[i * vreg_count + src] == 1) {
            if (i < dst)
                interference_graph[i * vreg_count + dst] = 1;
            else
                interference_graph[dst * vreg_count + i] = 1;
        }
    }
}

// Delete src, merging it into dst
void coalesce_live_range(Function *function, int src, int dst) {
    char *ig;
    int i, j, block_count, changed, vreg_count, from, to, from_offset;
    Tac *tac;
    Set *l;

    vreg_count = function->vreg_count;

    // Rewrite IR src => dst
    tac = function->ir;
    while (tac) {
        if (tac->dst  && tac->dst ->vreg == src) tac->dst ->vreg = dst;
        if (tac->src1 && tac->src1->vreg == src) tac->src1->vreg = dst;
        if (tac->src2 && tac->src2->vreg == src) tac->src2->vreg = dst;

        if (tac->operation == IR_MOVE && tac->dst && tac->dst->vreg && tac->src1 && tac->src1->vreg && tac->dst->vreg == tac->src1->vreg) {
            tac->operation = IR_NOP;
            tac->dst = 0;
            tac->src1 = 0;
            tac->src2 = 0;
        }

        // Sanity check for instrsel, ensure dst != src1, dst != src2 and src1 != src2
        if (tac->dst && tac->dst->vreg && tac->src1 && tac->src1->vreg && tac->dst->vreg == tac->src1->vreg) {
            print_instruction(stdout, tac);
            panic1d("Illegal violation of dst != src1 (%d), required by instrsel", tac->dst->vreg);
        }
        if (tac->dst && tac->dst->vreg && tac->src2 && tac->src2->vreg && tac->dst->vreg == tac->src2->vreg) {
            print_instruction(stdout, tac);
            panic1d("Illegal violation of dst != src2 (%d) , required by instrsel", tac->dst->vreg);
        }
        if (tac->src1 && tac->src1->vreg && tac->src2 && tac->src2->vreg && tac->src1->vreg == tac->src2->vreg) {
            print_instruction(stdout, tac);
            panic1d("Illegal violation of src1 != src2 (%d) , required by instrsel", tac->src1->vreg);
        }

        tac = tac->next;
    }

    copy_interference_graph_edges(function->interference_graph, vreg_count, src, dst);

    // Migrate liveouts
    block_count = function->cfg->node_count;
    for (i = 0; i < block_count; i++) {
        l = function->liveout[i];
        if (in_set(l, src)) {
            delete_from_set(l, src);
            add_to_set(l, dst);
        }
    }
}

// Page 706 of engineering a compiler
//
// An outer loop (re)calculates the interference graph.
// An inner loop discovers the live range to be coalesced, coalesces them,
// and does a conservative update to the interference graph. The update can introduce
// false interferences, however they will disappear when the outer loop runs again.
// Since earlier coalesces can lead to later coalesces not happening, with each inner
// loop, the registers with the highest spill cost are coalesced.
void coalesce_live_ranges(Function *function) {
    int i, j, k, dst, src, edge_count, outer_changed, inner_changed, vreg_count;
    int l1, l2;
    int max_cost, cost, merge_src, merge_dst;
    char *interference_graph, *merge_candidates, *instrsel_blockers;
    Tac *tac;

    make_vreg_count(function, RESERVED_PHYSICAL_REGISTER_COUNT);
    vreg_count = function->vreg_count;
    merge_candidates = malloc((vreg_count + 1) * (vreg_count + 1) * sizeof(char));
    instrsel_blockers = malloc((vreg_count + 1) * (vreg_count + 1) * sizeof(char));

    make_uevar_and_varkill(function);
    make_liveout(function);

    outer_changed = 1;
    while (outer_changed) {
        outer_changed = 0;

        make_live_range_spill_cost(function);
        make_interference_graph(function);

        if (!opt_enable_live_range_coalescing) return;

        interference_graph = function->interference_graph;

        inner_changed = 1;
        while (inner_changed) {
            inner_changed = max_cost = merge_src = merge_dst = 0;

            // A lower triangular matrix of all register copy operations and instrsel blockers
            memset(merge_candidates, 0, (vreg_count + 1) * (vreg_count + 1) * sizeof(char));
            memset(instrsel_blockers, 0, (vreg_count + 1) * (vreg_count + 1) * sizeof(char));

            // Create merge candidates
            tac = function->ir;
            while (tac) {
                // Don't coalesce a move if it promotes an integer type so that both vregs retain their precision.
                if (tac->operation == IR_MOVE && tac->dst->vreg && tac->src1->vreg && !is_promotion(tac->src1->type, tac->dst->type))
                    merge_candidates[tac->dst->vreg * vreg_count + tac->src1->vreg]++;
                else {
                    // Don't allow merges which violate instrsel constraints
                    if (tac-> dst && tac-> dst->vreg && tac->src1 && tac->src1->vreg) instrsel_blockers[tac-> dst->vreg * vreg_count + tac->src1->vreg] = 1;
                    if (tac-> dst && tac-> dst->vreg && tac->src2 && tac->src2->vreg) instrsel_blockers[tac-> dst->vreg * vreg_count + tac->src2->vreg] = 1;
                    if (tac->src1 && tac->src1->vreg && tac->src2 && tac->src2->vreg) instrsel_blockers[tac->src1->vreg * vreg_count + tac->src2->vreg] = 1;
                }
                tac = tac->next;
            }

            if (debug_ssa_live_range_coalescing) {
                print_ir(function, 0);
                printf("Live range coalesces:\n");
            }

            for (dst = 1; dst <= vreg_count; dst++) {
                for (src = 1; src <= vreg_count; src++) {
                    l1 = dst * vreg_count + src;
                    l2 = src * vreg_count + dst;

                    if (merge_candidates[l1] == 1 && instrsel_blockers[l1] != 1 && instrsel_blockers[l2] != 1) {
                        if (!((src < dst && interference_graph[l2]) || (interference_graph[l1]))) {
                            cost = function->spill_cost[src] + function->spill_cost[dst];
                            if (cost > max_cost) {
                                max_cost = cost;
                                merge_src = src;
                                merge_dst = dst;
                            }
                        }
                    }
                }
            }

            if (merge_src) {
                if (debug_ssa_live_range_coalescing) printf("Coalescing %d -> %d\n", merge_src, merge_dst);
                coalesce_live_range(function, merge_src, merge_dst);
                inner_changed = 1;
                outer_changed = 1;
            }
        }  // while inner changed
    } // while outer changed

    free(merge_candidates);
    free(instrsel_blockers);

    if (debug_ssa_live_range_coalescing) {
        printf("\n");
        print_ir(function, 0);
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
void add_infinite_spill_costs(Function *function) {
    int i, vreg_count, block_count, edge_count, *spill_cost;
    Block *blocks;
    Set *livenow;
    Tac *tac;

    spill_cost = function->spill_cost;
    vreg_count = function->vreg_count;

    blocks = function->blocks;
    block_count = function->cfg->node_count;

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

void make_live_range_spill_cost(Function *function) {
    Tac *tac;
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

    if (debug_ssa_spill_cost) {
        printf("Spill costs:\n");
        for (i = 1; i <= vreg_count; i++)
            printf("%d: %d\n", i, spill_cost[i]);

    }
}

