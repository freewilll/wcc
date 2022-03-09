#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "wcc.h"

static void make_live_range_spill_cost(Function *function);

typedef struct live_range {
    LongSet *set;
    long weight;
} LiveRange;

typedef struct coalesce {
    int
    src, dst, cost;
} Coalesce;

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

        else if (tac->src1->type->is_unsigned && (c & (c - 1)) == 0) {
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
    Block *blocks = wcalloc(MAX_BLOCKS, sizeof(Block));
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

                // Keep IR_DECL_LOCAL_COMP_OBJ, otherwise bad things will happen in the
                // stack offset allocation in codegen.
                if (tac->operation != IR_DECL_LOCAL_COMP_OBJ) {
                    tac->operation = IR_NOP;
                    tac->dst = 0;
                    tac->src1 = 0;
                    tac->src2 = 0;
                }
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

void free_control_flow_graph(Function *function) {
    free(function->blocks);
    free_graph(function->cfg);
}

// Algorithm from page 503 of Engineering a compiler
void make_block_dominance(Function *function) {
    Graph *cfg = function->cfg;
    int block_count = cfg->node_count;

    Set **dom = wcalloc(block_count, sizeof(Set));

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

    function->dominance = wcalloc(block_count, sizeof(Set));

    for (int i = 0; i < block_count; i++) function->dominance[i] = dom[i];

    if (debug_ssa_dominance) {
        printf("\nDominance:\n");
        for (int i = 0; i < block_count; i++) {
            printf("%d: ", i);
            print_set(dom[i]);
            printf("\n");
        }
    }
}

void free_block_dominance(Function *function) {
    free(function->dominance);
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

    int *rpos = wcalloc(block_count, sizeof(int));

    int *visited = wcalloc(block_count, sizeof(int));

    int pos = block_count - 1;
    make_rpo(function, rpos, &pos, visited, 0);

    int *rpos_order = wcalloc(block_count, sizeof(int));
    for (int i = 0; i < block_count; i++) rpos_order[rpos[i]] = i;

    int *idoms = wcalloc(block_count, sizeof(int));

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

    if (debug_ssa_idom) {
        printf("\nIdoms:\n");
        for (int i = 0; i < block_count; i++) printf("%d: %d\n", i, idoms[i]);
    }

    function->idom = wcalloc(block_count, sizeof(int));
    for (int i = 0; i < block_count; i++) function->idom[i] = idoms[i];

    free(rpos);
    free(visited);
}

static void free_block_immediate_dominators(Function *function) {
    free(function->idom);
}

// Algorithm on page 499 of engineering a compiler
// Walk the dominator tree defined by the idom (immediate dominator)s.
static void make_block_dominance_frontiers(Function *function) {
    Graph *cfg = function->cfg;
    int block_count = function->cfg->node_count;

    Set **df = wcalloc(block_count, sizeof(Set *));
    for (int i = 0; i < block_count; i++) df[i] = new_set(block_count);

    int *predecessors = wmalloc(block_count * sizeof(int));

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

    function->dominance_frontiers = wmalloc(block_count * sizeof(Set *));
    for (int i = 0; i < block_count; i++) function->dominance_frontiers[i] = df[i];

    if (debug_ssa_dominance_frontiers) {
        printf("\nDominance frontiers:\n");
        for (int i = 0; i < block_count; i++) {
            printf("%d: ", i);
            print_set(df[i]);
            printf("\n");
        }
    }

    free(predecessors);
}

static void free_block_dominance_frontiers(Function *function) {
    int block_count = function->cfg->node_count;
    for (int i = 0; i < block_count; i++) free_set(function->dominance_frontiers[i]);
    free(function->dominance_frontiers);
}

void analyze_dominance(Function *function) {
    sanity_test_ir_linkage(function);
    make_vreg_count(function, 0);
    make_control_flow_graph(function);
    make_block_dominance(function);
    make_block_immediate_dominators(function);
    make_block_dominance_frontiers(function);
}

void free_dominance(Function *function) {
    free_block_dominance_frontiers(function);
    free_block_immediate_dominators(function);
    free_block_dominance(function);
    free_control_flow_graph(function);
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

    function->uevar = wcalloc(block_count, sizeof(LongSet *));
    function->varkill = wcalloc(block_count, sizeof(LongSet *));

    for (int i = 0; i < block_count; i++) {
        LongSet *uevar = new_longset();
        LongSet *varkill = new_longset();
        function->uevar[i] = uevar;
        function->varkill[i] = varkill;

        Tac *tac = blocks[i].start;
        while (1) {
            if (tac->src1 && tac->src1->vreg && !longset_in(varkill, tac->src1->vreg)) longset_add(uevar, tac->src1->vreg);
            if (tac->src2 && tac->src2->vreg && !longset_in(varkill, tac->src2->vreg)) longset_add(uevar, tac->src2->vreg);
            if (tac->dst && tac->dst->vreg) longset_add(varkill, tac->dst->vreg);

            if (tac == blocks[i].end) break;
            tac = tac->next;
        }
    }

    if (debug_ssa) {
        printf("\nuevar & varkills:\n");
        for (int i = 0; i < block_count; i++) {
            printf("%d: uevar=", i);
            print_longset(function->uevar[i]);
            printf(" varkill=");
            print_longset(function->varkill[i]);
            printf("\n");
        }
    }
}

// Page 447 of Engineering a compiler
void make_liveout(Function *function) {
    Graph *cfg = function->cfg;
    int block_count = cfg->node_count;

    function->liveout = wcalloc(block_count, sizeof(LongSet *));

    make_vreg_count(function, 0);
    int vreg_count = function->vreg_count;
    int **block_uevars = wmalloc(block_count * sizeof(int *));

    // Keep track of versions of liveouts. Everytime a block liveout changes, the
    // version number is incremented. This is used to avoid recalculating the liveouts
    // repeatedly.
    int *liveout_versions = wcalloc(block_count, sizeof(int));

    int **successor_liveout_versions = wcalloc(block_count, sizeof(int *));

    for (int i = 0; i < block_count; i++) {
        block_uevars[i] = wcalloc(vreg_count + 1, sizeof(int));

        successor_liveout_versions[i] = wcalloc(block_count, sizeof(int));

        liveout_versions[i] = 1;
        int j = 0;
        int *bue = block_uevars[i];
        LongSet *uevar = function->uevar[i];
        for (LongSetIterator it = longset_iterator(uevar); !longset_iterator_finished(&it); longset_iterator_next(&it), j++)
            bue[j] = longset_iterator_element(&it);
    }

    // Set all liveouts to {0}
    for (int i = 0; i < block_count; i++)
        function->liveout[i] = new_longset();

    if (debug_ssa_liveout) printf("Doing liveout on %d blocks\n", block_count);

    LongSet *unions = new_longset();

    int inner_count = 0;
    int changed = 1;
    while (changed) {
        changed = 0;

        for (int i = 0; i < block_count; i++) {
            int *block_successor_liveout_versions = successor_liveout_versions[i];

            // Check to see if any of the successor liveouts have changed. If they
            // haven't, the entire calculation can be skipped for this block.
            int successor = 0;
            int match = 1;
            GraphEdge *e = cfg->nodes[i].succ;
            while (e) {
                int successor_block = e->to->id;

                if (block_successor_liveout_versions[successor] != liveout_versions[successor_block]) {
                    match = 0;
                    break;
                }

                e = e->next_succ;
                successor++;
            }

            if (match) continue;

            successor = 0;
            e = cfg->nodes[i].succ;
            while (e) {
                inner_count++;
                // Got a successor edge from i -> successor_block
                int successor_block = e->to->id;
                LongSet *successor_block_liveout = function->liveout[successor_block];
                LongSet *successor_block_varkill = function->varkill[successor_block];
                int *successor_block_uevars = block_uevars[successor_block];
                block_successor_liveout_versions[successor] = liveout_versions[successor_block];

                for (int *i = successor_block_uevars; *i; i++)
                    longset_add(unions, *i);

                longset_foreach(successor_block_liveout, it) {
                    long vreg = longset_iterator_element(&it);
                    if (!longset_in(successor_block_varkill, vreg))
                        longset_add(unions, vreg);

                }

                e = e->next_succ;
                successor++;
            }
            block_successor_liveout_versions[successor] = 0;

            // Ensure unions has been emptied before the next iteration
            if (!longset_eq(function->liveout[i], unions)) {
                liveout_versions[i]++;
                free_longset(function->liveout[i]);
                function->liveout[i] = unions;
                unions = new_longset();
                changed = 1;
            }
            else
                longset_empty(unions);
        }
    }

    free_longset(unions);

    if (debug_ssa_liveout) {
        printf("\nLiveouts:\n");
        for (int i = 0; i < block_count; i++) {
            printf("%d: ", i);
            print_longset(function->liveout[i]);
            printf("\n");
        }
    }

    for (int i = 0; i < block_count; i++) {
        free(block_uevars[i]);
        free(successor_liveout_versions[i]);
    }
    free(block_uevars);

    free(liveout_versions);
    free(successor_liveout_versions);
}

// Algorithm on page 501 of engineering a compiler
void make_globals_and_var_blocks(Function *function) {
    Block *blocks = function->blocks;
    int block_count = function->cfg->node_count;

    int vreg_count = function->vreg_count;

    Set **var_blocks = wcalloc(vreg_count + 1, sizeof(Set *));
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

void free_globals_and_var_blocks(Function *function) {
    int vreg_count = function->vreg_count;

    for (int i = 1; i <= vreg_count; i++) free_set(function->var_blocks[i]);
    free(function->var_blocks);

    free_set(function->globals);
}

// Algorithm on page 501 of engineering a compiler
void insert_phi_functions(Function *function) {
    Block *blocks = function->blocks;
    Graph *cfg = function->cfg;
    int block_count = cfg->node_count;
    Set *globals = function->globals;
    int vreg_count = function->vreg_count;

    Set **phi_functions = wmalloc(block_count * sizeof(Set *));
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

            Value *phi_values = wcalloc(predecessor_count + 1, sizeof(Value));
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

    int *counters = wcalloc(vreg_count + 1, sizeof(int));

    Stack **stack = wmalloc((vreg_count + 1) * sizeof(Stack *));
    for (int i = 1; i <= vreg_count; i++) stack[i] = new_stack();

    rename_vars(function, stack, counters, 0, vreg_count);

    if (debug_ssa_phi_renumbering) print_ir(function, 0, 0);

    for (int i = 1; i <= vreg_count; i++) free_stack(stack[i]);
    free(stack);
}

static int live_range_cmpfunc(const void *a, const void *b) {
    long diff = ((LiveRange *) a)->weight - ((LiveRange *) b)->weight;
    if (diff < 0) return -1;
    else if (diff > 0) return 1;
    else return 0;
}

static long live_range_hash(long l) {
    return 3 * l;
}

#define LR_HASH(vreg, ssa_subscript) ssa_subscript * (vreg_count + 1) + vreg
#define LR_HASH_SSA_SUBSCRIPT(hash) hash / (vreg_count + 1)
#define LR_HASH_VREG(hash) hash % (vreg_count + 1)

#define LR_ADD_TO_MAP(live_ranges, v) if (v && v->vreg) longmap_put(live_ranges, LR_HASH(v->vreg, v->ssa_subscript), (void *) 1)

#define LR_ASSIGN(final_map, v) if (v && v->vreg) { \
    long live_range = (long) longmap_get(final_map, (long) (LR_HASH(v->vreg, v->ssa_subscript))); \
    if (!live_range) panic("Missing live range for %d_%d", v->vreg, v->ssa_subscript); \
    v->live_range = (long) longmap_get(final_map, (long) (LR_HASH(v->vreg, v->ssa_subscript))); \
}

// Move out of SSA by converting all SSA variables to live ranges and removing phi
// functions. The algorithm on described in page 696 of engineering a compiler. First,
// live ranges are created for all SSA variables including phi function destinations
// and phi function parameters. Then, each phi function is examined. The sets of the
// dst and sets of the parameters are unioned to make a new set. The old sets are no
// longer relevant are get assigned the new set, which is later deduped. Once the
// deduping is done, the final sets are sorted, so that the vreg order is more or less
// preserved.
void make_live_ranges(Function *function) {
    if (debug_ssa_live_range) print_ir(function, 0, 0);

    LongMap *live_ranges = new_longmap();
    live_ranges->hashfunc = live_range_hash;

    make_vreg_count(function, 0);
    int vreg_count = function->vreg_count;

    // Make initial live ranges. Each SSA variable has a live range with itself in the set.

    // Collect all SSA vars and put them in the live_ranges map
    for (Tac *tac = function->ir; tac; tac = tac->next) {
        LR_ADD_TO_MAP(live_ranges, tac->dst);
        LR_ADD_TO_MAP(live_ranges, tac->src1);
        LR_ADD_TO_MAP(live_ranges, tac->src2);

        if (tac->operation == IR_PHI_FUNCTION)
            for (Value *v = tac->phi_values; v->type; v++) LR_ADD_TO_MAP(live_ranges, v);
    }

    // Create the initial sets
    int *keys;
    int keys_count = longmap_keys(live_ranges, &keys);

    for (int i = 0; i < keys_count; i++) {
        long key = keys[i];
        LongSet *s = new_longset();
        longset_add(s, key);
        longmap_put(live_ranges, key, s);
    }

    free(keys);

    // Create live range sets by unioning together the sets of the phi function destination and parameters.
    for (Tac *tac = function->ir; tac; tac = tac->next) {
        if (tac->operation != IR_PHI_FUNCTION) continue;

        LongSet *dst = longmap_get(live_ranges, LR_HASH(tac->dst->vreg, tac->dst->ssa_subscript));

        for (Value *v = tac->phi_values; v->type; v++) {
            long hash = LR_HASH(v->vreg, v->ssa_subscript);
            LongSet *src_set = longmap_get(live_ranges, hash);

            // Add all elements of the phi function parameter set to the dst
            longset_foreach(src_set, it) {
                long src = longset_iterator_element(&it);
                // Only add to the set if different to avoid an iteration changed panic
                if (dst != src_set) longset_add(dst, src);

                // Only put if the map is different to avoid an iteration changed panic
                if (src != hash) longmap_put(live_ranges, src, dst);
            }
        }

        // Make all phi function params the same set
        for (Value *v = tac->phi_values; v->type; v++) {
            longmap_put(live_ranges, LR_HASH(v->vreg, v->ssa_subscript), dst);
        }
    }

    // Dedupe live ranges by putting the pointers to the sets in a set
    LongSet *deduped = new_longset();
    longmap_foreach(live_ranges, it) {
        long key = longmap_iterator_key(&it);
        LongSet *s = longmap_get(live_ranges, key);
        longset_add(deduped, (long) s);
    }

    // Sort live ranges by first element of the live ranges set, vreg first, then
    // ssa_subscript. The sorting isn't necessary, but makes debugging & tests easier,
    // since the live ranges end up being ordered in the same way as the original
    // vregs.
    int live_range_count = 0;
    longset_foreach(deduped, it) live_range_count++;

    LiveRange *live_ranges_array = wmalloc(live_range_count * sizeof(LiveRange));
    int i = 0;
    for (LongSetIterator it = longset_iterator(deduped); !longset_iterator_finished(&it); longset_iterator_next(&it), i++) {
        // Find vreg/ssa_subscript for first element of the live range set
        LongSet *set = (LongSet *) longset_iterator_element(&it);
        LongMapIterator lrit = longmap_iterator(set->longmap);
        long hash = (long) longmap_iterator_key(&lrit);
        if (!hash) panic("Expected non-zero elements in live range set");

        LiveRange lr = { set, (LR_HASH_VREG(hash) << 32) + LR_HASH_SSA_SUBSCRIPT(hash) };
        live_ranges_array[i] = lr;
    }

    qsort(live_ranges_array, live_range_count, sizeof(LiveRange), live_range_cmpfunc);

    if (debug_ssa_live_range) printf("\nLive ranges:\n");
    LongMap *final_map = new_longmap();
    for (int live_range = 0; live_range < live_range_count; live_range++) {
        LongSet *s = live_ranges_array[live_range].set;

        int first = 1;
        longmap_foreach(s->longmap, it) {
            if (debug_ssa_live_range) {
                if (first) printf("%d: {", live_range + live_range_reserved_pregs_offset + 1); else printf(", ");
            }

            first = 0;
            long key = longmap_iterator_key(&it);
            if (debug_ssa_live_range) printf("%ld_%ld", LR_HASH_VREG(key), LR_HASH_SSA_SUBSCRIPT(key));
            longmap_put(final_map, key, (void *) (long) live_range + live_range_reserved_pregs_offset + 1);
        }
        if (debug_ssa_live_range) printf("}\n");
    }

    for (Tac *tac = function->ir; tac; tac = tac->next) {
        if (tac->operation == IR_PHI_FUNCTION) continue;

        LR_ASSIGN(final_map, tac->dst);
        LR_ASSIGN(final_map, tac->src1);
        LR_ASSIGN(final_map, tac->src2);
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
    int count = function->vreg_count;
    if (count < PHYSICAL_INT_REGISTER_COUNT + PHYSICAL_SSE_REGISTER_COUNT) count = PHYSICAL_INT_REGISTER_COUNT + PHYSICAL_SSE_REGISTER_COUNT;
    char *vreg_preg_classes = wcalloc(count + 1, sizeof(char));

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

    ig[index] |= 1;
}

// Add edges to a physical register for all live variables, preventing the physical register from
// getting used.
static void clobber_livenow(char *ig, int vreg_count, LongSet *livenow, Tac *tac, int preg_reg_index) {
    if (debug_ssa_interference_graph) printf("Clobbering livenow for pri=%d\n", preg_reg_index);

    longset_foreach(livenow, it)
        add_ig_edge(ig, vreg_count, preg_reg_index, longset_iterator_element(&it));
}

// Add edges to a physical register for all live variables and all values in an instruction
static void clobber_tac_and_livenow(char *ig, int vreg_count, LongSet *livenow, Tac *tac, int preg_reg_index) {
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
static void force_physical_register(char *ig, int vreg_count, LongSet *livenow, int vreg, int preg_reg_index, int preg_class) {
    if (debug_ssa_interference_graph || debug_register_allocation) {
        printf("Forcing ");
        print_physical_register_name_for_lr_reg_index(preg_reg_index);
        printf(" onto vreg %d\n", vreg);
    }

    longset_foreach(livenow, it) {
        int it_vreg = longset_iterator_element(&it);
        if (it_vreg != vreg) add_ig_edge(ig, vreg_count, preg_reg_index, it_vreg);
    }

    // Add edges to all non reserved physical registers
    int start = preg_class == PC_INT ? 1 : PHYSICAL_INT_REGISTER_COUNT + 1;
    int size = preg_class == PC_INT ? PHYSICAL_INT_REGISTER_COUNT : PHYSICAL_SSE_REGISTER_COUNT;
    for (int i = start; i < start + size; i++)
        if (preg_reg_index != i) add_ig_edge(ig, vreg_count, vreg, i);
}

static void enforce_live_range_preg_for_preg(char *interference_graph, int vreg_count, LongSet *livenow, Value *value, int preg_class, int *arg_registers) {
    if (value && value && value->preg_class == preg_class && value->live_range_preg)
        force_physical_register(interference_graph, vreg_count, livenow, value->vreg, value->live_range_preg, preg_class);
}

// For values that have live_range_preg set, add interference graph edges for all live ranges except live_range_preg
static void enforce_live_range_preg(char *interference_graph, int vreg_count, LongSet *livenow, Value *value) {
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
// If include_clobbers is set, then edges for any IR_* instruction that adds register constraints are skipped.
// If include_instrsel_constraints is set, then constraints are added that precent coalescing of registers
// to ensure instruction selection gets an IR it can deal with.
void make_interference_graph(Function *function, int include_clobbers, int include_instrsel_constraints) {
    if (debug_ssa_interference_graph) {
        printf("Make interference graph\n");
        printf("--------------------------------------------------------\n");
        print_ir(function, 0, 0);
    }

    int vreg_count = function->vreg_count;
    char *vreg_preg_classes = function->vreg_preg_classes;

    char *interference_graph = wcalloc((vreg_count + 1) * (vreg_count + 1), sizeof(int));

    Block *blocks = function->blocks;
    int block_count = function->cfg->node_count;

    for (int i = block_count - 1; i >= 0; i--) {
        LongSet *livenow = longset_copy(function->liveout[i]);

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

                // If it's a function call from a pointer in a vreg, ensure it doesn't reside in RAX
                if (tac->src1->vreg)
                    add_ig_edge(interference_graph, vreg_count, LIVE_RANGE_PREG_RAX_INDEX, tac->src1->vreg);
            }

            if (tac->operation == IR_DIV || tac->operation == IR_MOD || tac->operation == X_IDIV) {
                if (include_clobbers) {
                    clobber_tac_and_livenow(interference_graph, vreg_count, livenow, tac, LIVE_RANGE_PREG_RAX_INDEX);
                    clobber_tac_and_livenow(interference_graph, vreg_count, livenow, tac, LIVE_RANGE_PREG_RDX_INDEX);
                }
            }

            if (include_clobbers && tac->operation == IR_BSHL || tac->operation == IR_BSHR) {
                clobber_tac_and_livenow(interference_graph, vreg_count, livenow, tac, LIVE_RANGE_PREG_RCX_INDEX);
            }

            // Works together with the instruction rules. Ensure the shift value cannot be in rcx.
            if (tac->operation == X_SHR && tac->prev->dst && tac->prev->dst->vreg && tac->prev->src1 && tac->prev->src1->vreg) {
                clobber_tac_and_livenow(interference_graph, vreg_count, livenow, tac, LIVE_RANGE_PREG_RCX_INDEX);
                add_ig_edge(interference_graph, vreg_count, tac->prev->dst->vreg, LIVE_RANGE_PREG_RCX_INDEX);
                add_ig_edge(interference_graph, vreg_count, tac->prev->src1->vreg, LIVE_RANGE_PREG_RCX_INDEX);
            }

            if (tac->operation == X_LD_EQ_CMP)
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

                longset_foreach(livenow, it) {
                    int it_vreg = longset_iterator_element(&it);

                    if (it_vreg == tac->dst->vreg) continue; // Ignore self assignment
                    if (tac->dst->preg_class != vreg_preg_classes[it_vreg]) continue;

                    // Don't add an edge for register copies
                    if ((
                        tac->operation == IR_MOVE ||
                        tac->operation == X_MOV ||
                        tac->operation == X_MOVZ ||
                        tac->operation == X_MOVS ||
                        tac->operation == X_MOVC
                       ) && tac->src1 && tac->src1->vreg && tac->src1->vreg == it_vreg) continue;
                    add_ig_edge(interference_graph, vreg_count, tac->dst->vreg, it_vreg);
                    if (debug_ssa_interference_graph) printf("added dst <-> lr %d <-> %d\n", tac->dst->vreg, it_vreg);
                }
            }

            if (include_instrsel_constraints && tac->operation != IR_MOVE) {
                // Add edges necessary for instruction selection constraints:
                // dst != src1, dst != src2, src1 != src2,
                if (tac-> dst && tac-> dst->vreg && tac->src1 && tac->src1->vreg) add_ig_edge(interference_graph, vreg_count, tac-> dst->vreg, tac->src1->vreg);
                if (tac-> dst && tac-> dst->vreg && tac->src2 && tac->src2->vreg) add_ig_edge(interference_graph, vreg_count, tac-> dst->vreg, tac->src2->vreg);
                if (tac->src1 && tac->src1->vreg && tac->src2 && tac->src2->vreg) add_ig_edge(interference_graph, vreg_count, tac->src1->vreg, tac->src2->vreg);
            }

            if (tac->dst && tac->dst->vreg) {
                if (debug_ssa_interference_graph)
                    printf("livenow: -= %d -> ", tac->dst->vreg);
                longset_delete(livenow, tac->dst->vreg);
                if (debug_ssa_interference_graph) { print_longset(livenow); printf("\n"); }
            }

            if (tac->src1 && tac->src1->vreg) {
                if (debug_ssa_interference_graph)
                    printf("livenow: += (src1) %d -> ", tac->src1->vreg);
                longset_add(livenow, tac->src1->vreg);
                if (debug_ssa_interference_graph) { print_longset(livenow); printf("\n"); }
            }

            if (tac->src2 && tac->src2->vreg) {
                if (debug_ssa_interference_graph)
                    printf("livenow: += (src2) %d -> ", tac->src2->vreg);
                longset_add(livenow, tac->src2->vreg);
                if (debug_ssa_interference_graph) { print_longset(livenow); printf("\n"); }
            }

            if (tac == blocks[i].start) break;
            tac = tac->prev;
        }

        free_longset(livenow);
    }

    function->interference_graph = interference_graph;

    if (debug_ssa_interference_graph) {
        printf("Interference graph:\n");
        print_interference_graph(function);
    }
}

void free_interference_graph(Function *function) {
    if (function->interference_graph) {
        free(function->interference_graph);
        function->interference_graph = 0;
    }
}

static void copy_interference_graph_edges(char *interference_graph, int vreg_count, int src, int dst) {
    // Copy all edges in the lower triangular interference graph matrics from src to dst

    // Precalculate whatever is possible
    int dtvc = dst * vreg_count;
    int stvc = src * vreg_count;
    int itvc = vreg_count;

    // Fun with lower triangular matrices follows ...
    for (int i = 1; i <= vreg_count; i++) {
        // src < i case
        if (interference_graph[stvc + i]) {
            if (dst < i)
                interference_graph[dtvc + i] = 1;
            else
                interference_graph[itvc + dst] = 1;
        }

        // i < src case
        if (interference_graph[itvc + src]) {
            if (i < dst)
                interference_graph[itvc + dst] = 1;
            else
                interference_graph[dtvc + i] = 1;
        }

        itvc += vreg_count;
    }
}

#define move_coalesced_vreg(coalesces, v) { \
    if (v && v->vreg) { \
        int coalesce_dst = (long) longmap_get(coalesces, (long) v->vreg); \
        if (coalesce_dst) v->vreg = coalesce_dst; \
    } \
}

// Rewrite the IR, renaming all registers in the coalesces map. Then, upgade the interference graph and liveouts.
static void coalesce_pending_coalesces(Function *function, LongMap *coalesces, int check_register_constraints) {
    // Rewrite IR src => dst if present in the coalesces map
    for (Tac *tac = function->ir; tac; tac = tac->next) {
        move_coalesced_vreg(coalesces, tac->dst);
        move_coalesced_vreg(coalesces, tac->src1);
        move_coalesced_vreg(coalesces, tac->src2);

        // Create IR_NOP for the move instruction
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

    // Migrate liveouts
    longmap_foreach(coalesces, it) {
        int src = longmap_iterator_key(&it);
        int dst = (long) longmap_get(coalesces, src);

        int block_count = function->cfg->node_count;
        for (int i = 0; i < block_count; i++) {
            LongSet *l = function->liveout[i];
            if (longset_in(l, src)) {
                longset_delete(l, src);
                longset_add(l, dst);
            }
        }
    }
}

// Merge together the upper and lower 32 bits, otherwise there will be lost of
// hash collisions on the upper half.
long merge_candidates_hash(long l) {
    return 3 * (l >> 32) + 5 * (l & 31);
}

static int coalesce_cmpfunc(const void *a, const void *b) {
    return ((Coalesce *) b)->cost - ((Coalesce *) a)->cost;
}

// Page 706 of engineering a compiler
// A loop (re)calculates the interference graph.
//
// Inside the loop
// - Make live range candidates to be merged with a single pass over the IR
// - Sort the candidates by cost
// - Loop over all candidates, selecting the ones that are possible and keep track
// - of pending live ranges, while updating the interference graph, insrsel constraints and clobbers.
// - Rewrite the code
//
// The update can introduce false interferences, however they will disappear when the
// loop runs again. Since earlier coalesces can lead to later coalesces not
// happening, with each inner loop, the registers with the highest spill cost are
// coalesced.
static void coalesce_live_ranges_for_preg(Function *function, int check_register_constraints, int preg_class) {
    int vreg_count = function->vreg_count;
    char *clobbers = wmalloc((vreg_count + 1) * sizeof(char));

    int outer_changed = 1;
    while (outer_changed) {
        outer_changed = 0;

        make_live_range_spill_cost(function);
        free_interference_graph(function);
        make_interference_graph(function, 0, 1);

        char *interference_graph = function->interference_graph;

        // A lower triangular matrix of all register copy operations and instrsel blockers
        memset(clobbers, 0, (vreg_count + 1) * sizeof(char));

        // Make make of merge candidates
        LongMap *mc = new_longmap();
        mc->hashfunc = merge_candidates_hash;

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

            if (tac->dst  && tac->dst-> vreg && tac->dst ->live_range_preg) clobbers[tac->dst ->vreg] = 1;
            if (tac->src1 && tac->src1->vreg && tac->src1->live_range_preg) clobbers[tac->src1->vreg] = 1;
            if (tac->src2 && tac->src2->vreg && tac->src2->live_range_preg) clobbers[tac->src2->vreg] = 1;
        }

        if (debug_ssa_live_range_coalescing) {
            print_ir(function, 0, 0);
            printf("Live range coalesces:\n");
        }

        // Allocate memory for the worst case: all vregs are coalesced
        Coalesce *coalesces = wmalloc((vreg_count + 1) * sizeof(Coalesce));
        int coalesce_count = 0;

        long mask = ((1l << 32) - 1);
        longmap_foreach(mc, it) {
            long hash = longmap_iterator_key(&it);
            int dst = (hash >> 32) & mask;
            int src = hash & mask;

            coalesces[coalesce_count].src = src;
            coalesces[coalesce_count].dst = dst;
            coalesces[coalesce_count].cost = function->spill_cost[src] + function->spill_cost[dst];
            coalesce_count++;
        }

        qsort(coalesces, coalesce_count, sizeof(Coalesce), coalesce_cmpfunc);

        LongMap *pending_coalesces = new_longmap();
        for (int i = 0; i < coalesce_count; i++) {
            int src = (long) coalesces[i].src;
            int dst = (long) coalesces[i].dst;

            int coalesced_src = (long) longmap_get(pending_coalesces, src);
            if (coalesced_src) src = coalesced_src;

            int coalesced_dst = (long) longmap_get(pending_coalesces, dst);
            if (coalesced_dst) dst = coalesced_dst;

            if (clobbers[src] || clobbers[dst]) continue;

            int l1 = dst * vreg_count + src;
            int l2 = src * vreg_count + dst;

            if (interference_graph[l1] || interference_graph[l2]) continue;

            // Collect done_srcs in the pending_coalesces since we will used to modify the
            // map.
            int *done_srcs;
            int done_srcs_count = longmap_keys(pending_coalesces, &done_srcs);

            // Update all dsts
            for (int i = 0; i < done_srcs_count; i++) {
                int done_src = done_srcs[i];
                int done_dst = (long) longmap_get(pending_coalesces, done_src);

                if (done_dst == src)
                    longmap_put(pending_coalesces, (long) done_src, (void *) (long) dst);
            }
            free(done_srcs);

            // Update constraints, moving all from src -> dst
            copy_interference_graph_edges(function->interference_graph, vreg_count, src, dst);

            clobbers[dst] |= clobbers[src];

            // Add to the done coalesces
            if (debug_ssa_live_range_coalescing) printf("  %d -> %d\n", src, dst);
            longmap_put(pending_coalesces, (long) src, (void *) (long) dst);
            outer_changed = 1;
        }

        coalesce_pending_coalesces(function, pending_coalesces, check_register_constraints);

        free(coalesces);
        free_longmap(pending_coalesces);
    } // while outer changed

    free(clobbers);

    if (debug_ssa_live_range_coalescing) {
        printf("\nLive range coalesce results for preg_class=%d:\n", preg_class);
        print_ir(function, 0, 0);
    }
}

void coalesce_live_ranges(Function *function, int check_register_constraints) {
    make_vreg_count(function, live_range_reserved_pregs_offset);
    if (log_compiler_phase_durations) debug_log("Make uevar and varkill");
    make_uevar_and_varkill(function);
    if (log_compiler_phase_durations) debug_log("Make liveout");
    make_liveout(function);
    if (log_compiler_phase_durations) debug_log("Make preferred LR indexes");
    make_preferred_live_range_preg_indexes(function);
    set_preg_classes(function);

    if (!opt_enable_live_range_coalescing) {
        make_live_range_spill_cost(function);
        make_interference_graph(function, 0, 0);

        return;
    }

    if (log_compiler_phase_durations) debug_log("Coalesce live ranges for int");
    coalesce_live_ranges_for_preg(function, check_register_constraints, PC_INT);

    if (log_compiler_phase_durations) debug_log("Coalesce live ranges for SSE");
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
        LongSet *livenow = longset_copy(function->liveout[i]);

        Tac *tac = blocks[i].end;
        while (tac) {
            if (tac->prev && tac->prev->dst && tac->prev->dst->vreg) {
                if ((tac->src1 && tac->src1->vreg && tac->src1->vreg == tac->prev->dst->vreg) ||
                    (tac->src2 && tac->src2->vreg && tac->src2->vreg == tac->prev->dst->vreg)) {

                    if (!longset_in(livenow, tac->prev->dst->vreg))
                        spill_cost[tac->prev->dst->vreg] = 2 << 15;
                }
            }
            if (tac->dst && tac->dst->vreg) longset_delete(livenow, tac->dst->vreg);
            if (tac->src1 && tac->src1->vreg) longset_add(livenow, tac->src1->vreg);
            if (tac->src2 && tac->src2->vreg) longset_add(livenow, tac->src2->vreg);

            if (tac == blocks[i].start) break;
            tac = tac->prev;
        }

        free_longset(livenow);
    }
}

static void make_live_range_spill_cost(Function *function) {
    int vreg_count = function->vreg_count;
    int *spill_cost = wcalloc(vreg_count + 1, sizeof(int));

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

    char *preferred_live_range_preg_indexes = wcalloc(vreg_count + 1, sizeof(char));

    for (Tac *tac = function->ir; tac; tac = tac->next) {
        if (tac->src1 && tac->src1->preferred_live_range_preg_index)
            preferred_live_range_preg_indexes[tac->src1->vreg] = tac->src1->preferred_live_range_preg_index;
    }

    function->preferred_live_range_preg_indexes = preferred_live_range_preg_indexes;
}
