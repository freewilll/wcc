#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "wcc.h"

enum {
    MAX_INSTRUCTION_GRAPH_EDGE_COUNT = 64,
    MAX_INSTRUCTION_GRAPH_CHOICE_NODE_COUNT = 1024,
    MAX_INSTRUCTION_GRAPH_CHOICE_EDGE_COUNT = 1024,
    MAX_CHOICE_TRAIL_COUNT = 32,
    MAX_SAVED_REGISTERS = 8,
};

IGraph *igraphs;                // The current block's igraphs
int instr_count;                // The current block's instruction count

Graph *cost_graph;              // Graph of all possible options when tiling
int cost_graph_node_count;      // Number of nodes in cost graph
int *cost_to_igraph_map;        // Mapping of a cost graph node back to the instruction graph node
int *cost_rules;                // Mapping of cost graph node id to x86 instruction rule id
int *accumulated_cost;          // Total cost of sub tree of a cost graph node
Set **igraph_labels;            // Matched instruction rule ids for a igraph node id
Rule **igraph_rules;            // Matched lowest cost rule id for a igraph node id

static int recursive_tile_igraphs(IGraph *igraph, int node_id);

static void transform_lvalues(Function *function) {
    for (Tac *tac = function->ir; tac; tac = tac->next) {
        if (tac->operation == IR_MOVE_TO_PTR) {
            tac->src1 = dup_value(tac->src1);
            tac->src1->type = make_pointer(tac->src1->type);
            tac->src1->is_lvalue = 0;
            // Note: tac ->dst remains zero. src1 is the target of the pointer write, but is itself not modified
        }
        else {
            // Ensure type of dst and src1 matches in a pointer addition operation
            if (tac->operation == IR_ADD && tac->dst && tac->dst->is_lvalue_in_register) {
                tac->dst = dup_value(tac->dst);
                tac->dst->type = make_pointer(tac->dst->type);
                tac->dst->is_lvalue = 0;
            }


            if (tac->dst && tac->dst->vreg && tac->dst->is_lvalue && !tac->dst->is_lvalue_in_register) {
                tac->dst->type = make_pointer(tac->dst->type);
                tac->dst->is_lvalue = 0;
            }

            if (tac->src1 && tac->src1->vreg && tac->src1->is_lvalue) {
                tac->src1->type = make_pointer(tac->src1->type);
                tac->src1->is_lvalue = 0;
            }

            if (tac->src2 && tac->src2->vreg && tac->src2->is_lvalue) {
                tac->src2->type = make_pointer(tac->src2->type);
                tac->src2->is_lvalue = 0;
            }
        }
    }
}

static void recursive_dump_igraph(IGraph *ig, int node, int indent, int include_rules) {
    int c = indent * 2;
    IGraphNode *ign = &(ig->nodes[node]);

    printf("%3d ", node);
    for (int i = 0; i < indent; i++) printf("  ");

    if (ign->tac) {
        int operation = ign->tac->operation;
             if (operation == IR_MOVE)                 c += printf("=");
        else if (operation == IR_MOVE_PREG_CLASS)      c += printf("move to preg class");
        else if (operation == IR_MOVE_STACK_PTR)       c += printf("stack ptr to");
        else if (operation == IR_DECL_LOCAL_COMP_OBJ)  c += printf("declare");
        else if (operation == IR_LOAD_BIT_FIELD)       c += printf("load bit field");
        else if (operation == IR_SAVE_BIT_FIELD)       c += printf("save bit field");
        else if (operation == IR_ADD)                  c += printf("+");
        else if (operation == IR_SUB)                  c += printf("-");
        else if (operation == IR_MUL)                  c += printf("*");
        else if (operation == IR_DIV)                  c += printf("/");
        else if (operation == IR_BNOT)                 c += printf("~");
        else if (operation == IR_BSHL)                 c += printf("<<");
        else if (operation == IR_BSHR)                 c += printf(">>");
        else if (operation == IR_ASHR)                 c += printf("a>>");
        else if (operation == IR_BNOT)                 c += printf("!");
        else if (operation == IR_BOR)                  c += printf("|");
        else if (operation == IR_BAND)                 c += printf("&");
        else if (operation == IR_XOR)                  c += printf("~");
        else if (operation == IR_INDIRECT)             c += printf("indirect");
        else if (operation == IR_ADDRESS_OF)           c += printf("&");
        else if (operation == IR_MOVE_TO_PTR)          c += printf("move to ptr");
        else if (operation == IR_NOP)                  c += printf("noop");
        else if (operation == IR_RETURN)               c += printf("return");
        else if (operation == IR_LOAD_LONG_DOUBLE)     c += printf("load long double");
        else if (operation == IR_START_CALL)           c += printf("start call");
        else if (operation == IR_END_CALL)             c += printf("end call");
        else if (operation == IR_ARG)                  c += printf("arg");
        else if (operation == IR_ARG_STACK_PADDING)    c += printf("arg stack padding");
        else if (operation == IR_CALL)                 c += printf("call");
        else if (operation == IR_CALL_ARG_REG)         c += printf("call arg reg");
        else if (operation == IR_START_LOOP)           c += printf("start loop");
        else if (operation == IR_END_LOOP)             c += printf("end loop");
        else if (operation == IR_ALLOCATE_STACK)       c += printf("allocate stack");
        else if (operation == IR_JZ)                   c += printf("jz");
        else if (operation == IR_JNZ)                  c += printf("jnz");
        else if (operation == IR_JMP)                  c += printf("jmp");
        else if (operation == IR_EQ)                   c += printf("==");
        else if (operation == IR_NE)                   c += printf("!=");
        else if (operation == IR_LT)                   c += printf("<");
        else if (operation == IR_GT)                   c += printf(">");
        else if (operation == IR_LE)                   c += printf(">=");
        else if (operation == IR_GE)                   c += printf("<=");

        else
            c += printf("Operation %d", operation);

        if (ign->tac->dst) {
            c += printf(" -> ");
            c += print_value(stdout, ign->tac->dst, 0);
        }
    }
    else
        c += print_value(stdout, ign->value, 0);

    if (include_rules && igraph_rules[node]) {
        if (igraph_rules[node]) {
            for (int i = 0; i < 60 - c; i++) printf(" ");
            print_rule(igraph_rules[node], 0, 0);
        }
    }
    else
        printf("\n");

    GraphEdge *e = ig->graph->nodes[node].succ;
    while (e) {
        recursive_dump_igraph(ig, e->to->id, indent + 1, include_rules);
        e = e->next_succ;
    }
}

static void dump_igraph(IGraph *ig, int include_rules) {
    if (ig->node_count == 0) printf("Empty igraph\n");
    recursive_dump_igraph(ig, 0, 0, include_rules);
}

static void dup_inode(IGraphNode *src, IGraphNode *dst) {
    dst->tac = src->tac;
    dst->value = src->value;
}

static IGraph *shallow_dup_igraph(IGraph *src, IGraph *dst) {
    dst->nodes = src->nodes;
    dst->graph = src->graph;
    dst->node_count = src->node_count;
}

// Merge g2 into g1. The merge point is vreg
static IGraph *merge_igraphs(IGraph *g1, IGraph *g2, int vreg) {
    if (debug_instsel_tree_merging) {
        printf("g1 dst=%d\n", g1->nodes[0].tac->dst ? g1->nodes[0].tac->dst->vreg : -1);
        dump_igraph(g1, 0);
        printf("g2 dst=%d\n", g2->nodes[0].tac->dst ? g2->nodes[0].tac->dst->vreg : -1);
        dump_igraph(g2, 0);
        printf("\n");
    }

    IGraph *g = malloc(sizeof(IGraph));
    memset(g, 0, sizeof(IGraph));

    int node_count = g1->node_count + g2->node_count;
    IGraphNode *inodes = malloc(node_count * sizeof(IGraphNode));
    memset(inodes, 0, node_count * sizeof(IGraphNode));

    g->nodes = inodes;
    g->graph = new_graph(node_count, MAX_INSTRUCTION_GRAPH_EDGE_COUNT);
    g->node_count = node_count;
    int join_from = -1;
    int join_to = -1;
    IGraphNode *in1;

    if (g1->node_count == 0) panic("Unexpectedly got 0 g1->node_count");

    if (debug_instsel_tree_merging_deep) printf("g1->node_count=%d\n", g1->node_count);
    if (debug_instsel_tree_merging_deep) printf("g2->node_count=%d\n", g2->node_count);

    for (int i = 0; i < g1->node_count; i++) {
        if (debug_instsel_tree_merging_deep) printf("Copying g1 %d to %d\n", i, i);
        dup_inode(&(g1->nodes[i]), &(inodes[i]));

        IGraphNode *g1_inodes = g1->nodes;
        GraphNode *n = &(g1->graph->nodes[i]);

        GraphEdge *e = n->succ;

        while (e) {
            IGraphNode *in = &(g1_inodes[e->to->id]);
            if (in->value && in->value->vreg == vreg) {
                in1 = in;
                join_from = e->from->id;
                join_to = e->to->id;
                if (debug_instsel_tree_merging_deep) printf("Adding join edge %d -> %d\n", join_from, join_to);
                add_graph_edge(g->graph, join_from, join_to);
            }
            else {
                if (debug_instsel_tree_merging_deep) printf("Adding g1 edge %d -> %d\n", e->from->id, e->to->id);
                add_graph_edge(g->graph, e->from->id, e->to->id);
            }

            e = e->next_succ;
        }
    }

    if (join_from == -1 || join_to == -1) panic("Attempt to join two trees without a join node");

    // The g2 graph starts at g1->node_count
    for (int i = 0; i < g2->node_count; i++) {
        int d = g1->node_count + i;
        if (debug_instsel_tree_merging_deep) printf("Copying g2 node from %d to %d\n", i, d);
        dup_inode(&(g2->nodes[i]), &(inodes[d]));

        GraphNode *n2 = &(g2->graph->nodes[i]);
        GraphEdge *e = n2->succ;
        while (e) {
            int from = e->from->id + g1->node_count ;
            int to = e->to->id + g1->node_count;
            if (debug_instsel_tree_merging_deep) printf("Adding g2 edge %d -> %d\n", from, to);
            add_graph_edge(g->graph, from, to);

            e = e->next_succ;
        }
    }

    if (debug_instsel_tree_merging_deep) printf("\n");

    IGraphNode *in2 = &(g2->nodes[0]);

    // The coalesce live ranges code already removed any possible IR_MOVE, including
    // coercions. This means that any type change left must be made explicit with an
    // IR_MOVE, so that suitable instruction selection rules are matched, leading to
    // possible code generation.
    if (!type_eq(in1->value->type, in2->tac->dst->type)) {
        if (debug_instsel_tree_merging) {
            printf("Replacing %d with IR_MOVE tac for a type change from ", join_to);
            print_type(stdout, in2->tac->dst->type);
            printf(" -> ");
            print_type(stdout, in1->value->type);
            printf("\n");
        }

        IGraphNode *in = &(inodes[join_to]);
        in->tac = new_instruction(IR_MOVE);
        in->tac->src1 = in2->tac->dst;
        in->tac->dst = in1->value;

        if (debug_instsel_tree_merging) print_instruction(stdout, in->tac, 0);

        if (debug_instsel_tree_merging) printf("Adding inter graph join edge %d -> %d\n", join_to, g1->node_count);
        add_graph_edge(g->graph, join_to, g1->node_count);
    }
    else {
        if (debug_instsel_tree_merging_deep) printf("Repointing inter graph join node %d->%d to %d->%d\n", join_from, join_to, join_from, g1->node_count);
        GraphEdge *e = g->graph->nodes[join_from].succ;
        if (e->to->id != join_to) e = e->next_succ;
        e->to = &(g->graph->nodes[g1->node_count]);
    }

    if (debug_instsel_tree_merging) {
        printf("\ng:\n");
        dump_igraph(g, 0);
    }

    return g;
}

static int igraphs_are_neighbors(IGraph *igraphs, int i1, int i2) {
    i1++;
    if (i1 > i2) panic("Internal error: ordering issue in igraphs_are_neighbors()");

    while (i1 <= i2) {
        if (i1 == i2) return 1;
        int is_nop = igraphs[i1].node_count == 1 && igraphs[i1].nodes[0].tac->operation == IR_NOP;
        if (!is_nop && igraphs[i1].node_count != 0) return 0;
        i1++;
    }

    return 1;
}

static void make_igraphs(Function *function, int block_id) {
    Block *blocks = function->blocks;

    int vreg_count = 0;
    instr_count = 0;

    Tac *tac = blocks[block_id].start;
    while (1) {
        if (debug_instsel_tree_merging) print_instruction(stdout, tac, 0);

        if (tac->src1 && tac->src1->vreg && tac->src1->vreg > vreg_count) vreg_count = tac->src1->vreg;
        if (tac->src2 && tac->src2->vreg && tac->src2->vreg > vreg_count) vreg_count = tac->src2->vreg;
        if (tac-> dst && tac->dst ->vreg && tac->dst ->vreg > vreg_count) vreg_count = tac->dst ->vreg;

        instr_count++;

        if (tac == blocks[block_id].end) break;
        tac = tac->next;
    }

    igraphs = malloc(instr_count * sizeof(IGraph));

    int i = 0;
    tac = blocks[block_id].start;
    while (tac) {
        int node_count = 1;

        if (tac->src1) node_count++;
        if (tac->src2) node_count++;

        IGraphNode *nodes = malloc(node_count * sizeof(IGraphNode));
        memset(nodes, 0, node_count * sizeof(IGraphNode));

        Graph *graph = new_graph(node_count, MAX_INSTRUCTION_GRAPH_EDGE_COUNT);

        nodes[0].tac = tac;
        node_count = 1;

        if (tac->src1) { nodes[node_count].value = tac->src1; add_graph_edge(graph, 0, node_count); node_count++; }
        if (tac->src2) { nodes[node_count].value = tac->src2; add_graph_edge(graph, 0, node_count); node_count++; }

        igraphs[i].graph = graph;
        igraphs[i].nodes = nodes;
        igraphs[i].node_count = node_count;

        i++;

        if (tac == blocks[block_id].end) break;
        tac = tac->next;
    }

    // Mark liveouts as off-limits for merging
    Set *liveout = function->liveout[block_id];

    if (liveout->max_value > vreg_count) vreg_count = liveout->max_value;

    VregIGraph* vreg_igraphs = malloc((vreg_count + 1) * sizeof(VregIGraph));
    memset(vreg_igraphs, 0, (vreg_count + 1) * sizeof(VregIGraph));

    for (int i = 0; i <= liveout->max_value; i++) {
        if (!liveout->elements[i]) continue;
        vreg_igraphs[i].count++;
        vreg_igraphs[i].igraph_id = -1;
    }

    i = instr_count - 1;
    tac = blocks[block_id].end;
    while (tac) {
        int dst = 0;
        int src1 = 0;
        int src2 = 0;

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

        if (tac->operation != IR_MOVE_TO_PTR && dst && ((src1 && dst == src1) || (src2 && dst == src2))) {
            print_instruction(stdout, tac, 0);
            panic("Illegal assignment of src1/src2 to dst");
        }

        if (src1 && src2 && src1 == src2) {
            print_instruction(stdout, tac, 0);
            panic("src1 == src2 not handled");
        }

        int g1_igraph_id = vreg_igraphs[dst].igraph_id;

        // If dst is only used once and it's not in liveout, merge it.
        // Also, don't merge IR_CALLs. The IR_START_CALL and IR_END_CALL constraints don't permit
        // rearranging function calls without dire dowmstream side effects.
        // IR_CALL_ARG_REG is also off limits, since it's a placeholder for function
        // arg registers and no code is actually emitted.
        if (vreg_igraphs[dst].count == 1 && vreg_igraphs[dst].igraph_id != -1 &&
            tac->operation != IR_CALL && tac->operation != IR_MOVE_TO_PTR &&
            igraphs[g1_igraph_id].nodes[0].tac->operation != IR_CALL_ARG_REG &&
            tac->operation != IR_CALL_ARG_REG &&
            igraphs_are_neighbors(igraphs, i, g1_igraph_id)
            ) {

            // The instruction tiling code assumes dst != src1 and dst != src2..
            // Ensure that if there is a dst of target graph that it doesn't match src1
            IGraphNode *ign_g1 = &(igraphs[g1_igraph_id].nodes[0]);
            int ign_vreg = ign_g1->tac ? (ign_g1->tac->dst ? ign_g1->tac->dst->vreg : 0) : 0;

            if (ign_vreg && ign_vreg == src1) {
                if (debug_instsel_tree_merging)
                    printf("Not merging on vreg=%d since the target dst=%d would match src1=%d", dst, ign_vreg, src1);
            }
            else {
                if (debug_instsel_tree_merging) {
                    printf("\nMerging dst=%d src1=%d src2=%d ", dst, src1, src2);
                    printf("in locs %d and %d on vreg=%d\n----------------------------------------------------------\n", g1_igraph_id, i, dst);
                }

                IGraph *ig = merge_igraphs(&(igraphs[g1_igraph_id]), &(igraphs[i]), dst);

                igraphs[g1_igraph_id].nodes = ig->nodes;
                igraphs[g1_igraph_id].graph = ig->graph;
                igraphs[g1_igraph_id].node_count = ig->node_count;

                igraphs[i].node_count = 0;

                if (src1) vreg_igraphs[src1].igraph_id = g1_igraph_id;
                if (src2) vreg_igraphs[src2].igraph_id = g1_igraph_id;
            }
        }

        if (dst && tac->operation != IR_MOVE_TO_PTR)
            vreg_igraphs[dst].count = 0;

        i--;

        if (tac == blocks[block_id].start) break;
        tac = tac->prev;
    }

    if (debug_instsel_tree_merging)
        printf("\n=================================\n");

    if (debug_instsel_tree_merging) {
        for (int i = 0; i < instr_count; i++) {
            if (!igraphs[i].node_count) continue;
            tac = igraphs[i].nodes[0].tac;
            if (tac && tac->operation == IR_NOP) continue;
            if (tac) {
                Value *v = igraphs[i].nodes[0].tac->dst;
                if (v) {
                    print_value(stdout, v, 0);
                    printf(" = \n");
                }
            }
            dump_igraph(&(igraphs[i]), 0);
        }
    }
}

// Recurse down src igraph, copying nodes to dst igraph and adding edges
static void recursive_simplify_igraph(IGraph *src, IGraph *dst, int src_node_id, int dst_parent_node_id, int *dst_child_node_id) {
    IGraphNode *ign = &(src->nodes[src_node_id]);
    GraphEdge *e = src->graph->nodes[src_node_id].succ;
    Tac *tac = ign->tac;

    // If src is not the root node, the operation is a move, and it's not a type change,
    // recurse, with dst moved further down the tree
    int operation;
    if (tac) operation = tac->operation; else operation = 0;
    if (operation == IR_MOVE && src_node_id != 0 && type_eq(tac->dst->type, tac->src1->type)) {
        recursive_simplify_igraph(src, dst, e->to->id, dst_parent_node_id, dst_child_node_id);
        return;
    }

    // Copy src node to dst
    dup_inode(&(src->nodes[src_node_id]), &(dst->nodes[*dst_child_node_id]));
    if (dst_parent_node_id != -1) {
        // add an edge if it's not the root node
        add_graph_edge(dst->graph, dst_parent_node_id, *dst_child_node_id);
    }

    dst_parent_node_id = *dst_child_node_id;
    (*dst_child_node_id)++;

    // Recurse down the children
    while (e) {
        recursive_simplify_igraph(src, dst, e->to->id, dst_parent_node_id, dst_child_node_id);
        e = e->next_succ;
    }
}

// Remove sequences of moves from the instruction graph. A new graph is created by
// recursing through the src.
static IGraph *simplify_igraph(IGraph *src) {
    IGraph *dst = malloc(sizeof(IGraph));
    memset(dst, 0, sizeof(IGraph));

    int node_count = src->node_count;
    IGraphNode *inodes = malloc(node_count * sizeof(IGraphNode));
    memset(inodes, 0, node_count * sizeof(IGraphNode));

    dst->nodes = inodes;
    dst->graph = new_graph(node_count, MAX_INSTRUCTION_GRAPH_EDGE_COUNT);
    dst->node_count = node_count;

    if (debug_instsel_igraph_simplification) {
        printf("simplify_igraph() on:\n");
        dump_igraph(src, 0);
    }

    int dst_node_id = 0; // The root of the dst igraph
    recursive_simplify_igraph(src, dst, 0, -1, &dst_node_id);

    if (debug_instsel_igraph_simplification) {
        printf("\n");
        printf("Igraph simplification result:\n");
        dump_igraph(dst, 0);
        printf("\n");
    }

    dst->node_count = dst_node_id;

    return dst;
}

static void simplify_igraphs() {
    for (int i = 0; i < instr_count; i++) {
        IGraphNode *ign = &(igraphs[i].nodes[0]);
        int operation;
        if (ign->tac) operation = ign->tac->operation; else operation = 0;
        if (operation != IR_NOP && igraphs[i].node_count) {
            IGraph *ig = simplify_igraph(&(igraphs[i]));
            shallow_dup_igraph(ig, &(igraphs[i]));
        }
    }
}

// Convert an instruction graph node from an operation that puts the result
// into a register to an assigment, using a constant value.
static Value *merge_cst_node(IGraph *igraph, int node_id, Value *v) {
    GraphNode *src_node = &(igraph->graph->nodes[node_id]);

    if (node_id == 0) {
        // Root node, convert it into a move, since it has to end up
        // in a register
        igraph->nodes[node_id].tac->operation = IR_MOVE;
        igraph->nodes[node_id].tac->src1 = v;
        igraph->nodes[node_id].tac->src2 = 0;
        igraph->nodes[src_node->succ->to->id].value = v;
        src_node->succ->next_succ = 0;

    }
    else {
        // Not root-node, convert it from a tac into a value node
        igraph->nodes[node_id].tac = 0;
        igraph->nodes[node_id].value = v;
        src_node->succ = 0;
    }

    return v;
}

static Value *merge_cst_int_node(IGraph *igraph, int node_id, long constant_value, int is_unsigned) {
    Value *v = new_value();
    v->type = new_type(TYPE_LONG);
    v->type->is_unsigned = is_unsigned;
    v->is_constant = 1;
    v->int_value = constant_value;
    return merge_cst_node(igraph, node_id, v);
}

static Value *merge_cst_fp_node(IGraph *igraph, int node_id, Type *type, long double constant_value) {
    Value *v = new_value();
    v->type = dup_type(type);
    v->is_constant = 1;
    v->fp_value = constant_value;
    return merge_cst_node(igraph, node_id, v);
}

static Value* merge_integer_constants(IGraph *igraph, int node_id, int operation, Value *src1, Value *src2) {
    int is_unsigned = src1->type->is_unsigned;
    int is_long = src1->type->type == TYPE_LONG || src2->type->type == TYPE_LONG;

    int ui =  is_unsigned && !is_long;
    int ul =  is_unsigned &&  is_long;
    int si = !is_unsigned && !is_long;
    int sl = !is_unsigned &&  is_long;

    unsigned int  s1ui = (unsigned int)  src1->int_value;
    unsigned long s1ul = (unsigned long) src1->int_value;
    int           s1si = (int)           src1->int_value;
    long          s1sl = (long)          src1->int_value;

    unsigned int  s2ui = (unsigned int)  src2->int_value;
    unsigned long s2ul = (unsigned long) src2->int_value;
    int           s2si = (int)           src2->int_value;
    long          s2sl = (long)          src2->int_value;

    unsigned long r = 0;

         if (operation == IR_ADD ) { if (ui) r = s1ui +  s2ui; else if (ul) r = s1ul +  s2ul; else if (si) r = s1si +  s2si; else if (sl) r = s1sl +  s2sl; }
    else if (operation == IR_SUB ) { if (ui) r = s1ui -  s2ui; else if (ul) r = s1ul -  s2ul; else if (si) r = s1si -  s2si; else if (sl) r = s1sl -  s2sl; }
    else if (operation == IR_MUL ) { if (ui) r = s1ui *  s2ui; else if (ul) r = s1ul *  s2ul; else if (si) r = s1si *  s2si; else if (sl) r = s1sl *  s2sl; }
    else if (operation == IR_DIV ) { if (ui) r = s1ui /  s2ui; else if (ul) r = s1ul /  s2ul; else if (si) r = s1si /  s2si; else if (sl) r = s1sl /  s2sl; }
    else if (operation == IR_MOD ) { if (ui) r = s1ui %  s2ui; else if (ul) r = s1ul %  s2ul; else if (si) r = s1si %  s2si; else if (sl) r = s1sl %  s2sl; }
    else if (operation == IR_BAND) { if (ui) r = s1ui &  s2ui; else if (ul) r = s1ul &  s2ul; else if (si) r = s1si &  s2si; else if (sl) r = s1sl &  s2sl; }
    else if (operation == IR_BOR ) { if (ui) r = s1ui |  s2ui; else if (ul) r = s1ul |  s2ul; else if (si) r = s1si |  s2si; else if (sl) r = s1sl |  s2sl; }
    else if (operation == IR_XOR ) { if (ui) r = s1ui ^  s2ui; else if (ul) r = s1ul ^  s2ul; else if (si) r = s1si ^  s2si; else if (sl) r = s1sl ^  s2sl; }
    else if (operation == IR_BSHL) { if (ui) r = s1ui << s2ui; else if (ul) r = s1ul << s2ul; else if (si) r = s1si << s2si; else if (sl) r = s1sl << s2sl; }
    else if (operation == IR_BSHR) { if (ui) r = s1ui >> s2ui; else if (ul) r = s1ul >> s2ul; else if (si) r = s1si >> s2si; else if (sl) r = s1sl >> s2sl; }
    else if (operation == IR_EQ  ) { if (ui) r = s1ui == s2ui; else if (ul) r = s1ul == s2ul; else if (si) r = s1si == s2si; else if (sl) r = s1sl == s2sl; }
    else if (operation == IR_NE  ) { if (ui) r = s1ui != s2ui; else if (ul) r = s1ul != s2ul; else if (si) r = s1si != s2si; else if (sl) r = s1sl != s2sl; }
    else if (operation == IR_LT  ) { if (ui) r = s1ui <  s2ui; else if (ul) r = s1ul <  s2ul; else if (si) r = s1si <  s2si; else if (sl) r = s1sl <  s2sl; }
    else if (operation == IR_GT  ) { if (ui) r = s1ui >  s2ui; else if (ul) r = s1ul >  s2ul; else if (si) r = s1si >  s2si; else if (sl) r = s1sl >  s2sl; }
    else if (operation == IR_LE  ) { if (ui) r = s1ui <= s2ui; else if (ul) r = s1ul <= s2ul; else if (si) r = s1si <= s2si; else if (sl) r = s1sl <= s2sl; }
    else if (operation == IR_GE  ) { if (ui) r = s1ui >= s2ui; else if (ul) r = s1ul >= s2ul; else if (si) r = s1si >= s2si; else if (sl) r = s1sl >= s2sl; }
    else return 0;

    return merge_cst_int_node(igraph, node_id, r, is_unsigned);

}

// Assert that both src1 and src2 are long doubles
static void check_fp_src1_src2(Value *src1, Value *src2) {
    if (is_floating_point_type(src1->type) != is_floating_point_type(src2->type))
        panic("Attempt to merge two floating point constants when one of them is not a floating point");
}

static Value* merge_fp_constants(IGraph *igraph, int node_id, int operation, Value *src1, Value *src2, Type *type) {
         if (operation == IR_ADD) { check_fp_src1_src2(src1, src2); return merge_cst_fp_node( igraph, node_id, type, src1->fp_value +  src2->fp_value); }
    else if (operation == IR_SUB) { check_fp_src1_src2(src1, src2); return merge_cst_fp_node( igraph, node_id, type, src1->fp_value -  src2->fp_value); }
    else if (operation == IR_MUL) { check_fp_src1_src2(src1, src2); return merge_cst_fp_node( igraph, node_id, type, src1->fp_value *  src2->fp_value); }
    else if (operation == IR_DIV) { check_fp_src1_src2(src1, src2); return merge_cst_fp_node( igraph, node_id, type, src1->fp_value /  src2->fp_value); }
    else if (operation == IR_EQ ) { check_fp_src1_src2(src1, src2); return merge_cst_int_node(igraph, node_id, src1->fp_value == src2->fp_value, 0 ); }
    else if (operation == IR_NE ) { check_fp_src1_src2(src1, src2); return merge_cst_int_node(igraph, node_id, src1->fp_value != src2->fp_value, 0 ); }
    else if (operation == IR_LT ) { check_fp_src1_src2(src1, src2); return merge_cst_int_node(igraph, node_id, src1->fp_value <  src2->fp_value, 0 ); }
    else if (operation == IR_GT ) { check_fp_src1_src2(src1, src2); return merge_cst_int_node(igraph, node_id, src1->fp_value >  src2->fp_value, 0 ); }
    else if (operation == IR_LE ) { check_fp_src1_src2(src1, src2); return merge_cst_int_node(igraph, node_id, src1->fp_value <= src2->fp_value, 0 ); }
    else if (operation == IR_GE ) { check_fp_src1_src2(src1, src2); return merge_cst_int_node(igraph, node_id, src1->fp_value >= src2->fp_value, 0 ); }
    else return 0;
}

static Value *recursive_merge_constants(IGraph *igraph, int node_id) {
    GraphNode *src_node = &(igraph->graph->nodes[node_id]);
    Tac *tac = igraph->nodes[node_id].tac;
    if (!tac) return igraph->nodes[node_id].value;
    int operation = tac->operation;
    if (operation == IR_NOP) return 0;

    int i = 1;
    Value *src1 = 0;
    Value *src2 = 0;
    GraphEdge *e = src_node->succ;
    while (e) {
        Value *v = recursive_merge_constants(igraph, e->to->id);
        if (i == 1) src1 = v; else src2 = v;
        e = e->next_succ;
        i++;
    }

    int cst1 = src1 && src1->is_constant;

    if (!src1 || !cst1) return 0;

    // Unary operations
    if (operation == IR_BNOT) return merge_cst_int_node(igraph, node_id, ~src1->int_value, src1->type->is_unsigned);

    // Binary operations
    int cst2 = cst1 && (src2 && src2->is_constant);
    if (!src2 || !cst2) return 0;

    Type *common_type;
    if (src1->type->type == TYPE_LONG_DOUBLE || src2->type->type == TYPE_LONG_DOUBLE)
        common_type = new_type(TYPE_LONG_DOUBLE);
    else if (src1->type->type == TYPE_DOUBLE || src2->type->type == TYPE_DOUBLE)
        common_type = new_type(TYPE_DOUBLE);
    else if (src1->type->type == TYPE_FLOAT || src2->type->type == TYPE_FLOAT)
        common_type = new_type(TYPE_FLOAT);
    else
        common_type = new_type(TYPE_LONG);

    if (is_floating_point_type(src1->type) && is_floating_point_type(src2->type)) {
        return merge_fp_constants(igraph, node_id, operation, src1, src2, common_type);
    }
    else
        return merge_integer_constants(igraph, node_id, operation, src1, src2);
}

// Walk all instruction trees and merge nodes where there are constant operations
static void merge_constants(Function *function) {
    for (int i = 0; i < instr_count; i++) {
        if (!igraphs[i].node_count) continue;
        recursive_merge_constants(&(igraphs[i]), 0);
    }
}

// Print the cost graph, only showing choices where parent src and child dst match
static void recursive_print_cost_graph(Graph *cost_graph, int *cost_rules, int *accumulated_cost, int node_id, int parent_node_id, int parent_src, int indent) {
    GraphEdge *choice_edge = cost_graph->nodes[node_id].succ;
    while (choice_edge) {
        int choice_node_id = choice_edge->to->id;
        GraphNode *choice_node = &(cost_graph->nodes[choice_node_id]);

        int match = 1;
        if (parent_node_id != -1) {
            Rule *parent_rule = &(instr_rules[cost_rules[parent_node_id]]);
            int src = (parent_src == 1) ? parent_rule->src1 : parent_rule->src2;
            match = (src == instr_rules[cost_rules[choice_node_id]].dst);
            if (match) {
                printf("%-3d ", choice_node_id);
                for (int i = 0; i < indent; i++) printf("  ");
                printf("%d ", cost_rules[choice_node_id]);
            }
        }
        else
            printf("%d ", cost_rules[choice_node_id]);

        if (match) {
            printf(" (%d)\n", accumulated_cost[choice_node_id]);

            GraphEdge *e = choice_node->succ;
            int j = 1;
            while (e) {
                printf("%-3d ", choice_node_id);
                for (int i = 0; i < indent + 1; i++) printf("  ");
                printf("src%d\n", j);
                recursive_print_cost_graph(cost_graph, cost_rules, accumulated_cost, e->to->id, choice_node_id, j, indent + 2);

                e = e->next_succ;
                j++;
            }
        }

        choice_edge = choice_edge->next_succ;
    }
}

static void print_cost_graph(Graph *cost_graph, int *cost_rules, int *accumulated_cost) {
    recursive_print_cost_graph(cost_graph, cost_rules, accumulated_cost, 0, -1, -1, 0);
}

static int new_cost_graph_node() {
    if (cost_graph_node_count == MAX_INSTRUCTION_GRAPH_CHOICE_NODE_COUNT)
        panic("MAX_INSTRUCTION_GRAPH_CHOICE_NODE_COUNT");
    return cost_graph_node_count++;
}

static void calculate_accumulated_cost_for_new_cost_node(Rule *r, int src1_cost_graph_node_id, int src2_cost_graph_node_id, int choice_node_id) {
    // Do some on the fly accounting, find the minimal cost sub trees
    int min_cost_src1 = 100000000;
    int min_cost_src2 = 100000000;

    // Loop over src1 and src2 if there is one
    for (int i = 1; i <= 2; i++) {
        if (!src2_cost_graph_node_id && i == 2) continue; // Unary operation

        int src = i == 1 ? r->src1 : r->src2;
        GraphEdge *e = i == 1
            ? cost_graph->nodes[src1_cost_graph_node_id].succ
            : cost_graph->nodes[src2_cost_graph_node_id].succ;

        // Find the lowest cost from all the edges
        int min_cost = 100000000;
        while (e) {
            Rule *child_rule = &(instr_rules[cost_rules[e->to->id]]);

            if (src == child_rule->dst) {
                int cost = accumulated_cost[e->to->id];
                if (cost < min_cost) min_cost = cost;
            }

            e = e->next_succ;
        }

        if (i == 1)
            min_cost_src1 = min_cost;
        else
            min_cost_src2 = min_cost;
    }

    if (!src2_cost_graph_node_id) min_cost_src2 = 0;

    accumulated_cost[choice_node_id] = min_cost_src1 + min_cost_src2 + r->cost;
}

// Tile a graph node that has an operation but no operands.
// These are used e.g. in a parameterless return statement.
static int tile_igraph_operand_less_node(IGraph *igraph, int node_id) {
    if (debug_instsel_tiling) printf("tile_igraph_operand_less_node on node=%d\n", node_id);

    int cost_graph_node_id = new_cost_graph_node();
    cost_to_igraph_map[cost_graph_node_id] = node_id;

    Tac *tac = igraph->nodes[node_id].tac;
    for (int i = 0; i < instr_rule_count; i++) {
        Rule *r = &(instr_rules[i]);
        if (r->operation == tac->operation) {
            if (debug_instsel_tiling) {
                printf("matched rule %d:\n", i);
                print_rule(r, 0, 0);
            }

            int choice_node_id = new_cost_graph_node();
            cost_to_igraph_map[choice_node_id] = node_id;
            cost_rules[choice_node_id] = i;
            accumulated_cost[choice_node_id] = instr_rules[i].cost;

            add_graph_edge(cost_graph, cost_graph_node_id, choice_node_id);

            return cost_graph_node_id;
        }
    }

    dump_igraph(igraph, 0);
    panic("Did not match any rules");
}

// Tile a leaf node in the instruction tree. Rules for leaf nodes all have a zero
// operation. This is simply a case of matching the value to the rule src1.
static int tile_igraph_leaf_node(IGraph *igraph, int node_id) {
    if (debug_instsel_tiling) dump_igraph(igraph, 0);

    int cost_graph_node_id = new_cost_graph_node();
    cost_to_igraph_map[cost_graph_node_id] = node_id;

    Value *v = igraph->nodes[node_id].value;

    // Find a matching instruction
    int matched = 0;
    for (int i = 0; i < instr_rule_count; i++) {
        Rule *r = &(instr_rules[i]);
        if (r->operation) continue;

        if (!match_value_to_rule_src(v, r->src1)) continue;
        if (!v->is_constant && !v->label && !v->function_symbol && !match_value_type_to_rule_dst(v, r->dst)) continue;

        if (debug_instsel_tiling) {
            printf("matched rule %d:\n", i);
            print_rule(r, 0, 0);
        }
        add_to_set(igraph_labels[node_id], i);

        int choice_node_id = new_cost_graph_node();
        cost_to_igraph_map[choice_node_id] = node_id;
        cost_rules[choice_node_id] = i;
        accumulated_cost[choice_node_id] = instr_rules[i].cost;

        add_graph_edge(cost_graph, cost_graph_node_id, choice_node_id);

        matched = 1;
    }

    if (!matched) {
        dump_igraph(igraph, 0);
        panic("Did not match any rules");
    }

    return cost_graph_node_id;
}

// Check if the dst for any label in the child tree for src_id matches rule_src.
static int match_subtree_labels_to_rule(int src_id, int rule_src) {
    int count = igraph_labels[src_id]->cached_element_count;
    int *cached_elements = igraph_labels[src_id]->cached_elements;
    for (int i = 0; i < count; i++)
        if (rule_src == instr_rules[cached_elements[i]].dst) return 1;

    return 0;
}

// Tile an instruction graph node that has an operation with 0, 1 or 2 operands.
static int tile_igraph_operation_node(IGraph *igraph, int node_id) {
    IGraphNode *inode = &(igraph->nodes[node_id]);
    Tac *tac = inode->tac;
    int operation = tac->operation;

    if (debug_instsel_tiling) {
        printf("tile_igraph_operation_node on node=%d\n", node_id);
        print_instruction(stdout, tac, 0);
        dump_igraph(igraph, 0);
    }

    // Fetch the operands from the graph
    int src1_id = 0;
    int src2_id = 0;
    GraphEdge *e = igraph->graph->nodes[node_id].succ;
    if (e) {
        src1_id = e->to->id;
        e = e->next_succ;
        if (e) src2_id = e->to->id;
    }

    // Special case: no operands
    if (!src1_id) return tile_igraph_operand_less_node(igraph, node_id);

    // Create the root cost instruction graph node
    int cost_graph_node_id = new_cost_graph_node();
    cost_to_igraph_map[cost_graph_node_id] = node_id;

    // Recurse down the src1 and src2 trees
    int src1_cost_graph_node_id = recursive_tile_igraphs(igraph, src1_id);

    int src2_cost_graph_node_id;
    if (src2_id)
        src2_cost_graph_node_id = recursive_tile_igraphs(igraph, src2_id);
    else
        src2_cost_graph_node_id = 0;

    if (debug_instsel_tiling) {
        printf("back from recursing at node=%d\n\n", node_id);
        printf("src1 labels: "); print_set(igraph_labels[src1_id]); printf(" ");
        printf("\n");

        if (src2_id) {
            printf("src2 labels: ");
            print_set(igraph_labels[src2_id]);
            printf("\n");
        }
    }

    if (src1_id) cache_set_elements(igraph_labels[src1_id]);
    if (src2_id) cache_set_elements(igraph_labels[src2_id]);

    if (debug_instsel_tiling && tac->dst)
        printf("Want dst %s\n", value_to_non_terminal_string(tac->dst));

    // Loop over all rules and gather matches in the cost graph
    int matched = 0;
    for (int i = 0; i < instr_rule_count; i++) {
        Rule *r = &(instr_rules[i]);

        if (r->operation != operation) continue; // Only handle operations nodes
        if (src2_id && !r->src2 || !src2_id && r->src2) continue; // Filter rules requiring src2

        // If this is the top level or a rule requires it, ensure the dst matches.
        int match_dst = tac->dst && node_id == 0;

        // IR_MOVE_TO_PTR is a special case since it doesn't have a dst. The dst is actually
        // src1, which isn't modified.
        if (match_dst) {
            if (tac->operation != IR_MOVE_TO_PTR && !match_value_to_rule_dst(tac->dst, r->dst)) continue;
            if (tac->operation == IR_MOVE_TO_PTR && !match_value_to_rule_dst(tac->src1, r->dst)) continue;
        }

        if (tac->operation == IR_MOVE_TO_PTR && !match_value_type_to_rule_dst(tac->src1, r->dst)) continue;
        else if (tac->dst && !match_value_type_to_rule_dst(tac->dst, r->dst)) continue;

        // Check dst of the subtree tile matches what is needed
        int matched_src = match_subtree_labels_to_rule(src1_id, r->src1);
        if (!matched_src) continue;

        if (src2_id) {
            matched_src = match_subtree_labels_to_rule(src2_id, r->src2);
            if (!matched_src) continue;
        }

        // We have a winner
        matched = 1;

        if (debug_instsel_tiling) {
            printf("matched rule %d:\n", i);
            print_rule(r, 1, 0);
        }

        add_to_set(igraph_labels[node_id], i); // Add a label

        // Add cost graph nodes for src1 and potentially src2
        int choice_node_id = new_cost_graph_node();
        cost_rules[choice_node_id] = i;
        cost_to_igraph_map[choice_node_id] = node_id;

        add_graph_edge(cost_graph, cost_graph_node_id, choice_node_id);
        add_graph_edge(cost_graph, choice_node_id, src1_cost_graph_node_id);
        if (src2_id) add_graph_edge(cost_graph, choice_node_id, src2_cost_graph_node_id);

        calculate_accumulated_cost_for_new_cost_node(r, src1_cost_graph_node_id, src2_cost_graph_node_id, choice_node_id);
    }

    // All rules have been visited, if there are no matches, we have a problem, bail
    // with an error.
    if (!matched) {
        printf("\nNo rules matched\n");
        if (tac->dst) printf("Want dst %s\n", value_to_non_terminal_string(tac->dst));
        print_instruction(stdout, tac, 0);
        dump_igraph(igraph, 0);
        exit(1);
    }

    return cost_graph_node_id;
}

// Algorithm from page 618 of Engineering a compiler
// with additional minimal cost tiling determination as suggested on page 619.
static int recursive_tile_igraphs(IGraph *igraph, int node_id) {
    if (debug_instsel_tiling) printf("\nrecursive_tile_igraphs on node=%d\n", node_id);

    if (!igraph->nodes[node_id].tac)
        return tile_igraph_leaf_node(igraph, node_id);
    else
        return tile_igraph_operation_node(igraph, node_id);
}

// find the lowest cost successor in the cost graph at node_id
static int get_least_expensive_choice_node_id(int node_id, int parent_node_id, int parent_src) {
    int min_cost = 100000000;
    int least_expensive_choice_node_id = -1;
    GraphEdge *choice_edge = cost_graph->nodes[node_id].succ;
    while (choice_edge) {
        int choice_node_id = choice_edge->to->id;
        int match;

        if (parent_node_id != -1) {
            // Ensure src and dst match for non-root nodes
            Rule *parent_rule = &(instr_rules[cost_rules[parent_node_id]]);
            int pv = (parent_src == 1) ? parent_rule->src1 : parent_rule->src2;
            match = (pv == instr_rules[cost_rules[choice_node_id]].dst);
        }
        else
            match = 1;

        if (match) {
            int cost = accumulated_cost[choice_node_id];
            if (cost < min_cost) {
                min_cost = cost;
                least_expensive_choice_node_id = choice_node_id;
            }
        }

        choice_edge = choice_edge->next_succ;
    }

    if (least_expensive_choice_node_id == -1)
        panic("Internal error: No matched choices in recursive_make_intermediate_representation");

    return least_expensive_choice_node_id;
}

static Value *load_value_from_slot(int slot, char *arg) {
    slot = slot - SV1 + 1;

    if (slot > MAX_SAVED_REGISTERS)
        panic2d("Loaded register exceeds maximum: %d > %d", slot, MAX_SAVED_REGISTERS);

    Value *slot_value = saved_values[slot];
    if (!slot_value) {
        printf("Got a null value in slot %d for arg %s\n", slot, arg);
        panic("Aborting");
    }

    if (debug_instsel_tiling) {
        printf("  using value from slot %d ", slot);
        if (slot_value->type) print_value(stdout, slot_value, 0);
        printf(" for arg %s\n", arg);
    }

    return slot_value;
}

static Value *generate_instructions(Function *function, IGraphNode *ign, int is_root, Rule *rule, Value *src1, Value *src2) {
    if (debug_instsel_tiling) {
        printf("Generating instructions for rule %-4d: ", rule->index);
        print_rule(rule, 0, 0);
    }

    Value *dst = 0;
    if (is_root) {
        // Use the root node tac or value as a result
        if (ign->tac) {
            if (debug_instsel_tiling)
                printf("  using root vreg for dst vreg=%d\n", ign->tac->dst ? ign->tac->dst->vreg : -1);
            dst = ign->tac->dst;
        }
        else
            dst = ign->value;
    }
    else {
        // Determine the result from the x86 definition
        if (rule->x86_operations) {
            // It's an operation on a non-root node. Allocate a vreg.

            dst = new_value();
            dst->vreg = ++function->vreg_count;
            dst->ssa_subscript = -1;

            if (ign->tac) {
                dst->type = dup_type(ign->tac->dst->type);
                dst->offset = ign->tac->dst->offset;
            }
            else
                dst->type = dup_type(ign->value->type);

            if (debug_instsel_tiling) printf("  allocated new vreg for dst vreg=%d\n", dst->vreg);
        }
        else {
            if (debug_instsel_tiling) printf("  using passthrough dst\n");
            dst = src1; // Passthrough for cst or reg
        }
    }

    // In order for the spill code to generate correct size spills,
    // x86_size has to be set to match what the x86 instructions
    // are doing. This cannot be done from the type since the type
    // reflects what the parser has produced, and doesn't necessarily
    // match what the x86 code is doing.
    if (dst)  dst->x86_size  = make_x86_size_from_non_terminal(rule->dst);
    if (src1) src1->x86_size = make_x86_size_from_non_terminal(rule->src1);
    if (src2) src2->x86_size = make_x86_size_from_non_terminal(rule->src2);

    // A composite rule may do some saves, which are done in already run x86 save
    // operations. The final operation(s) then loads the values from the saved slots
    // and add them to the tac.
    // These keep track of the values outputted during the loads.
    X86Operation *x86op = rule->x86_operations;
    Value *x86_dst, *x86_v1, *x86_v2;

    while (x86op) {
             if (x86op->dst == 0)    x86_dst = 0;
        else if (x86op->dst == SRC1) x86_dst = src1;
        else if (x86op->dst == SRC2) x86_dst = src2;
        else if (x86op->dst == DST)  x86_dst = dst;
        else if (x86op->dst >= SV1 && x86op->dst <= SV8) x86_dst = load_value_from_slot(x86op->dst, "dst");
        else panic1d("Unknown operand to x86 instruction: %d", x86op->v1);

             if (x86op->v1 == 0)    x86_v1 = 0;
        else if (x86op->v1 == SRC1) x86_v1 = src1;
        else if (x86op->v1 == SRC2) x86_v1 = src2;
        else if (x86op->v1 == DST)  x86_v1 = dst;
        else if (x86op->v1 >= SV1 && x86op->v1 <= SV8) x86_v1 = load_value_from_slot(x86op->v1, "v1");
        else panic1d("Unknown operand to x86 instruction: %d", x86op->v1);

             if (x86op->v2 == 0)    x86_v2 = 0;
        else if (x86op->v2 == SRC1) x86_v2 = src1;
        else if (x86op->v2 == SRC2) x86_v2 = src2;
        else if (x86op->v2 == DST)  x86_v2 = dst;
        else if (x86op->v2 >= SV1 && x86op->v2 <= SV8) x86_v2 = load_value_from_slot(x86op->v2, "v2");
        else panic1d("Unknown operand to x86 instruction: %d", x86op->v2);

        if (x86_dst) x86_dst = dup_value(x86_dst);
        if (x86_v1)  x86_v1  = dup_value(x86_v1);
        if (x86_v2)  x86_v2  = dup_value(x86_v2);

        Value *slot_value;

        if (x86op->save_value_in_slot) {
            if (x86op->save_value_in_slot > MAX_SAVED_REGISTERS)
                panic2d("Saved register exceeds maximum: %d > %d", x86op->save_value_in_slot, MAX_SAVED_REGISTERS);

                 if (x86op->arg == 1) slot_value = src1;
            else if (x86op->arg == 2) slot_value = src2;
            else if (x86op->arg == 3) slot_value = dst;
            else panic1d ("Unknown load arg target", x86op->arg);

            saved_values[x86op->save_value_in_slot] = slot_value;
            if (debug_instsel_tiling) {
                printf("  saved arg %d ", x86op->arg);
                if (slot_value->type) print_value(stdout, slot_value, 0);
                printf(" to slot %d\n", x86op->save_value_in_slot);
            }
        }
        else if (x86op->allocate_stack_index_in_slot) {
            int stack_index = -(++function->stack_register_count);
            Value *slot_value = new_value();
            slot_value->type = new_type(x86op->allocated_type);
            slot_value->stack_index = stack_index;

            saved_values[x86op->allocate_stack_index_in_slot] = slot_value;
            if (debug_instsel_tiling)
                printf("  allocated stack index %d, type %d in slot %d\n", stack_index, x86op->allocated_type, x86op->allocate_stack_index_in_slot);
        }
        else if (x86op->allocate_register_in_slot) {
            int vreg = ++function->vreg_count;
            Value *slot_value = new_value();
            slot_value->type = new_type(x86op->allocated_type);
            slot_value->vreg = vreg;

            saved_values[x86op->allocate_register_in_slot] = slot_value;
            if (debug_instsel_tiling)
                printf("  allocated vreg %d, type %d in slot %d\n", vreg, x86op->allocated_type, x86op->allocate_register_in_slot);
        }
        else if (x86op->allocate_label_in_slot) {
            Value *slot_value = new_value();
            slot_value->label = ++label_count;

            saved_values[x86op->allocate_label_in_slot] = slot_value;
            if (debug_instsel_tiling)
                printf("  allocated label %d, in slot %d\n", slot_value->label, x86op->allocate_label_in_slot);
        }
        else {
            // Add a tac to the IR

            int label = 0;
            X86Operation *x86op2 = x86op;
            if (x86op->template && x86op->template[0] == '.' && x86op->template[1] == 'L') {
                x86op2 = dup_x86_operation(x86op);
                // If the template starts with ".Ln:", where n is a digit, load a label
                // from slot n, use it in the instruction and rewrite the mnemonic
                int slot = x86op2->template[2] - '0';
                Value *v = load_value_from_slot(SV1 + slot - 1, "label");
                label = v->label;

                // Strip off the label
                x86op2->template = &x86op2->template[4];

                // Set the instruction to zero if there is only a label
                if (x86op2->template[0] == 0) x86op2->template = 0;
            }

            Tac *tac = add_x86_instruction(x86op2, x86_dst, x86_v1, x86_v2);
            tac->label = label;
            if (debug_instsel_tiling) print_instruction(stdout, tac, 0);
        }

        x86op = x86op->next;
    }

    return dst;
}

// Add instructions to the intermediate representation by doing a post-order walk over the cost tree, picking the matching rules with lowest cost
static Value *recursive_make_intermediate_representation(Function *function, IGraph *igraph, int node_id, int parent_node_id, int parent_src) {
    int least_expensive_choice_node_id = get_least_expensive_choice_node_id(node_id, parent_node_id, parent_src);
    int igraph_node_id = cost_to_igraph_map[least_expensive_choice_node_id];
    IGraphNode *ign = &(igraph->nodes[igraph_node_id]);

    Rule *rule = &(instr_rules[cost_rules[least_expensive_choice_node_id]]);
    igraph_rules[igraph_node_id] = rule;
    add_to_set(rule_coverage, rule->index);

    GraphEdge *e = cost_graph->nodes[least_expensive_choice_node_id].succ;
    int child_src = 1;
    Value *src1 = ign->value;
    Value *src2 = 0;

    while (e) {
        Value *src = recursive_make_intermediate_representation(function, igraph, e->to->id, least_expensive_choice_node_id, child_src);
        if (child_src == 1)
            src1 = src;
        else
            src2 = src;

        e = e->next_succ;
        child_src++;
    }

    int is_root = parent_node_id == -1;

    return generate_instructions(function, ign, is_root, rule, src1, src2);
}

static void make_intermediate_representation(Function *function, IGraph *igraph) {
    if (debug_instsel_tiling) {
        printf("\nMaking IR\n");
        dump_igraph(igraph, 0);
        printf("\n");
    }

    saved_values = malloc((MAX_SAVED_REGISTERS + 1) * sizeof(Value *));
    recursive_make_intermediate_representation(function, igraph, 0, -1, -1);
    free(saved_values);

    if (debug_instsel_tiling) {
        printf("\nMatched rules\n");
        dump_igraph(igraph, 1);
        printf("\n");
    }
}

static void tile_igraphs(Function *function) {
    if (debug_instsel_tiling) {
        printf("\nAll trees\n-----------------------------------------------------\n");

        for (int i = 0; i < instr_count; i++) {
            if (!igraphs[i].node_count) continue;
            Tac *tac = igraphs[i].nodes[0].tac;
            if (tac && tac->dst) {
                print_value(stdout, tac->dst, 0);
                printf(" =\n");
            }
            if (igraphs[i].node_count) dump_igraph(&(igraphs[i]), 0);
        }
    }

    ir_start = 0;

    for (int i = 0; i < instr_count; i++) {
        if (!igraphs[i].node_count) continue;
        Tac *tac = igraphs[i].nodes[0].tac;

        // Whitelist operations there are no rules for
        if (tac &&
            (tac->operation == IR_NOP ||
             tac->operation == IR_DECL_LOCAL_COMP_OBJ ||
             tac->operation == IR_START_CALL ||
             tac->operation == IR_END_CALL ||
             tac->operation == IR_RETURN ||
             tac->operation == IR_START_LOOP ||
             tac->operation == IR_END_LOOP ||
             tac->operation == IR_CALL_ARG_REG ||
             tac->operation == IR_ARG_STACK_PADDING)) {

            add_tac_to_ir(tac);
            continue;
        }

        igraph_labels = malloc(igraphs[i].node_count * sizeof(Set *));
        for (int j = 0; j < igraphs[i].node_count; j++) igraph_labels[j] = new_set(instr_rule_count);

        igraph_rules = malloc(igraphs[i].node_count * sizeof(Rule *));
        memset(igraph_rules, 0, igraphs[i].node_count * sizeof(Rule *));

        if (debug_instsel_tiling)
            printf("\nTiling\n-----------------------------------------------------\n");

        cost_graph = new_graph(MAX_INSTRUCTION_GRAPH_CHOICE_NODE_COUNT, MAX_INSTRUCTION_GRAPH_CHOICE_EDGE_COUNT);

        cost_rules = malloc(MAX_INSTRUCTION_GRAPH_CHOICE_NODE_COUNT * sizeof(int));
        memset(cost_rules, 0, MAX_INSTRUCTION_GRAPH_CHOICE_NODE_COUNT * sizeof(int));

        accumulated_cost = malloc(MAX_INSTRUCTION_GRAPH_CHOICE_NODE_COUNT * sizeof(int));
        memset(accumulated_cost, 0, MAX_INSTRUCTION_GRAPH_CHOICE_NODE_COUNT * sizeof(int));

        cost_graph_node_count = 0;

        cost_to_igraph_map = malloc(MAX_INSTRUCTION_GRAPH_CHOICE_NODE_COUNT * sizeof(int));
        memset(cost_to_igraph_map, 0, MAX_INSTRUCTION_GRAPH_CHOICE_NODE_COUNT * sizeof(int));

        recursive_tile_igraphs(&(igraphs[i]), 0);
        if (debug_instsel_cost_graph) print_cost_graph(cost_graph, cost_rules, accumulated_cost);

        if (tac && tac->label) {
            add_instruction(IR_NOP, 0, 0, 0);
            ir_start->label = tac->label;
        }

        Tac *current_instruction_ir_start = ir;
        make_intermediate_representation(function, &(igraphs[i]));
        if (debug_instsel_tiling) {
            Function *f = new_function();
            f->ir = current_instruction_ir_start;
            print_ir(f, 0, 0);
        }
    }

    if (debug_instsel_tiling) {
        printf("\nFinal IR for block:\n");
        Function *f = new_function();
        f->ir = ir_start;
        print_ir(f, 0, 0);
    }
}

void select_instructions(Function *function) {
    Block *blocks = function->blocks;
    Graph *cfg = function->cfg;
    int block_count = cfg->node_count;

    transform_lvalues(function);

    // new_ir_start is the start of the new IR
    Tac *new_ir_start = new_instruction(IR_NOP);
    Tac *new_ir_pos = new_ir_start;

    // Loop over all blocks
    for (int i = 0; i < block_count; i++) {
        blocks[i].end->next = 0; // Will be re-entangled later

        make_igraphs(function, i);
        simplify_igraphs(function, i);
        if (!disable_merge_constants) merge_constants(function);
        tile_igraphs(function);

        if (ir_start) {
            new_ir_pos->next = ir_start;
            ir_start->prev = new_ir_pos;
        }

        while (new_ir_pos->next) new_ir_pos = new_ir_pos->next;
    }

    function->ir = new_ir_start;

    if (debug_instsel_tiling) {
        printf("\nFinal IR for function:\n");
        print_ir(function, 0, 0);
    }
}

// This removes instructions that copy a stack location to itself by replacing them with noops.
void remove_stack_self_moves(Function *function) {
    for (Tac *tac = function->ir; tac; tac = tac->next) {
        if (tac->operation == X_MOV && tac->dst && tac->dst->stack_index && tac->src1 && tac->src1->stack_index && tac->dst->stack_index == tac->src1->stack_index) {
            tac->operation = IR_NOP;
            tac->dst = 0;
            tac->src1 = 0;
            tac->src2 = 0;
            tac->x86_template = 0;
        }
    }
}

// This removes instructions that copy a register to itself by replacing them with noops.
void remove_vreg_self_moves(Function *function) {
    for (Tac *tac = function->ir; tac; tac = tac->next) {
        if (tac->operation == X_MOV && tac->dst && tac->dst->vreg && tac->src1 && tac->src1->vreg && tac->dst->vreg == tac->src1->vreg) {
            tac->operation = IR_NOP;
            tac->dst = 0;
            tac->src1 = 0;
            tac->src2 = 0;
            tac->x86_template = 0;
        }
    }
}

static Tac *make_spill_instruction(Value *v) {
    int x86_operation;
    char *x86_template;

    make_value_x86_size(v);

    if (v->type->type == TYPE_FLOAT) {
        x86_operation = X_MOV;
        x86_template = "movss %v1F, %vdF";
    }
    else if (v->type->type == TYPE_FLOAT) {
        x86_operation = X_MOV;
        x86_template = "movsd %v1D, %vdD";
    }
    else if (v->x86_size == 1) {
        x86_operation = X_MOV;
        x86_template = "movb %v1b, %vdb";
    }
    else if (v->x86_size == 2) {
        x86_operation = X_MOV;
        x86_template = "movw %v1w, %vdw";
    }
    else if (v->x86_size == 3) {
        x86_operation = X_MOV;
        x86_template = "movl %v1l, %vdl";
    }
    else if (v->x86_size == 4) {
        x86_operation = X_MOV;
        x86_template = "movq %v1q, %vdq";
    }
    else
        panic1d("Unknown x86 size %d", v->x86_size);

    Tac *tac = new_instruction(x86_operation);
    tac->x86_template = x86_template;

    return tac;
}

static void add_spill_load(Tac *ir, int src, int preg) {
    Value *v = src == 1 ? ir->src1 : ir->src2;

    Tac *tac = make_spill_instruction(v);
    tac->src1 = v;
    tac->dst = new_value();
    tac->dst->type = new_type(TYPE_LONG);
    tac->dst->x86_size = 4;
    tac->dst->vreg = -1000;   // Dummy value
    tac->dst->preg = preg;

    if (src == 1)
        ir->src1 = dup_value(tac->dst);
    else
        ir->src2 = dup_value(tac->dst);

    insert_instruction(ir, tac, 1);

    // If a register has an offset, it's used in an indirect operation, such
    // as 4(rax). This means, take the value in rax and add four to it. However, by
    // moving it to the stack, without any manipulation the meaning changes from
    // 4(rax) to 4 + allocated_stack_offset(rbp), which would be incorrect. To fix this,
    // the offset is removed and converted to an add instruction, so that we get
    // movq allocated_stack_offset(rbp), spill_reg
    // addq spill_reg, 4.
    if (v->offset) {
        Tac *tac = new_instruction(X_MOV);
        tac->x86_template = "addq $%v1q, %vdq";

        tac->src1 = new_integral_constant(TYPE_LONG, v->offset);

        tac->dst = new_value();
        tac->dst->type = new_type(TYPE_LONG);
        tac->dst->x86_size = 4;
        tac->dst->vreg = -1000;   // Dummy value
        tac->dst->preg = preg;

        insert_instruction(ir, tac, 1);

        v->offset = 0;
    }
}

static void add_spill_store(Tac *ir, Value *v, int preg) {
    Tac *tac = make_spill_instruction(v);
    tac->src1 = new_value();
    tac->src1->type = new_type(TYPE_LONG);
    tac->src1->x86_size = 4;
    tac->src1->vreg = -1000;   // Dummy value
    tac->src1->preg = preg;

    tac->dst = v;
    ir->dst = dup_value(tac->src1);

    if (!ir->next) {
        ir->next = tac;
        tac->prev = ir;
    }
    else
        insert_instruction(ir->next, tac, 0);
}

// Return one of the int or sse registers used as temporary in spill code
static int get_spill_register(Value *v, int spill_register) {
    if (is_sse_floating_point_type(v->type))
        return spill_register == 1 ? REG_XMM14 : REG_XMM15;
    else
        return spill_register == 1 ? REG_R10 : REG_R11;
}

void add_spill_code(Function *function) {
    if (debug_instsel_spilling) printf("\nAdding spill code\n");

    for (Tac *tac = function->ir; tac; tac = tac->next) {
        if (debug_instsel_spilling) print_instruction(stdout, tac, 0);

        // Allow all moves where either dst is a register and src is on the stack
        if (tac->operation == X_MOV || tac->operation == X_MOVS || tac->operation == X_MOVZ)
            if (tac->dst && tac->dst->preg != -1 && tac->src1 && tac->src1->stack_index) continue;

        // Allow non sign-extends moves if the dst is on the stack and the src is a register
        if (tac->operation == X_MOV && tac->dst && tac->dst->stack_index && tac->src1 && tac->src1->preg != -1) continue;

        int dst_eq_src1 = (tac->dst && tac->src1 && tac->dst->vreg == tac->src1->vreg);

        if (tac->src1 && tac->src1->spilled)  {
            if (debug_instsel_spilling) printf("Adding spill load\n");
            add_spill_load(tac, 1, get_spill_register(tac->src1, 1));
            if (dst_eq_src1) {
                // Special case where src1 is the same as dst, in that case, r10/xmm14 contains the result.
                int spill_register = get_spill_register(tac->dst, 1);
                add_spill_store(tac, tac->dst, spill_register);
                tac = tac->next;
            }
        }

        if (tac->src2 && tac->src2->spilled) {
            if (debug_instsel_spilling) printf("Adding spill load\n");
            add_spill_load(tac, 2, get_spill_register(tac->src2, 2));
        }

        if (tac->dst && tac->dst->spilled) {
            if (debug_instsel_spilling) printf("Adding spill store\n");
            if (tac->operation == X_CALL) panic("Unexpected spill from X_CALL");
            add_spill_store(tac, tac->dst, get_spill_register(tac->dst, 2));
            tac = tac->next;
        }
    }
}
