#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "wcc.h"

static void make_live_range_spill_cost(Function *function);

// Optimize arithmetic operations for integer values
void optimize_integer_arithmetic_operation(Tac *tac) {
    Value *v;
    Value *cv = 0;
    long c, l;

    // If both are constants, don't optimizing, since the operation will be evaluated later on.
    if (tac->src1 && tac->src1->is_constant && tac->src2 && tac->src2->is_constant) return;

    if (tac->src1 && tac->src1->is_constant) {
        cv = tac->src1;
        v = tac->src2;
    }
    else if (tac->src2 && tac->src2->is_constant) {
        v = tac->src1;
        cv = tac->src2;
    }
    if (cv) c = cv->int_value;

    if (tac->operation == IR_MUL && cv) {
        if (c == 0) {
            tac->operation = IR_MOVE;
            tac->src1 = new_integral_constant(tac->dst->type->type, 0);
            tac->src2 = 0;
        }

        else if (c == 1) {
            tac->operation = IR_MOVE;
            tac->src1 = v;
            tac->src2 = 0;
        }

        else if ((c & (c - 1)) == 0) {
            l = -1;
            while (c > 0) { c >>= 1; l++; }
            tac->operation = IR_BSHL;
            tac->src1 = v;
            tac->src2 = new_integral_constant(TYPE_INT, l);
        }
    }

    else if (tac->operation == IR_DIV && cv && tac->src2->is_constant) {
        if (c == 1) {
            tac->operation = IR_MOVE;
            tac->src1 = v;
            tac->src2 = 0;
        }

        else if ((c & (c - 1)) == 0) {
            l = -1;
            while (c > 0) { c >>= 1; l++; }
            tac->operation = IR_BSHR;
            tac->src1 = v;
            tac->src2 = new_integral_constant(TYPE_INT, l);
        }
    }

    else if (tac->operation == IR_MOD && cv && tac->src2->is_constant) {
        if (c == 1) {
            tac->operation = IR_MOVE;
            tac->src1 = new_integral_constant(tac->dst->type->type, 0);
            tac->src2 = 0;
        }

        else if ((c & (c - 1)) == 0) {
            l = 0;
            while (c > 1) { c = c >> 1; l = (l << 1) | 1; }
            tac->operation = IR_BAND;
            tac->src1 = v;
            tac->src2 = new_integral_constant(TYPE_INT, l);
        }
    }
}

void optimize_floating_point_arithmetic_operation(Tac *tac) {
    Value *v;
    Value *cv = 0;
    long double c;

    if (tac->src1 && tac->src1->is_constant) {
        cv = tac->src1;
        v = tac->src2;
    }
    else if (tac->src2 && tac->src2->is_constant) {
        v = tac->src1;
        cv = tac->src2;
    }
    if (cv) c = cv->fp_value;

    if (tac->operation == IR_MUL && cv) {
        if (c == 0.0L) {
            tac->operation = IR_MOVE;
            tac->src1 = new_floating_point_constant(tac->dst->type->type, 0.0L);
            tac->src2 = 0;
        }
        else if (c == 1.0L) {
            tac->operation = IR_MOVE;
            tac->src1 = v;
            tac->src2 = 0;
        }
    }

    else if (tac->operation == IR_DIV && cv && tac->src2->is_constant) {
        c = tac->src2->fp_value;

        if (c == 0.0L)
            panic("Division by zero");

        else if (c == 1.0L) {
            tac->operation = IR_MOVE;
            tac->src1 = v;
            tac->src2 = 0;
        }
    }
}

void optimize_arithmetic_operations(Function *function) {
    if (!opt_optimize_arithmetic_operations) return;

    for (Tac *tac = function->ir; tac; tac = tac->next) {
        Type *src1_type = tac->src1 && tac->src1->is_constant ? tac->src1->type : 0;
        Type *src2_type = tac->src2 && tac->src2->is_constant ? tac->src2->type : 0;

        if ((src1_type && is_floating_point_type(src1_type)) || (src2_type && is_floating_point_type(src2_type)))
            optimize_floating_point_arithmetic_operation(tac);
        else
            optimize_integer_arithmetic_operation(tac);
    }
}

// Convert an IR_MOVE with an dst is an lvalue in a register into IR_MOVE_TO_PTR.
// The difference with the regular IR_MOVE is that src1 is the destination and src2 is
// the src. The reason for that is that it makes the SSA calulation easier since both
// src1 and src2 are values in registers that are read but not written to in this
// instruction.
void rewrite_lvalue_reg_assignments(Function *function) {
    for (Tac *tac = function->ir; tac; tac = tac->next) {
        if (tac->operation == IR_MOVE && tac->dst->vreg && tac->dst->is_lvalue) {
            tac->operation = IR_MOVE_TO_PTR;
            tac->src2 = tac->src1;
            tac->src1 = tac->dst;
            tac->src1->is_lvalue_in_register = 1;
            tac->dst = 0;
        }
    }
}

static void index_tac(Tac *ir) {
    int i = 0;
    for (Tac *tac = ir; tac; tac = tac->next) {
        tac->index = i;
        i++;
    }
}

void make_control_flow_graph(Function *function) {
    Block *blocks = malloc(MAX_BLOCKS * sizeof(Block));
    memset(blocks, 0, MAX_BLOCKS * sizeof(Block));
    blocks[0].start = function->ir;
    int block_count = 1;

    for (Tac *tac = function->ir; tac; tac = tac->next) {
        if (tac->label) {
            if (block_count == MAX_BLOCKS) panic("Exceeded max blocks %d", MAX_BLOCKS);
            blocks[block_count - 1].end = tac->prev;
            blocks[block_count++].start = tac;
        }

        // Start a new block after a conditional jump.
        // Check if a label is set so that we don't get a double block
        if (tac->next && !tac->next->label && (tac->operation == IR_JZ || tac->operation == IR_JNZ || tac->operation == X_JZ || tac->operation == X_JNZ || tac->operation == X_JE || tac->operation == X_JNE || tac->operation == X_JGT || tac->operation == X_JLT || tac->operation == X_JGE || tac->operation == X_JLE || tac->operation == X_JB || tac->operation == X_JA || tac->operation == X_JBE || tac->operation == X_JAE)) {
            if (block_count == MAX_BLOCKS) panic("Exceeded max blocks %d", MAX_BLOCKS);
            blocks[block_count - 1].end = tac;
            blocks[block_count++].start = tac->next;
        }

        // Make instructions IR_NOP after with JMP operations until the next label since
        // the instructions afterwards will never get executed. Furthermore, the
        // instructions later on will mess with the liveness analysis, leading to
        // incorrect live ranges for the code that _is_ executed, so they need to get
        // excluded.
        if ((tac->operation == IR_JMP || tac->operation == X_JMP) && tac->next && !tac->next->label) {
            while (tac->next && !tac->next->label) {
                tac = tac->next;
                tac->operation = IR_NOP;
                tac->dst = 0;
                tac->src1 = 0;
                tac->src2 = 0;
            }
            tac = tac->prev;
        }
    }


    Tac *tac = function->ir;
    while (tac->next) tac = tac->next;
    blocks[block_count - 1].end = tac;

    index_tac(function->ir);

    Graph *cfg = new_graph(block_count, 0);

    for (int i = 0; i < block_count; i++) {
        tac = blocks[i].start;
        while (1) {
            if (tac->operation == IR_JMP || tac->operation == IR_JZ || tac->operation == IR_JNZ || tac->operation == X_JMP || tac->operation == X_JZ || tac->operation == X_JNZ || tac->operation == X_JE || tac->operation == X_JNE || tac->operation == X_JGT || tac->operation == X_JLT || tac->operation == X_JGE || tac->operation == X_JLE || tac->operation == X_JB || tac->operation == X_JA || tac->operation == X_JBE || tac->operation == X_JAE) {
                int label = tac->operation == IR_JMP ||  tac->operation == X_JMP || tac->operation == X_JZ || tac->operation == X_JNZ || tac->operation == X_JE || tac->operation == X_JNE || tac->operation == X_JGT || tac->operation == X_JLT || tac->operation == X_JGE || tac->operation == X_JLE || tac->operation == X_JB || tac->operation == X_JA || tac->operation == X_JBE || tac->operation == X_JAE
                    ? tac->src1->label
                    : tac->src2->label;
                for (int j = 0; j < block_count; j++)
                    if (blocks[j].start->label == label)
                        add_graph_edge(cfg, i, j);
            }
            else if (tac->operation != IR_RETURN && tac->next && tac->next->label)
                // For normal instructions, check if the next instruction is a label, if so it's an edge
                add_graph_edge(cfg, i, i + 1);

            if (tac->operation == IR_JZ || tac->operation == IR_JNZ || tac->operation == X_JZ || tac->operation == X_JNZ || tac->operation == X_JE || tac->operation == X_JNE || tac->operation == X_JGT || tac->operation == X_JLT || tac->operation == X_JGE || tac->operation == X_JLE || tac->operation == X_JB || tac->operation == X_JA || tac->operation == X_JBE || tac->operation == X_JAE)
                add_graph_edge(cfg, i, i + 1);

            if (tac == blocks[i].end) break;
            if (tac->operation == IR_JMP || tac->operation == X_JMP) break;

            tac = tac->next;
        }
    }

    function->blocks = blocks;
    function->cfg = cfg;

    index_tac(function->ir);

    if (debug_ssa_cfg) {
        print_ir(function, 0, 0);

        printf("Blocks:\n");
        for (int i = 0; i < block_count; i++) printf("%d: %d -> %d\n", i, blocks[i].start->index, blocks[i].end->index);

        printf("\nEdges:\n");
        for (int i = 0; i < cfg->edge_count; i++) printf("%d: %d -> %d\n", i, cfg->edges[i].from->id, cfg->edges[i].to->id);
    }
}

// Algorithm from page 503 of Engineering a compiler
void make_block_dominance(Function *function) {
    Graph *cfg = function->cfg;
    int block_count = cfg->node_count;

    Set **dom = malloc(block_count * sizeof(Set));
    memset(dom, 0, block_count * sizeof(Set));

    // dom[0] = {0}
    dom[0] = new_set(block_count);
    add_to_set(dom[0], 0);

    // dom[1 to n] = {0,1,2,..., n}, i.e. the set of all blocks
    for (int i = 1; i < block_count; i++) {
        dom[i] = new_set(block_count);
        for (int j = 0; j < block_count; j++) add_to_set(dom[i], j);
    }

    Set *is1 = new_set(block_count);
    Set *is2 = new_set(block_count);

    // Recalculate dom by looking at each node's predecessors until nothing changes
    // Dom(n) = {n} union (intersection of all predecessor's doms)
    int changed = 1;
    while (changed) {
        changed = 0;

        for (int i = 0; i < block_count; i++) {
            Set *pred_intersections = new_set(block_count);
            for (int j = 0; j < block_count; j++) add_to_set(pred_intersections, j);
            int got_predecessors = 0;

            GraphEdge *e = cfg->nodes[i].pred;
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

    for (int i = 0; i < block_count; i++) function->dominance[i] = dom[i];
}

static void make_rpo(Function *function, int *rpos, int *pos, int *visited, int block) {
    if (visited[block]) return;
    visited[block] = 1;

    Graph *cfg = function->cfg;

    GraphEdge *e = cfg->nodes[block].succ;
    while (e) {
        make_rpo(function, rpos, pos, visited, e->to->id);
        e = e->next_succ;
    }

    rpos[block] = *pos;
    (*pos)--;
}

static int intersect(int *rpos, int *idoms, int i, int j) {
    int f1 = i;
    int f2 = j;

    while (f1 != f2) {
        while (rpos[f1] > rpos[f2]) f1 = idoms[f1];
        while (rpos[f2] > rpos[f1]) f2 = idoms[f2];
    }

    return f1;
}

// Algorithm on page 532 of engineering a compiler
static void make_block_immediate_dominators(Function *function) {
    Graph *cfg = function->cfg;
    int block_count = function->cfg->node_count;

    int *rpos = malloc(block_count * sizeof(int));
    memset(rpos, 0, block_count * sizeof(int));

    int *visited = malloc(block_count * sizeof(int));
    memset(visited, 0, block_count * sizeof(int));

    int pos = block_count - 1;
    make_rpo(function, rpos, &pos, visited, 0);

    int *rpos_order = malloc(block_count * sizeof(int));
    memset(rpos_order, 0, block_count * sizeof(int));
    for (int i = 0; i < block_count; i++) rpos_order[rpos[i]] = i;

    int *idoms = malloc(block_count * sizeof(int));
    memset(idoms, 0, block_count * sizeof(int));

    for (int i = 0; i < block_count; i++) idoms[i] = -1;
    idoms[0] = 0;

    int changed = 1;
    while (changed) {
        changed = 0;

        for (int i = 0; i < block_count; i++) {
            int b = rpos_order[i];
            if (b == 0) continue;

            int new_idom = -1;
            GraphEdge *e = cfg->nodes[b].pred;
            while (e) {
                int p = e->from->id;

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
        for (int i = 0; i < block_count; i++) printf("%d: %d\n", i, idoms[i]);
    }

    function->idom = malloc(block_count * sizeof(int));
    memset(function->idom, 0, block_count * sizeof(int));

    for (int i = 0; i < block_count; i++) function->idom[i] = idoms[i];
}

// Algorithm on page 499 of engineering a compiler
// Walk the dominator tree defined by the idom (immediate dominator)s.
static void make_block_dominance_frontiers(Function *function) {
    Graph *cfg = function->cfg;
    int block_count = function->cfg->node_count;

    Set **df = malloc(block_count * sizeof(Set));
    memset(df, 0, block_count * sizeof(Set));

    for (int i = 0; i < block_count; i++) df[i] = new_set(block_count);

    int *predecessors = malloc(block_count * sizeof(int));

    int *idom = function->idom;

    for (int i = 0; i < block_count; i++) {
        int predecessor_count = 0;
        GraphEdge *e = cfg->nodes[i].pred;
        while (e) {
            predecessors[predecessor_count++] = e->from->id;
            e = e ->next_pred;
        }

        if (predecessor_count > 1) {
            for (int j = 0; j < predecessor_count; j++) {
                int p = predecessors[j];
                int runner = p;
                while (runner != idom[i] && runner != -1) {
                    add_to_set(df[runner], i);
                    runner = idom[runner];
                }
            }
        }
    }

    function->dominance_frontiers = malloc(block_count * sizeof(Set *));
    for (int i = 0; i < block_count; i++) function->dominance_frontiers[i] = df[i];

    if (debug_ssa) {
        printf("\nDominance frontiers:\n");
        for (int i = 0; i < block_count; i++) {
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
    int vreg_count = starting_count;
    for (Tac *tac = function->ir; tac; tac = tac->next) {
        if (tac->src1 && tac->src1->vreg && tac->src1->vreg > vreg_count) vreg_count = tac->src1->vreg;
        if (tac->src2 && tac->src2->vreg && tac->src2->vreg > vreg_count) vreg_count = tac->src2->vreg;
        if (tac->dst  && tac->dst ->vreg && tac->dst ->vreg > vreg_count) vreg_count = tac->dst ->vreg;
    }
    function->vreg_count = vreg_count;

    return vreg_count;
}

void make_uevar_and_varkill(Function *function) {
    Block *blocks = function->blocks;
    int block_count = function->cfg->node_count;

    function->uevar = malloc(block_count * sizeof(Set *));
    memset(function->uevar, 0, block_count * sizeof(Set *));
    function->varkill = malloc(block_count * sizeof(Set *));
    memset(function->varkill, 0, block_count * sizeof(Set *));

    int vreg_count = function->vreg_count;

    for (int i = 0; i < block_count; i++) {
        Set *uevar = new_set(vreg_count);
        Set *varkill = new_set(vreg_count);
        function->uevar[i] = uevar;
        function->varkill[i] = varkill;

        Tac *tac = blocks[i].start;
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
        for (int i = 0; i < block_count; i++) {
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
    Block *blocks = function->blocks;
    Graph *cfg = function->cfg;
    int block_count = cfg->node_count;

    function->liveout = malloc(block_count * sizeof(Set *));
    memset(function->liveout, 0, block_count * sizeof(Set *));

    int vreg_count = function->vreg_count;

    // Set all liveouts to {0}
    for (int i = 0; i < block_count; i++)
        function->liveout[i] = new_set(vreg_count);

    // Set all_vars to {0, 1, 2, ... n}, i.e. the set of all vars in a block
    Set *all_vars = new_set(vreg_count);
    char *all_vars_elements = all_vars->elements;

    for (int i = 0; i < block_count; i++) {
        Tac *tac = blocks[i].start;
        while (1) {
            if (tac->src1 && tac->src1->vreg) add_to_set(all_vars, tac->src1->vreg);
            if (tac->src2 && tac->src2->vreg) add_to_set(all_vars, tac->src2->vreg);
            if (tac->dst && tac->dst->vreg) add_to_set(all_vars, tac->dst->vreg);

            if (tac == blocks[i].end) break;
            tac = tac->next;
        }
    }

    Set *unions = new_set(vreg_count);
    char *unions_elements = unions->elements;
    Set *inv_varkill = new_set(vreg_count);
    Set *is1 = new_set(vreg_count);
    Set *is2 = new_set(vreg_count);

    if (debug_ssa_liveout) printf("Doing liveout on %d blocks\n", block_count);

    int changed = 1;
    while (changed) {
        changed = 0;

        for (int i = 0; i < block_count; i++) {
            empty_set(unions);

            GraphEdge *e = cfg->nodes[i].succ;
            while (e) {
                // Got a successor edge from i -> successor_block
                int successor_block = e->to->id;
                char *successor_block_liveout_elements = function->liveout[successor_block]->elements;
                char *successor_block_varkill_elements = function->varkill[successor_block]->elements;
                char *successor_block_uevar_elements = function->uevar[successor_block]->elements;

                for (int j = 1; j <= vreg_count; j++) {
                    unions->elements[j] =
                        successor_block_uevar_elements[j] ||                               // union
                        unions_elements[j] ||                                              // union
                        (successor_block_liveout_elements[j] &&                            // intersection
                            (all_vars_elements[j] && !successor_block_varkill_elements[j]) // difference
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
        for (int i = 0; i < block_count; i++) {
            printf("%d: ", i);
            print_set(function->liveout[i]);
            printf("\n");
        }
    }
}

// Algorithm on page 501 of engineering a compiler
void make_globals_and_var_blocks(Function *function) {
    Block *blocks = function->blocks;
    int block_count = function->cfg->node_count;

    int vreg_count = function->vreg_count;

    Set **var_blocks = malloc((vreg_count + 1) * sizeof(Set *));
    memset(var_blocks, 0, (vreg_count + 1) * sizeof(Set *));
    for (int i = 1; i <= vreg_count; i++) var_blocks[i] = new_set(block_count);

    make_vreg_count(function, 0);
    Set *globals = new_set(vreg_count);

    for (int i = 0; i < block_count; i++) {
        Set *varkill = new_set(vreg_count);

        Tac *tac = blocks[i].start;
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
        for (int i = 1; i <= vreg_count; i++) {
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
    Block *blocks = function->blocks;
    Graph *cfg = function->cfg;
    int block_count = cfg->node_count;
    Set *globals = function->globals;
    int vreg_count = function->vreg_count;

    Set **phi_functions = malloc(block_count * sizeof(Set *));
    for (int i = 0; i < block_count; i++) phi_functions[i] = new_set(vreg_count);

    for (int global = 0; global <= globals->max_value; global++) {
        if (!globals->elements[global]) continue;

        Set *work_list = copy_set(function->var_blocks[global]);

        while (set_len(work_list)) {
            for (int b = 0; b <= work_list->max_value; b++) {
                if (!work_list->elements[b]) continue;
                delete_from_set(work_list, b);

                Set *df = function->dominance_frontiers[b];
                for (int d = 0; d <= df->max_value; d++) {
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
    for (int b = 0; b < block_count; b++) {
        if (debug_ssa_phi_insertion) {
            printf("%d: ", b);
            print_set(phi_functions[b]);
            printf("\n");
        }

        int label = blocks[b].start->label;
        blocks[b].start->label = 0;

        Set *vars = phi_functions[b];
        for (int v = vreg_count; v >= 0; v--) {
            if (!vars->elements[v]) continue;

            Tac *tac = new_instruction(IR_PHI_FUNCTION);
            tac->dst  = new_value();
            tac->dst ->type = new_type(TYPE_LONG);
            tac->dst-> vreg = v;

            int predecessor_count = 0;
            GraphEdge *e = cfg->nodes[b].pred;
            while (e) { predecessor_count++; e = e->next_pred; }

            Value *phi_values = malloc((predecessor_count + 1) * sizeof(Value));
            memset(phi_values, 0, (predecessor_count + 1) * sizeof(Value));
            for (int i = 0; i < predecessor_count; i++) {
                init_value(&phi_values[i]);
                phi_values[i].type = new_type(TYPE_LONG);
                phi_values[i].vreg = v;
            }
            tac->phi_values = phi_values;

            Tac *prev = blocks[b].start->prev;
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
        print_ir(function, 0, 0);
    }
}

static int new_subscript(Stack **stack, int *counters, int n) {
    int i = counters[n]++;
    push_onto_stack(stack[n], i);

    return i;
}

static void print_stack_and_counters(Stack **stack, int *counters, int vreg_count) {
    printf("         ");
    for (int i = 1; i <= vreg_count; i++) printf("%-2d ", i);
    printf("\n");
    printf("counters ");
    for (int i = 1; i <= vreg_count; i++) printf("%-2d ", counters[i]);
    printf("\n");
    printf("stack    ");
    for (int i = 1; i <= vreg_count; i++) {
        if (stack[i]->pos == MAX_STACK_SIZE)
            printf("   ");
        else
            printf("%-2d ", stack_top(stack[i]));
    }
    printf("\n");
}

// If nothing is on the stack, push one. This is to deal with undefined
// variables being used.
static int safe_stack_top(Stack **stack, int *counters, int n) {
    if (stack[n]->pos == MAX_STACK_SIZE) new_subscript(stack, counters, n);
    return stack_top(stack[n]);
}

// Algorithm on page 506 of engineering a compiler
static void rename_vars(Function *function, Stack **stack, int *counters, int block_number, int vreg_count) {
    if (debug_ssa_phi_renumbering) {
        printf("\n----------------------------------------\nrename_vars\n");
        print_stack_and_counters(stack, counters, vreg_count);
        printf("\n");
    }

    Block *blocks = function->blocks;
    Graph *cfg = function->cfg;
    int block_count = cfg->node_count;
    int *idoms = function->idom;

    Block *b = &blocks[block_number];

    // Rewrite phi function dsts
    if (debug_ssa_phi_renumbering) printf("Rewriting phi function dsts\n");
    Tac *tac = b->start;
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
    GraphEdge *e = cfg->nodes[block_number].succ;
    while (e) {
        if (debug_ssa_phi_renumbering) printf("Successor %d\n", e->to->id);
        Tac *tac = function->blocks[e->to->id].start;
        Tac *end = function->blocks[e->to->id].end;
        while (1) {
            if (tac->operation == IR_PHI_FUNCTION) {
                Value *v = tac->phi_values;
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
    for (int i = 0; i < block_count; i++) {
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
    int vreg_count = 0;
    for (Tac *tac = function->ir; tac; tac = tac->next) {
        if (tac->dst  && tac->dst ->vreg && tac->dst ->vreg > vreg_count) vreg_count = tac->dst ->vreg;
        if (tac->src1 && tac->src1->vreg && tac->src1->vreg > vreg_count) vreg_count = tac->src1->vreg;
        if (tac->src2 && tac->src2->vreg && tac->src2->vreg > vreg_count) vreg_count = tac->src2->vreg;
    }

    int *counters = malloc((vreg_count + 1) * sizeof(int));
    memset(counters, 0, (vreg_count + 1) * sizeof(int));

    Stack **stack = malloc((vreg_count + 1) * sizeof(Stack *));
    for (int i = 1; i <= vreg_count; i++) stack[i] = new_stack();

    rename_vars(function, stack, counters, 0, vreg_count);

    if (debug_ssa_phi_renumbering) print_ir(function, 0, 0);
}

// Make mapping from (vreg, ssa_subscript) tuple to a unique index.
// The LongMap has void * values, but they are being used to store longs.
// The vars are sorted like r_1_0, r_1_1, r2_0, r3_0, r4_1 etc to preserve
// the order they were declared in. This isn't necessary for compilation, but it
// makes writing tests and debugging easier, since the live ranges will be in the same
// order.
static int make_live_range_varmap(Function *function, LongMap *map, long *reverse_map) {
    // Populate the longmap with the (vreg, ssa_subscript) tuples.
    for (Tac *tac = function->ir; tac; tac = tac->next) {
        if (tac->dst  && tac->dst ->vreg) {
            long hash = ((long) tac->dst ->vreg << 32) + tac->dst ->ssa_subscript;
            longmap_put(map, hash, (void *) 1);
        }

        if (tac->src1 && tac->src1->vreg) {
            long hash = ((long) tac->src1->vreg << 32) + tac->src1->ssa_subscript;
            longmap_put(map, hash, (void *) 1);
        }

        if (tac->src2 && tac->src2->vreg) {
            long hash = ((long) tac->src2->vreg << 32) + tac->src2->ssa_subscript;
            longmap_put(map, hash, (void *) 1);
        }

        if (tac->operation == IR_PHI_FUNCTION) {
            Value *v = tac->phi_values;
            while (v->type) {
                long hash = ((long) v->vreg << 32) + v->ssa_subscript;
                longmap_put(map, hash, (void *) 1);
                v++;
            }
        }
    }

    // Make an array with the keys
    int count = 0;
    for (LongMapIterator it = longmap_iterator(map); !longmap_iterator_finished(&it); longmap_iterator_next(&it)) count++;
    if (count == 0) return 0; // Bail if there are no vregs
    unsigned long *hashes = malloc(sizeof(long) * count);
    int i = 0;
    for (LongMapIterator it = longmap_iterator(map); !longmap_iterator_finished(&it); longmap_iterator_next(&it), i++)
        hashes[i] = longmap_iterator_key(&it);

    // Sort the array
    quicksort_ulong_array(hashes, 0, count - 1);

    // Update the values in the map and populate the reverse map
    for (int i = 0; i < count; i++) {
        unsigned long hash = hashes[i];
        longmap_put(map, hash, (void *) (long) i + 1);
        if (debug_ssa_live_range) reverse_map[i + 1] = hash;
    }

    free(hashes);

    return count + 1;
}

void make_live_ranges(Function *function) {
    if (debug_ssa_live_range) print_ir(function, 0, 0);

    LongMap *varmap = new_longmap();
    long *reverse_varmap;
    if (debug_ssa_live_range) reverse_varmap = malloc(1000000 * sizeof(long));

    int ssa_var_count = make_live_range_varmap(function, varmap, reverse_varmap);

    // Create live ranges sets for all variables, each set with the variable itself in it.
    // Allocate all the memory we need. live_range_count
    Set **live_ranges = malloc(ssa_var_count * sizeof(Set *));
    for (int i = 1; i < ssa_var_count; i++) {
        live_ranges[i] = new_set(ssa_var_count);
        add_to_set(live_ranges[i], i);
    }

    Set *s = new_set(ssa_var_count);

    int *src_ssa_vars = malloc(MAX_BLOCK_PREDECESSOR_COUNT * sizeof(int));
    int *src_set_indexes = malloc(MAX_BLOCK_PREDECESSOR_COUNT * sizeof(int));

    // Make live ranges out of SSA variables in phi functions
    for (Tac *tac = function->ir; tac; tac = tac->next) {
        if (tac->operation == IR_PHI_FUNCTION) {
            int value_count = 0;
            Value *v = tac->phi_values;
            while (v->type) {
                if (value_count == MAX_BLOCK_PREDECESSOR_COUNT) panic("Exceeded MAX_BLOCK_PREDECESSOR_COUNT");
                src_ssa_vars[value_count++] = (long) longmap_get(varmap, ((long) v->vreg << 32) + v->ssa_subscript);
                v++;
            }

            int dst = (long) longmap_get(varmap, ((long) tac->dst->vreg << 32) + tac->dst->ssa_subscript);
            int dst_set_index;

            for (int i = 1; i < ssa_var_count; i++) {
                if (live_ranges[i]->elements[dst]) dst_set_index = i;

                for (int j = 0; j < value_count; j++)
                    if (live_ranges[i]->elements[src_ssa_vars[j]]) src_set_indexes[j] = i;
            }

            Set *dst_set = live_ranges[dst_set_index];

            copy_set_to(s, dst_set);
            for (int j = 0; j < value_count; j++) {
                int set_index = src_set_indexes[j];
                set_union_to(s, s, live_ranges[set_index]);
            }
            copy_set_to(live_ranges[dst_set_index], s);

            for (int j = 0; j < value_count; j++) {
                int set_index = src_set_indexes[j];
                if (set_index != dst_set_index) empty_set(live_ranges[set_index]);
            }
        }
    }

    free_set(s);

    // Remove empty live ranges
    int live_range_count = 0;
    for (int i = 1; i < ssa_var_count; i++) {
        if (set_len(live_ranges[i]))
            live_ranges[live_range_count++] = live_ranges[i];
    }

    // From here on, live ranges start at live_range_reserved_pregs_offset + 1
    if (debug_ssa_live_range) {
        printf("Live ranges:\n");
        for (int i = 0; i < live_range_count; i++) {
            printf("%d: ", i + live_range_reserved_pregs_offset + 1);
            printf("{");
            int first = 1;
            long mask = ((1l << 32) - 1);
            for (int j = 1; j < live_ranges[i]->max_value; j++) {
                if (!live_ranges[i]->elements[j]) continue;
                if (!first)
                    printf(", ");
                else
                    first = 0;

                long hash = reverse_varmap[j];
                printf("%ld_%ld", (hash >> 32) & mask, hash & mask);
            }

            printf("}\n");
        }

        printf("\n");
        print_ir(function, 0, 0);
    }

    // Make a map of variable names to live range
    int *map = malloc(ssa_var_count * sizeof(int));
    memset(map, -1, ssa_var_count * sizeof(int));

    for (int i = 0; i < live_range_count; i++) {
        Set *s = live_ranges[i];
        char *elements = s->elements;
        for (int j = 1; j < s->max_value; j++)
            if (elements[j]) map[j] = i;
    }

    // Assign live ranges to TAC & build live_ranges set
    for (Tac *tac = function->ir; tac; tac = tac->next) {
        if (tac->dst && tac->dst->vreg)
            tac->dst->live_range = map[(long) longmap_get(varmap, ((long) tac->dst->vreg << 32) + tac->dst->ssa_subscript)] + live_range_reserved_pregs_offset + 1;

        if (tac->src1 && tac->src1->vreg)
            tac->src1->live_range = map[(long) longmap_get(varmap, ((long) tac->src1->vreg << 32) + tac->src1->ssa_subscript)] + live_range_reserved_pregs_offset + 1;

        if (tac->src2 && tac->src2->vreg)
            tac->src2->live_range = map[(long) longmap_get(varmap, ((long) tac->src2->vreg << 32) + tac->src2->ssa_subscript)] + live_range_reserved_pregs_offset + 1;
    }

    // Remove phi functions
    Block *blocks = function->blocks;
    int block_count = function->cfg->node_count;

    for (int i = 0; i < block_count; i++) {
        Tac *tac = blocks[i].start;
        while (tac->operation == IR_PHI_FUNCTION) {
            tac->next->label = tac->label;
            tac->next->prev = tac->prev;
            tac->prev->next = tac->next;
            tac = tac->next;
        }
        blocks[i].start = tac;
    }

    if (debug_ssa_live_range) print_ir(function, 0, 0);
}

// Having vreg & live_range separately isn't particularly useful, since most
// downstream code traditionally deals with vregs. So do a vreg=live_range
// for all values
void blast_vregs_with_live_ranges(Function *function) {
    for (Tac *tac = function->ir; tac; tac = tac->next) {
        if (tac->src1 && tac->src1->vreg) tac->src1->vreg = tac->src1->live_range;
        if (tac->src2 && tac->src2->vreg) tac->src2->vreg = tac->src2->live_range;
        if (tac->dst  && tac->dst-> vreg) tac->dst-> vreg = tac->dst-> live_range;
    }

    // Nuke the live ranges, they should not be used downstream and this guarantees a horrible failure if they are.
    // Also, remove ssa_subscript, since this is no longer used. Mostly so that print_ir
    // doesn't show them.
    for (Tac *tac = function->ir; tac; tac = tac->next) {
        if (tac->src1 && tac->src1->vreg) { tac->src1->live_range = -100000; tac->src1->ssa_subscript = -1; }
        if (tac->src2 && tac->src2->vreg) { tac->src2->live_range = -100000; tac->src2->ssa_subscript = -1; }
        if (tac->dst  && tac->dst-> vreg) { tac->dst-> live_range = -100000; tac->dst-> ssa_subscript = -1; }
    }
}

// Set preg_class (PC_INT or PC_SSE) for all vregs in the IR by looking at the type
// The first 28 values are for the available physical registers,
// 1-12 is for integers, 13-28 for floating point values.
static void set_preg_classes(Function *function) {
    char *vreg_preg_classes = malloc(sizeof(char) * (function->vreg_count + 1));
    memset(vreg_preg_classes, 0, sizeof(char) * (function->vreg_count + 1));

    if (live_range_reserved_pregs_offset > 0) {
        for (int i = 1; i <= PHYSICAL_SSE_REGISTER_COUNT; i++) vreg_preg_classes[i] = PC_INT;
        for (int i = PHYSICAL_SSE_REGISTER_COUNT + 1; i <= PHYSICAL_INT_REGISTER_COUNT + PHYSICAL_SSE_REGISTER_COUNT; i++) vreg_preg_classes[i] = PC_SSE;
    }

    function->vreg_preg_classes = vreg_preg_classes;

    for (Tac *tac = function->ir; tac; tac = tac->next) {
        // Ensure nothing in the TAC that has a vreg has a type. If so, it's a bug.
        if (tac->dst  && tac->dst ->vreg && !tac->dst ->type) { print_instruction(stdout, tac, 0); panic("Type is unexpectedly zero on dst "); }
        if (tac->src1 && tac->src1->vreg && !tac->src1->type) { print_instruction(stdout, tac, 0); panic("Type is unexpectedly zero on src1"); }
        if (tac->src2 && tac->src2->vreg && !tac->src2->type) { print_instruction(stdout, tac, 0); panic("Type is unexpectedly zero on src2"); }

        if (tac->dst  && tac->dst->vreg) {
            tac->dst->preg_class = is_sse_floating_point_type(tac->dst->type) ? PC_SSE : PC_INT;
            vreg_preg_classes[tac->dst->vreg] = tac->dst->preg_class;
        }
        if (tac->src1 && tac->src1->vreg) {
            tac->src1->preg_class = is_sse_floating_point_type(tac->src1->type) ? PC_SSE : PC_INT;
            vreg_preg_classes[tac->src1->vreg] = tac->src1->preg_class;
        }
        if (tac->src2 && tac->src2->vreg) {
            tac->src2->preg_class = is_sse_floating_point_type(tac->src2->type) ? PC_SSE : PC_INT;
            vreg_preg_classes[tac->src2->vreg] = tac->src2->preg_class;
        }
    }
}

void add_ig_edge(char *ig, int vreg_count, int to, int from) {
    int index;

    if (debug_ssa_interference_graph) printf("Adding edge %d <-> %d\n", to, from);

    if (from > to)
        index = to * vreg_count + from;
    else
        index = from * vreg_count + to;

    if (!ig[index]) ig[index] = 1;
}

// Add edges to a physical register for all live variables, preventing the physical register from
// getting used.
static void clobber_livenow(char *ig, int vreg_count, Set *livenow, Tac *tac, int preg_reg_index) {
    if (debug_ssa_interference_graph) printf("Clobbering livenow for pri=%d\n", preg_reg_index);

    int max_value = livenow->max_value;
    char *elements = livenow->elements;

    for (int i = 0; i <= max_value; i++) {
        if (elements[i])
            add_ig_edge(ig, vreg_count, preg_reg_index, i);
    }
}

// Add edges to a physical register for all live variables and all values in an instruction
static void clobber_tac_and_livenow(char *ig, int vreg_count, Set *livenow, Tac *tac, int preg_reg_index) {
    if (debug_ssa_interference_graph) printf("Adding edges for pri=%d\n", preg_reg_index);

    clobber_livenow(ig, vreg_count, livenow, tac, preg_reg_index);

    if (tac->dst  && tac->dst ->vreg) add_ig_edge(ig, vreg_count, preg_reg_index, tac->dst->vreg );
    if (tac->src1 && tac->src1->vreg) add_ig_edge(ig, vreg_count, preg_reg_index, tac->src1->vreg);
    if (tac->src2 && tac->src2->vreg) add_ig_edge(ig, vreg_count, preg_reg_index, tac->src2->vreg);
}

static void print_physical_register_name_for_lr_reg_index(int preg_reg_index) {
    switch(preg_reg_index) {
        case LIVE_RANGE_PREG_RAX_INDEX:         printf("rax");   break;
        case LIVE_RANGE_PREG_RBX_INDEX:         printf("rbx");   break;
        case LIVE_RANGE_PREG_RCX_INDEX:         printf("rcx");   break;
        case LIVE_RANGE_PREG_RDX_INDEX:         printf("rdx");   break;
        case LIVE_RANGE_PREG_RSI_INDEX:         printf("rsi");   break;
        case LIVE_RANGE_PREG_RDI_INDEX:         printf("rdi");   break;
        case LIVE_RANGE_PREG_R08_INDEX:         printf("r8");    break;
        case LIVE_RANGE_PREG_R09_INDEX:         printf("r9");    break;
        case LIVE_RANGE_PREG_R12_INDEX:         printf("r12");   break;
        case LIVE_RANGE_PREG_R13_INDEX:         printf("r13");   break;
        case LIVE_RANGE_PREG_R14_INDEX:         printf("r14");   break;
        case LIVE_RANGE_PREG_R15_INDEX:         printf("r15");   break;
        case LIVE_RANGE_PREG_XMM00_INDEX:       printf("xmm0");  break;
        case LIVE_RANGE_PREG_XMM00_INDEX + 1:   printf("xmm1");  break;
        case LIVE_RANGE_PREG_XMM00_INDEX + 2:   printf("xmm2");  break;
        case LIVE_RANGE_PREG_XMM00_INDEX + 3:   printf("xmm3");  break;
        case LIVE_RANGE_PREG_XMM00_INDEX + 4:   printf("xmm4");  break;
        case LIVE_RANGE_PREG_XMM00_INDEX + 5:   printf("xmm5");  break;
        case LIVE_RANGE_PREG_XMM00_INDEX + 6:   printf("xmm6");  break;
        case LIVE_RANGE_PREG_XMM00_INDEX + 7:   printf("xmm7");  break;
        case LIVE_RANGE_PREG_XMM00_INDEX + 8:   printf("xmm8");  break;
        case LIVE_RANGE_PREG_XMM00_INDEX + 9:   printf("xmm9");  break;
        case LIVE_RANGE_PREG_XMM00_INDEX + 10:  printf("xmm10"); break;
        case LIVE_RANGE_PREG_XMM00_INDEX + 11:  printf("xmm11"); break;
        case LIVE_RANGE_PREG_XMM00_INDEX + 12:  printf("xmm12"); break;
        case LIVE_RANGE_PREG_XMM00_INDEX + 13:  printf("xmm13"); break;
        default: printf("Unknown LR preg index %d", preg_reg_index);
    }
}

// Force a physical register to be assigned to vreg by the graph coloring by adding edges to all other pregs
static void force_physical_register(char *ig, int vreg_count, Set *livenow, int vreg, int preg_reg_index, int preg_class) {
    if (debug_ssa_interference_graph || debug_register_allocation) {
        printf("Forcing ");
        print_physical_register_name_for_lr_reg_index(preg_reg_index);
        printf(" onto vreg %d\n", vreg);
    }

    for (int i = 0; i <= livenow->max_value; i++)
        if (i != vreg && livenow->elements[i])
            add_ig_edge(ig, vreg_count, preg_reg_index, i);

    // Add edges to all non reserved physical registers
    int start = preg_class == PC_INT ? 1 : PHYSICAL_INT_REGISTER_COUNT + 1;
    int size = preg_class == PC_INT ? PHYSICAL_INT_REGISTER_COUNT : PHYSICAL_SSE_REGISTER_COUNT;
    for (int i = start; i < start + size; i++)
        if (preg_reg_index != i) add_ig_edge(ig, vreg_count, vreg, i);
}

static void enforce_live_range_preg_for_preg(char *interference_graph, int vreg_count, Set *livenow, Value *value, int preg_class, int *arg_registers) {
    if (value && value && value->preg_class == preg_class && value->live_range_preg)
        force_physical_register(interference_graph, vreg_count, livenow, value->vreg, value->live_range_preg, preg_class);
}

// For values that have live_range_preg set, add interference graph edges for all live ranges except live_range_preg
static void enforce_live_range_preg(char *interference_graph, int vreg_count, Set *livenow, Value *value) {
    enforce_live_range_preg_for_preg(interference_graph, vreg_count, livenow, value, PC_INT, int_arg_registers);
    enforce_live_range_preg_for_preg(interference_graph, vreg_count, livenow, value, PC_SSE, sse_arg_registers);
}

static void print_interference_graph(Function *function) {
    char *interference_graph = function->interference_graph;
    int vreg_count = function->vreg_count;

    for (int from = 1; from <= vreg_count; from++) {
        int from_offset = from * vreg_count;
        for (int to = from + 1; to <= vreg_count; to++) {
            if (interference_graph[from_offset + to])
                printf("%-4d    %d\n", to, from);
        }
    }
}

// Page 701 of engineering a compiler
void make_interference_graph(Function *function, int include_clobbers) {
    if (debug_ssa_interference_graph) {
        printf("Make interference graph\n");
        printf("--------------------------------------------------------\n");
        print_ir(function, 0, 0);
    }

    int vreg_count = function->vreg_count;
    char *vreg_preg_classes = function->vreg_preg_classes;

    char *interference_graph = malloc((vreg_count + 1) * (vreg_count + 1) * sizeof(int));
    memset(interference_graph, 0, (vreg_count + 1) * (vreg_count + 1) * sizeof(int));

    Block *blocks = function->blocks;
    int block_count = function->cfg->node_count;

    for (int i = block_count - 1; i >= 0; i--) {
        Set *livenow = copy_set(function->liveout[i]);
        int livenow_max_value = livenow->max_value;
        char *livenow_elements = livenow->elements;

        Tac *tac = blocks[i].end;
        while (tac) {
            if (debug_ssa_interference_graph) print_instruction(stdout, tac, 0);

            enforce_live_range_preg(interference_graph, vreg_count, livenow, tac->dst);
            enforce_live_range_preg(interference_graph, vreg_count, livenow, tac->src1);
            enforce_live_range_preg(interference_graph, vreg_count, livenow, tac->src2);

            if (include_clobbers && tac->operation == IR_CALL || tac->operation == X_CALL) {
                // Integer arguments are clobbered
                for (int j = 0; j < 6; j++) {
                    if (j == 2) continue; // RDX is a special case, see below
                    clobber_livenow(interference_graph, vreg_count, livenow, tac, int_arg_registers[j]);
                }

                // Unless the function returns something in rax, clobber rax
                if (!tac->src1->return_value_live_ranges || !in_set(tac->src1->return_value_live_ranges, LIVE_RANGE_PREG_RAX_INDEX))
                    clobber_livenow(interference_graph, vreg_count, livenow, tac, LIVE_RANGE_PREG_RAX_INDEX);

                // Unless the function returns something in rdx, clobber rdx
                if (!tac->src1->return_value_live_ranges || !in_set(tac->src1->return_value_live_ranges, LIVE_RANGE_PREG_RDX_INDEX))
                    clobber_livenow(interference_graph, vreg_count, livenow, tac, LIVE_RANGE_PREG_RDX_INDEX);

                // All SSE registers xmm2, xmm3, ... are clobbered
                for (int j = 2; j < PHYSICAL_SSE_REGISTER_COUNT; j++)
                    clobber_livenow(interference_graph, vreg_count, livenow, tac, LIVE_RANGE_PREG_XMM00_INDEX + j);

                // Unless the function returns something in xmm0, clobber xmm0
                if (!tac->src1->return_value_live_ranges || !in_set(tac->src1->return_value_live_ranges, LIVE_RANGE_PREG_XMM00_INDEX))
                    clobber_livenow(interference_graph, vreg_count, livenow, tac, LIVE_RANGE_PREG_XMM00_INDEX);
                // Unless the function returns something in xmm1, clobber xmm1
                if (!tac->src1->return_value_live_ranges || !in_set(tac->src1->return_value_live_ranges, LIVE_RANGE_PREG_XMM01_INDEX))
                    clobber_livenow(interference_graph, vreg_count, livenow, tac, LIVE_RANGE_PREG_XMM01_INDEX);
            }

            if (tac->operation == IR_DIV || tac->operation == IR_MOD || tac->operation == X_IDIV) {
                if (include_clobbers) {
                    clobber_tac_and_livenow(interference_graph, vreg_count, livenow, tac, LIVE_RANGE_PREG_RAX_INDEX);
                    clobber_tac_and_livenow(interference_graph, vreg_count, livenow, tac, LIVE_RANGE_PREG_RDX_INDEX);
                }

                // Works together with the instruction rules
                if (tac->operation == X_IDIV)
                    add_ig_edge(interference_graph, vreg_count, tac->src2->vreg, tac->dst->vreg);
            }

            if (include_clobbers && tac->operation == IR_BSHL || tac->operation == IR_BSHR) {
                clobber_tac_and_livenow(interference_graph, vreg_count, livenow, tac, LIVE_RANGE_PREG_RCX_INDEX);
            }

            // Works together with the instruction rules. Ensure the shift value cannot be in rcx.
            if (include_clobbers && (tac->operation == X_SHR) && tac->prev->dst && tac->prev->dst->vreg && tac->prev->src1 && tac->prev->src1->vreg) {
                clobber_tac_and_livenow(interference_graph, vreg_count, livenow, tac, LIVE_RANGE_PREG_RCX_INDEX);
                add_ig_edge(interference_graph, vreg_count, tac->prev->dst->vreg, LIVE_RANGE_PREG_RCX_INDEX);
                add_ig_edge(interference_graph, vreg_count, tac->prev->src1->vreg, LIVE_RANGE_PREG_RCX_INDEX);
            }

            if (include_clobbers && tac->operation == X_LD_EQ_CMP)
                clobber_tac_and_livenow(interference_graph, vreg_count, livenow, tac, LIVE_RANGE_PREG_RDX_INDEX);

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

                for (int j = 0; j <= livenow_max_value; j++) {
                    if (!livenow_elements[j]) continue;

                    if (j == tac->dst->vreg) continue; // Ignore self assignment
                    if (tac->dst->preg_class != vreg_preg_classes[j]) continue;

                    // Don't add an edge for register copies
                    if ((
                        tac->operation == IR_MOVE ||
                        tac->operation == X_MOV ||
                        tac->operation == X_MOVZ ||
                        tac->operation == X_MOVS ||
                        tac->operation == X_MOVC
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
        printf("Interference graph:\n");
        print_interference_graph(function);
    }
}

static void copy_interference_graph_edges(char *interference_graph, int vreg_count, int src, int dst) {
    // Copy all edges in the lower triangular interference graph matrics from src to dst
    // Fun with lower triangular matrices follows ...
    for (int i = 1; i <= vreg_count; i++) {
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
static void coalesce_live_range(Function *function, int src, int dst, int check_register_constraints) {
    int vreg_count = function->vreg_count;

    // Rewrite IR src => dst
    for (Tac *tac = function->ir; tac; tac = tac->next) {
        if (tac->dst  && tac->dst ->vreg == src) tac->dst ->vreg = dst;
        if (tac->src1 && tac->src1->vreg == src) tac->src1->vreg = dst;
        if (tac->src2 && tac->src2->vreg == src) tac->src2->vreg = dst;

        if (tac->operation == IR_MOVE && tac->dst && tac->dst->vreg && tac->src1 && tac->src1->vreg && tac->dst->vreg == tac->src1->vreg) {
            tac->operation = IR_NOP;
            tac->dst = 0;
            tac->src1 = 0;
            tac->src2 = 0;
        }

        if (check_register_constraints) {
            // Sanity check for instrsel, ensure dst != src1, dst != src2 and src1 != src2
            if (tac->dst && tac->dst->vreg && tac->src1 && tac->src1->vreg && tac->dst->vreg == tac->src1->vreg) {
                print_instruction(stdout, tac, 0);
                panic("Illegal violation of dst != src1 (%d), required by instrsel", tac->dst->vreg);
            }
            if (tac->dst && tac->dst->vreg && tac->src2 && tac->src2->vreg && tac->dst->vreg == tac->src2->vreg) {
                print_instruction(stdout, tac, 0);
                panic("Illegal violation of dst != src2 (%d) , required by instrsel", tac->dst->vreg);
            }
            if (tac->src1 && tac->src1->vreg && tac->src2 && tac->src2->vreg && tac->src1->vreg == tac->src2->vreg) {
                print_instruction(stdout, tac, 0);
                panic("Illegal violation of src1 != src2 (%d) , required by instrsel", tac->src1->vreg);
            }
        }
    }

    copy_interference_graph_edges(function->interference_graph, vreg_count, src, dst);

    // Migrate liveouts
    int block_count = function->cfg->node_count;
    for (int i = 0; i < block_count; i++) {
        Set *l = function->liveout[i];
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
static void coalesce_live_ranges_for_preg(Function *function, int check_register_constraints, int preg_class) {
    int vreg_count = function->vreg_count;
    char *merge_candidates = malloc((vreg_count + 1) * (vreg_count + 1) * sizeof(char));
    char *instrsel_blockers = malloc((vreg_count + 1) * (vreg_count + 1) * sizeof(char));
    char *clobbers = malloc((vreg_count + 1) * sizeof(char));

    int outer_changed = 1;
    while (outer_changed) {
        outer_changed = 0;

        make_live_range_spill_cost(function);
        make_interference_graph(function, 0);

        if (!opt_enable_live_range_coalescing) return;

        char *interference_graph = function->interference_graph;

        int inner_changed = 1;
        int max_cost;
        int merge_src, merge_dst;
        while (inner_changed) {
            inner_changed = max_cost = merge_src = merge_dst = 0;

            // A lower triangular matrix of all register copy operations and instrsel blockers
            memset(instrsel_blockers, 0, (vreg_count + 1) * (vreg_count + 1) * sizeof(char));
            memset(clobbers, 0, (vreg_count + 1) * sizeof(char));

            LongMap *mc = new_longmap();

            // Create merge candidates
            for (Tac *tac = function->ir; tac; tac = tac->next) {
                // Don't coalesce a move if the type doesn't match
                if (tac->operation == IR_MOVE && tac->dst->vreg && tac->src1->vreg && tac->dst->preg_class == preg_class && type_eq(tac->src1->type, tac->dst->type))
                    longmap_put(mc, ((long) tac->dst->vreg << 32) + tac->src1->vreg, (void *) 1l);

                else if (tac->operation == X_MOV && tac->dst && tac->dst->vreg && tac->dst->preg_class == preg_class && tac->src1 && tac->src1->vreg && tac->src1->preg_class == preg_class && tac->next) {
                    if ((tac->next->operation == X_ADD || tac->next->operation == X_SUB || tac->next->operation == X_MUL) && tac->next->src2 && tac->next->src2->vreg)
                        longmap_put(mc, ((long) tac->dst->vreg << 32) + tac->src1->vreg, (void *) 1l);

                    if ((tac->next->operation == X_SHR) && tac->next->src1 && tac->next->src1->vreg)
                        longmap_put(mc, ((long) tac->dst->vreg << 32) + tac->src1->vreg, (void *) 1l);
                }

                else {
                    if (tac-> dst && tac-> dst->vreg && tac->src1 && tac->src1->vreg) { instrsel_blockers[tac-> dst->vreg * vreg_count + tac->src1->vreg] = 1; }
                    if (tac-> dst && tac-> dst->vreg && tac->src2 && tac->src2->vreg) { instrsel_blockers[tac-> dst->vreg * vreg_count + tac->src2->vreg] = 1; }
                    if (tac->src1 && tac->src1->vreg && tac->src2 && tac->src2->vreg) { instrsel_blockers[tac->src1->vreg * vreg_count + tac->src2->vreg] = 1; }
                }

                if (tac->dst  && tac->dst-> vreg && tac->dst ->live_range_preg) clobbers[tac->dst ->vreg] = 1;
                if (tac->src1 && tac->src1->vreg && tac->src1->live_range_preg) clobbers[tac->src1->vreg] = 1;
                if (tac->src2 && tac->src2->vreg && tac->src2->live_range_preg) clobbers[tac->src2->vreg] = 1;
            }

            if (debug_ssa_live_range_coalescing) {
                print_ir(function, 0, 0);
                printf("Live range coalesces:\n");
            }

            long mask = ((1l << 32) - 1);
            for (LongMapIterator it = longmap_iterator(mc); !longmap_iterator_finished(&it); longmap_iterator_next(&it)) {
                long hash = longmap_iterator_key(&it);
                int dst = (hash >> 32) & mask;
                int src = hash & mask;

                if (clobbers[src] || clobbers[dst]) continue;

                int l1 = dst * vreg_count + src;
                int l2 = src * vreg_count + dst;

                if (instrsel_blockers[l1] || instrsel_blockers[l2]) continue;
                if (interference_graph[l1] || interference_graph[l2]) continue;

                int cost = function->spill_cost[src] + function->spill_cost[dst];
                if (cost > max_cost) {
                    max_cost = cost;
                    merge_src = src;
                    merge_dst = dst;
                }
            }

            if (merge_src) {
                if (debug_ssa_live_range_coalescing) printf("Coalescing %d -> %d\n", merge_src, merge_dst);
                coalesce_live_range(function, merge_src, merge_dst, check_register_constraints);
                inner_changed = 1;
                outer_changed = 1;
            }
        }  // while inner changed
    } // while outer changed

    free(merge_candidates);
    free(instrsel_blockers);
    free(clobbers);

    if (debug_ssa_live_range_coalescing) {
        printf("\nLive range coalesce results for preg_class=%d:\n", preg_class);
        print_ir(function, 0, 0);
    }
}

void coalesce_live_ranges(Function *function, int check_register_constraints) {
    make_vreg_count(function, live_range_reserved_pregs_offset);
    make_uevar_and_varkill(function);
    make_liveout(function);
    make_preferred_live_range_preg_indexes(function);
    set_preg_classes(function);

    coalesce_live_ranges_for_preg(function, check_register_constraints, PC_INT);
    coalesce_live_ranges_for_preg(function, check_register_constraints, PC_SSE);
}

// 10^p
static int ten_power(int p) {
    int result = 1;
    for (int i = 0; i < p; i++) result = result * 10;

    return result;
}

// Page 699 of engineering a compiler
// The algorithm is incomplete though. It only looks ad adjacent instructions
// That have a store and a load. A more general version should check for
// any live ranges that have no other live ranges ending between their store
// and loads.
static void add_infinite_spill_costs(Function *function) {
    int *spill_cost = function->spill_cost;
    Block *blocks = function->blocks;
    int block_count = function->cfg->node_count;

    for (int i = block_count - 1; i >= 0; i--) {
        Set *livenow = copy_set(function->liveout[i]);

        Tac *tac = blocks[i].end;
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

static void make_live_range_spill_cost(Function *function) {
    int vreg_count = function->vreg_count;
    int *spill_cost = malloc((vreg_count + 1) * sizeof(int));
    memset(spill_cost, 0, (vreg_count + 1) * sizeof(int));

    int for_loop_depth = 0;
    for (Tac *tac = function->ir; tac; tac = tac->next) {
        if (tac->operation == IR_START_LOOP) for_loop_depth++;
        if (tac->operation == IR_END_LOOP) for_loop_depth--;

        if (tac->dst  && tac->dst ->vreg) spill_cost[tac->dst ->vreg] += ten_power(for_loop_depth);
        if (tac->src1 && tac->src1->vreg) spill_cost[tac->src1->vreg] += ten_power(for_loop_depth);
        if (tac->src2 && tac->src2->vreg) spill_cost[tac->src2->vreg] += ten_power(for_loop_depth);
    }

    function->spill_cost = spill_cost;

    if (opt_short_lr_infinite_spill_costs) add_infinite_spill_costs(function);

    if (debug_ssa_spill_cost) {
        printf("Spill costs:\n");
        for (int i = 1; i <= vreg_count; i++)
            printf("%d: %d\n", i, spill_cost[i]);

    }
}

void make_preferred_live_range_preg_indexes(Function *function) {
    int vreg_count = function->vreg_count;

    char *preferred_live_range_preg_indexes = malloc((vreg_count + 1) * sizeof(char));
    memset(preferred_live_range_preg_indexes, 0, (vreg_count + 1) * sizeof(char));

    for (Tac *tac = function->ir; tac; tac = tac->next) {
        if (tac->src1 && tac->src1->preferred_live_range_preg_index)
            preferred_live_range_preg_indexes[tac->src1->vreg] = tac->src1->preferred_live_range_preg_index;
    }

    function->preferred_live_range_preg_indexes = preferred_live_range_preg_indexes;
}
