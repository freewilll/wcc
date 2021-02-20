#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "wc4.h"

enum {
    MAX_INSTRUCTION_GRAPH_EDGE_COUNT = 64,
    MAX_INSTRUCTION_GRAPH_CHOICE_NODE_COUNT = 1024,
    MAX_INSTRUCTION_GRAPH_CHOICE_EDGE_COUNT = 1024,
    MAX_CHOICE_TRAIL_COUNT = 32,
};

IGraph *eis_igraphs;            // The current block's igraphs
int eis_instr_count;            // The current block's instruction count

Graph *cost_graph;              // Graph of all possible options when tiling
int cost_graph_node_count;      // Number of nodes in cost graph
int *cost_to_igraph_map;        // Mapping of a cost graph node back to the instruction graph node
int *cost_rules;                // Mapping of cost graph node id to x86 instruction rule id
int *accumulated_cost;          // Total cost of sub tree of a cost graph node
Set **igraph_labels;            // Matched instruction rule ids for a igraph node id

int recursive_tile_igraphs(IGraph *igraph, int node_id);

void set_assign_to_reg_lvalue_dsts(Function *function) {
    Tac *tac;

    tac = function->ir;
    while (tac) {
        if (tac->operation == IR_MOVE_TO_REG_LVALUE) {
            tac->src1 = dup_value(tac->src1);
            tac->src1->type += TYPE_PTR;
            tac->src1->is_lvalue = 0;
            tac->dst = tac->src1;
        }
        else {
            if (tac->src1 && tac->src1->is_lvalue && !(tac->src1->global_symbol || tac->src1->local_index)) {
                tac->src1->type += TYPE_PTR;
                tac->src1->is_lvalue = 0;
            }
            if (tac->src2 && tac->src2->is_lvalue && !(tac->src2->global_symbol || tac->src2->local_index)) {
                tac->src2->type += TYPE_PTR;
                tac->src2->is_lvalue = 0;
            }
        }

        tac = tac->next;
    }
}

void recursive_dump_igraph(IGraph *ig, int node, int indent) {
    int i, operation;
    Graph *g;
    IGraphNode *ign;
    GraphEdge *e;
    Tac *t;

    ign = &(ig->nodes[node]);

    printf("%3d ", node);
    for (i = 0; i < indent; i++) printf("  ");

    if (ign->tac) {
        operation = ign->tac->operation;

             if (operation == IR_MOVE)                 printf("= (move)\n");
        else if (operation == IR_ADD)                  printf("+\n");
        else if (operation == IR_SUB)                  printf("-\n");
        else if (operation == IR_MUL)                  printf("*\n");
        else if (operation == IR_DIV)                  printf("/\n");
        else if (operation == IR_BNOT)                 printf("~\n");
        else if (operation == IR_BSHL)                 printf("<<\n");
        else if (operation == IR_BSHR)                 printf(">>\n");
        else if (operation == IR_INDIRECT)             printf("indirect\n");
        else if (operation == IR_ADDRESS_OF)           printf("&\n");
        else if (operation == IR_CAST)                 printf("= (cast)\n");
        else if (operation == IR_MOVE_TO_REG_LVALUE) printf("assign to lvalue\n");
        else if (operation == IR_NOP)                  printf("noop\n");
        else if (operation == IR_RETURN)               printf("return\n");
        else if (operation == IR_START_CALL)           printf("start call\n");
        else if (operation == IR_END_CALL)             printf("end call\n");
        else if (operation == IR_ARG)                  printf("arg\n");
        else if (operation == IR_CALL)                 printf("call\n");
        else if (operation == IR_START_LOOP)           printf("start loop");
        else if (operation == IR_END_LOOP)             printf("end loop");
        else if (operation == IR_JZ)                   printf("jz\n");
        else if (operation == IR_JNZ)                  printf("jnz\n");
        else if (operation == IR_JMP)                  printf("jmp\n");
        else if (operation == IR_EQ)                   printf("==\n");
        else if (operation == IR_NE)                   printf("!=\n");
        else if (operation == IR_LT)                   printf("<\n");
        else if (operation == IR_GT)                   printf(">\n");
        else if (operation == IR_LE)                   printf(">=\n");
        else if (operation == IR_GE)                   printf("<=\n");

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

void dup_inode(IGraphNode *src, IGraphNode *dst) {
    dst->tac = src->tac;
    dst->value = src->value;
}

IGraph *shallow_dup_igraph(IGraph *src, IGraph *dst) {
    dst->nodes = src->nodes;
    dst->graph = src->graph;
    dst->node_count = src->node_count;
}

// Merge g2 into g1. The merge point is vreg
IGraph *merge_igraphs(IGraph *g1, IGraph *g2, int vreg) {
    int i, j, operation, node_count, d, join_from, join_to, from, to, type_change;
    int type;
    IGraph *g;
    IGraphNode *inodes, *g1_inodes, *inodes2, *in, *in1, *in2;
    GraphNode *n, *n2;
    GraphEdge *e, *e2;
    Value *v;
    Tac *tac;

    if (DEBUG_INSTSEL_TREE_MERGING) {
        printf("g1 dst=%d\n", g1->nodes[0].tac->dst ? g1->nodes[0].tac->dst->vreg : -1);
        dump_igraph(g1);
        printf("g2 dst=%d\n", g2->nodes[0].tac->dst ? g2->nodes[0].tac->dst->vreg : -1);
        dump_igraph(g2);
        printf("\n");
    }

    g = malloc(sizeof(IGraph));
    memset(g, 0, sizeof(IGraph));

    node_count = g1->node_count + g2->node_count;
    inodes = malloc(node_count * sizeof(IGraphNode));
    memset(inodes, 0, node_count * sizeof(IGraphNode));

    g->nodes = inodes;
    g->graph = new_graph(node_count, MAX_INSTRUCTION_GRAPH_EDGE_COUNT);
    g->node_count = node_count;
    join_from = -1;
    join_to = -1;

    if (g1->node_count == 0) panic("Unexpectedly got 0 g1->node_count");

    if (DEBUG_INSTSEL_TREE_MERGING_DEEP) printf("g1->node_count=%d\n", g1->node_count);
    if (DEBUG_INSTSEL_TREE_MERGING_DEEP) printf("g2->node_count=%d\n", g2->node_count);

    for (i = 0; i < g1->node_count; i++) {
        if (DEBUG_INSTSEL_TREE_MERGING_DEEP) printf("Copying g1 %d to %d\n", i, i);
        dup_inode(&(g1->nodes[i]), &(inodes[i]));

        g1_inodes = g1->nodes;
        n = &(g1->graph->nodes[i]);

        e = n->succ;

        while (e) {
            in = &(g1_inodes[e->to->id]);
            if (in->value && in->value->vreg == vreg) {
                in1 = in;
                join_from = e->from->id;
                join_to = e->to->id;
                if (DEBUG_INSTSEL_TREE_MERGING_DEEP) printf("Adding join edge %d -> %d\n", join_from, join_to);
                add_graph_edge(g->graph, join_from, join_to);
            }
            else {
                if (DEBUG_INSTSEL_TREE_MERGING_DEEP) printf("Adding g1 edge %d -> %d\n", e->from->id, e->to->id);
                add_graph_edge(g->graph, e->from->id, e->to->id);
            }

            e = e->next_succ;
        }
    }

    if (join_from == -1 || join_to == -1) panic("Attempt to join two trees without a join node");

    // The g2 graph starts at g1->node_count
    for (i = 0; i < g2->node_count; i++) {
        d = g1->node_count + i;
        if (DEBUG_INSTSEL_TREE_MERGING_DEEP) printf("Copying g2 node from %d to %d\n", i, d);
        dup_inode(&(g2->nodes[i]), &(inodes[d]));

        n2 = &(g2->graph->nodes[i]);
        e = n2->succ;
        while (e) {
            from = e->from->id + g1->node_count ;
            to = e->to->id + g1->node_count;
            if (DEBUG_INSTSEL_TREE_MERGING_DEEP) printf("Adding g2 edge %d -> %d\n", from, to);
            add_graph_edge(g->graph, from, to);

            e = e->next_succ;
        }
    }

    if (DEBUG_INSTSEL_TREE_MERGING_DEEP) printf("\n");

    in2 = &(g2->nodes[0]);
    type_change = in1->value->type != in2->tac->dst->type;

    if (type_change) {
        if (DEBUG_INSTSEL_TREE_MERGING) printf("Changing type from %d -> %d\n", in2->tac->dst->type, in1->value->type);
        if (DEBUG_INSTSEL_TREE_MERGING) printf("Replacing %d with IR_CAST tac\n", join_to);
        in = &(inodes[join_to]);
        in->tac = new_instruction(IR_CAST);
        in->tac->src1 = in2->tac->dst;
        in->tac->dst = in1->value;

        if (DEBUG_INSTSEL_TREE_MERGING) print_instruction(stdout, in->tac);

        if (DEBUG_INSTSEL_TREE_MERGING) printf("Adding inter graph join edge %d -> %d\n", join_to, g1->node_count);
        add_graph_edge(g->graph, join_to, g1->node_count);
    }
    else {
        if (DEBUG_INSTSEL_TREE_MERGING_DEEP) printf("Repointing inter graph join node %d->%d to %d->%d\n", join_from, join_to, join_from, g1->node_count);
        e = g->graph->nodes[join_from].succ;
        if (e->to->id != join_to) e = e->next_succ;
        e->to = &(g->graph->nodes[g1->node_count]);
    }

    if (DEBUG_INSTSEL_TREE_MERGING) {
        printf("\ng:\n");
        dump_igraph(g);
    }

    return g;
}

int igraphs_are_neighbors(IGraph *igraphs, int i1, int i2) {
    VregIGraph *g1, *g2;

    i1++;
    if (i1 > i2) panic("Internal error: ordering issue in igraphs_are_neighbors()");

    while (i1 <= i2) {
        if (i1 == i2) return 1;
        if (igraphs[i1].node_count != 0) return 0;
        i1++;
    }

    return 1;
}

void make_igraphs(Function *function, int block_id) {
    int instr_count, i, j, node_count, vreg_count;
    int dst, src1, src2, g1_igraph_id;
    Block *blocks;
    Tac *tac;
    IGraph *igraphs, *ig;
    IGraphNode *nodes;
    Set *liveout;
    Graph *graph;
    VregIGraph* vreg_igraphs;
    Value *v;

    blocks = function->blocks;

    vreg_count = 0;
    instr_count = 0;

    tac = blocks[block_id].start;
    while (1) {
        if (DEBUG_INSTSEL_TREE_MERGING) print_instruction(stdout, tac);

        if (tac->src1 && tac->src1->vreg && tac->src1->vreg > vreg_count) vreg_count = tac->src1->vreg;
        if (tac->src2 && tac->src2->vreg && tac->src2->vreg > vreg_count) vreg_count = tac->src2->vreg;
        if (tac-> dst && tac->dst ->vreg && tac->dst ->vreg > vreg_count) vreg_count = tac->dst ->vreg;

        instr_count++;

        if (tac == blocks[block_id].end) break;
        tac = tac->next;
    }

    igraphs = malloc(instr_count * sizeof(IGraph));

    i = 0;
    tac = blocks[block_id].start;
    while (tac) {
        node_count = 1;

        if (tac->src1) node_count++;
        if (tac->src2) node_count++;

        nodes = malloc(node_count * sizeof(IGraphNode));
        memset(nodes, 0, node_count * sizeof(IGraphNode));

        graph = new_graph(node_count, MAX_INSTRUCTION_GRAPH_EDGE_COUNT);

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
    liveout = function->liveout[block_id];

    if (liveout->max_value > vreg_count) vreg_count = liveout->max_value;

    vreg_igraphs = malloc((vreg_count + 1) * sizeof(VregIGraph));
    memset(vreg_igraphs, 0, (vreg_count + 1) * sizeof(VregIGraph));

    for (i = 0; i <= liveout->max_value; i++) {
        if (!liveout->elements[i]) continue;
        vreg_igraphs[i].count++;
        vreg_igraphs[i].igraph_id = -1;
    }

    i = instr_count - 1;
    tac = blocks[block_id].end;
    while (tac) {
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

        if (tac->operation != IR_MOVE_TO_REG_LVALUE && dst && (src1 && dst == src1) || (src2 && dst == src2)) {
            print_instruction(stdout, tac);
            panic("Illegal assignment of src1/src2 to dst");
        }

        if (src1 && src2 && src1 == src2) {
            print_instruction(stdout, tac);
            panic("src1 == src2 not handled");
        }

        g1_igraph_id = vreg_igraphs[dst].igraph_id;

        // If dst is only used once and it's not in liveout, merge it.
        // Also, don't merge IR_CALLs. The IR_START_CALL and IR_END_CALL contraints don't permit
        // rearranging function calls without dire dowmstream side effects.
        if (vreg_igraphs[dst].count == 1 && vreg_igraphs[dst].igraph_id != -1 &&
            tac->operation != IR_CALL && tac->operation != IR_MOVE_TO_REG_LVALUE &&
            igraphs_are_neighbors(igraphs, i, g1_igraph_id)) {

            if (DEBUG_INSTSEL_TREE_MERGING) {
                printf("\nMerging dst=%d src1=%d src2=%d ", dst, src1, src2);
                printf("in locs %d and %d on vreg=%d\n----------------------------------------------------------\n", g1_igraph_id, i, dst);
            }
            ig = merge_igraphs(&(igraphs[g1_igraph_id]), &(igraphs[i]), dst);

            igraphs[g1_igraph_id].nodes = ig->nodes;
            igraphs[g1_igraph_id].graph = ig->graph;
            igraphs[g1_igraph_id].node_count = ig->node_count;

            igraphs[i].node_count = 0;

            if (src1) vreg_igraphs[src1].igraph_id = g1_igraph_id;
            if (src2) vreg_igraphs[src2].igraph_id = g1_igraph_id;
        }

        if (dst && tac->operation != IR_MOVE_TO_REG_LVALUE)
            vreg_igraphs[dst].count = 0;

        i--;

        if (tac == blocks[block_id].start) break;
        tac = tac->prev;
    }

    if (DEBUG_INSTSEL_TREE_MERGING)
        printf("\n=================================\n");

    if (DEBUG_INSTSEL_TREE_MERGING) {
        for (i = 0; i < instr_count; i++) {
            if (!igraphs[i].node_count) continue;
            tac = igraphs[i].nodes[0].tac;
            if (tac && tac->operation == IR_NOP) continue;
            if (tac) {
                v = igraphs[i].nodes[0].tac->dst;
                if (v) {
                    print_value(stdout, v, 0);
                    printf(" = \n");
                }
            }
            dump_igraph(&(igraphs[i]));
        }
    }

    eis_igraphs = igraphs;
    eis_instr_count = instr_count;
}

void recursive_simplify_igraph(IGraph *src, IGraph *dst, int src_node_id, int dst_parent_node_id, int *dst_node_id) {
    int operation;
    Tac *tac;
    IGraphNode *ign;
    GraphEdge *e;

    ign = &(src->nodes[src_node_id]);

    tac = ign->tac;
    if (tac) operation = tac->operation; else operation = 0;

    e = src->graph->nodes[src_node_id].succ;

    if (operation == IR_MOVE) {
        if (src_node_id != 0) {
            if (tac->dst->type == tac->src1->type) {
                recursive_simplify_igraph(src, dst, e->to->id, dst_parent_node_id, dst_node_id);
                return;
            }
        }
    }

    dup_inode(&(src->nodes[src_node_id]), &(dst->nodes[*dst_node_id]));
    if (dst_parent_node_id != -1) add_graph_edge(dst->graph, dst_parent_node_id, *dst_node_id);

    dst_parent_node_id = *dst_node_id;
    (*dst_node_id)++;

    while (e) {
        recursive_simplify_igraph(src, dst, e->to->id, dst_parent_node_id, dst_node_id);
        e = e->next_succ;
    }
}

IGraph *simplify_igraph(IGraph *src) {
    int i, j, operation, node_count, d, join_from, join_to, from, to, type_change;
    int type, dst_node_id;
    IGraph *dst;
    IGraphNode *inodes;

    dst = malloc(sizeof(IGraph));
    memset(dst, 0, sizeof(IGraph));

    node_count = src->node_count;
    inodes = malloc(node_count * sizeof(IGraphNode));
    memset(inodes, 0, node_count * sizeof(IGraphNode));

    dst->nodes = inodes;
    dst->graph = new_graph(node_count, MAX_INSTRUCTION_GRAPH_EDGE_COUNT);
    dst->node_count = node_count;

    if (DEBUG_INSTSEL_IGRAPH_SIMPLIFICATION) {
        printf("simplify_igraph() on:\n");
        dump_igraph(src);
    }

    dst_node_id = 0;
    recursive_simplify_igraph(src, dst, 0, -1, &dst_node_id);
    if (DEBUG_INSTSEL_IGRAPH_SIMPLIFICATION) printf("\n");

    if (DEBUG_INSTSEL_IGRAPH_SIMPLIFICATION) {
        printf("Result:\n");
        dump_igraph(dst);
        printf("\n");
    }

    dst->node_count = dst_node_id;

    return dst;
}

void simplify_igraphs() {
    int i, operation;
    IGraphNode *ign;
    IGraph *ig;

    for (i = 0; i < eis_instr_count; i++) {
        ign = &(eis_igraphs[i].nodes[0]);
        if (ign->tac) operation = ign->tac->operation; else operation = 0;
        if (operation != IR_NOP && eis_igraphs[i].node_count) {
            ig = simplify_igraph(&(eis_igraphs[i]));
            shallow_dup_igraph(ig, &(eis_igraphs[i]));
        }
    }
}

// Convert an instruction graph node from an operation that puts the result
// into a register to an assigment, using a constant value.
Value *merge_cst_node(IGraph *igraph, int node_id, long constant_value) {
    int operation;
    GraphNode *src_node;
    Value *v;

    v = new_value();
    v->type = TYPE_LONG;
    v->is_constant = 1;
    v->value = constant_value;

    src_node = &(igraph->graph->nodes[node_id]);

    if (node_id == 0) {
        // Root node, convert it into a move, since it has to end up
        // in a register
        igraph->nodes[node_id].tac->operation = IR_MOVE;
        igraph->nodes[node_id].tac->src1 = v;
        igraph->nodes[node_id].tac->src2 = 0;
        igraph->nodes[src_node->succ->to->id].value->is_constant = 1;
        igraph->nodes[src_node->succ->to->id].value->value = v->value;
        src_node->succ->next_succ = 0;

    }
    else {
        // Not root-node, convert it from a tac into a value node
        igraph->nodes[node_id].tac = 0;
        igraph->nodes[node_id].value = v;
    }

    return v;
}

Value *recursive_merge_constants(IGraph *igraph, int node_id) {
    int i, j, operation, cst1, cst2;
    GraphNode *src_node;
    GraphEdge *e;
    Tac *tac;
    Value *v, *src1, *src2;

    src_node = &(igraph->graph->nodes[node_id]);
    tac = igraph->nodes[node_id].tac;
    if (!tac) return igraph->nodes[node_id].value;
    operation = tac->operation;
    if (operation == IR_NOP) return 0;

    j = 1;
    src1 = src2 = 0;
    e = src_node->succ;
    while (e) {
        v = recursive_merge_constants(igraph, e->to->id);
        if (j == 1) src1 = v; else src2 = v;
        e = e->next_succ;
        j++;
    }

    cst1 = src1 && src1->is_constant;
    cst2 = cst1 && (src2 && src2->is_constant);

         if (operation == IR_BNOT && cst1) return merge_cst_node(igraph, node_id, ~src1->value);
    else if (operation == IR_ADD  && cst2) return merge_cst_node(igraph, node_id, src1->value +  src2->value);
    else if (operation == IR_SUB  && cst2) return merge_cst_node(igraph, node_id, src1->value -  src2->value);
    else if (operation == IR_MUL  && cst2) return merge_cst_node(igraph, node_id, src1->value *  src2->value);
    else if (operation == IR_DIV  && cst2) return merge_cst_node(igraph, node_id, src1->value /  src2->value);
    else if (operation == IR_MOD  && cst2) return merge_cst_node(igraph, node_id, src1->value %  src2->value);
    else if (operation == IR_BAND && cst2) return merge_cst_node(igraph, node_id, src1->value &  src2->value);
    else if (operation == IR_BOR  && cst2) return merge_cst_node(igraph, node_id, src1->value |  src2->value);
    else if (operation == IR_XOR  && cst2) return merge_cst_node(igraph, node_id, src1->value ^  src2->value);
    else if (operation == IR_EQ   && cst2) return merge_cst_node(igraph, node_id, src1->value == src2->value);
    else if (operation == IR_NE   && cst2) return merge_cst_node(igraph, node_id, src1->value != src2->value);
    else if (operation == IR_LT   && cst2) return merge_cst_node(igraph, node_id, src1->value <  src2->value);
    else if (operation == IR_GT   && cst2) return merge_cst_node(igraph, node_id, src1->value >  src2->value);
    else if (operation == IR_LE   && cst2) return merge_cst_node(igraph, node_id, src1->value <= src2->value);
    else if (operation == IR_GE   && cst2) return merge_cst_node(igraph, node_id, src1->value >= src2->value);
    else if (operation == IR_BSHL && cst2) return merge_cst_node(igraph, node_id, src1->value << src2->value);
    else if (operation == IR_BSHR && cst2) return merge_cst_node(igraph, node_id, src1->value >> src2->value);

    else
        return 0;

}

// Walk all instruction trees and merge nodes where there are constant operations
void merge_constants(Function *function) {
    int i;

    for (i = 0; i < eis_instr_count; i++) {
        if (!eis_igraphs[i].node_count) continue;
        recursive_merge_constants(&(eis_igraphs[i]), 0);
    }
}

int rules_match(int parent, Rule *child, int allow_downsize) {
    if (!allow_downsize) return parent == child->non_terminal;

    if (parent == child->non_terminal) return 1;

    if (parent == REGB && child->non_terminal == REGW) return 1;
    if (parent == REGB && child->non_terminal == REGL) return 1;
    if (parent == REGB && child->non_terminal == REGQ) return 1;
    if (parent == REGW && child->non_terminal == REGL) return 1;
    if (parent == REGW && child->non_terminal == REGQ) return 1;
    if (parent == REGL && child->non_terminal == REGQ) return 1;

    return 0;
}

int non_terminal_for_constant_value(Value *v) {
    if (v->value >= -2147483648 && v->value <= 2147483647)
        return CSTL;
    else
        return CSTQ;
}

int non_terminal_for_value(Value *v) {
    int adr_base;
    int result;

    if (!v->x86_size) make_value_x86_size(v);
    if (v->non_terminal) return v->non_terminal;

    adr_base = v->vreg ? ADR : MDR;

         if (v->is_constant)                                        result =  non_terminal_for_constant_value(v);
    else if (v->is_string_literal)                                  result =  STL;
    else if (v->label)                                              result =  LAB;
    else if (v->function_symbol)                                    result =  FUN;
    else if (v->type == TYPE_PTR + TYPE_VOID)                       result =  adr_base + 5; // *void
    else if (v->type >= TYPE_PTR + TYPE_PTR)                        result =  adr_base + 4; // **...
    else if (v->type >= TYPE_PTR + TYPE_STRUCT)                     result =  adr_base + 5; // *void
    else if (v->type >= TYPE_PTR)                                   result =  adr_base + value_ptr_target_x86_size(v);
    else if (v->is_lvalue_in_register)                              result =  ADR + v->x86_size;
    else if (v->global_symbol || v->local_index || v->stack_index)  result =  MEM + v->x86_size;
    else if (v->vreg)                                               result =  REG + v->x86_size;
    else {
        print_value(stdout, v, 0);
        panic("Bad value in non_terminal_for_value()");
    }

    v->non_terminal = result;

    return result;
}

int match_value_to_rule_src(Value *v, int src) {
    return non_terminal_for_value(v) == src;
}

int match_value_to_rule_dst(Value *v, Rule *r) {
    return rules_match(non_terminal_for_value(v), r, 1);
}

// Print the cost graph, only showing choices where parent src and child dst match
void recursive_print_cost_graph(Graph *cost_graph, int *cost_rules, int *accumulated_cost, int node_id, int parent_node_id, int parent_src, int indent) {
    int i, j, choice_node_id, src, match;
    GraphNode *choice_node;
    GraphEdge *choice_edge, *e;
    Rule *parent_rule;

    choice_edge = cost_graph->nodes[node_id].succ;
    while (choice_edge) {
        choice_node_id = choice_edge->to->id;
        choice_node = &(cost_graph->nodes[choice_node_id]);

        match = 1;
        if (parent_node_id != -1) {
            parent_rule = &(instr_rules[cost_rules[parent_node_id]]);
            src = (parent_src == 1) ? parent_rule->src1 : parent_rule->src2;
            match = rules_match(src, &(instr_rules[cost_rules[choice_node_id]]), 0);
            if (match) {
                printf("%-3d ", choice_node_id);
                for (i = 0; i < indent; i++) printf("  ");
                printf("%d ", cost_rules[choice_node_id]);
            }
        }
        else
            printf("%d ", cost_rules[choice_node_id]);

        if (match) {
            printf(" (%d)\n", accumulated_cost[choice_node_id]);

            e = choice_node->succ;
            j = 1;
            while (e) {
                printf("%-3d ", choice_node_id);
                for (i = 0; i < indent + 1; i++) printf("  ");
                printf("src%d\n", j);
                recursive_print_cost_graph(cost_graph, cost_rules, accumulated_cost, e->to->id, choice_node_id, j, indent + 2);

                e = e->next_succ;
                j++;
            }
        }

        choice_edge = choice_edge->next_succ;
    }
}

void print_cost_graph(Graph *cost_graph, int *cost_rules, int *accumulated_cost) {
    recursive_print_cost_graph(cost_graph, cost_rules, accumulated_cost, 0, -1, -1, 0);
}

int new_cost_graph_node() {
    if (cost_graph_node_count == MAX_INSTRUCTION_GRAPH_CHOICE_NODE_COUNT)
        panic("MAX_INSTRUCTION_GRAPH_CHOICE_NODE_COUNT");
    return cost_graph_node_count++;
}

int tile_igraph_operand_less_node(IGraph *igraph, int node_id) {
    int i, cost_graph_node_id, choice_node_id;
    Tac *tac;
    Rule *r;

    if (DEBUG_INSTSEL_TILING) printf("tile_igraph_operand_less_node on node=%d\n", node_id);

    cost_graph_node_id = new_cost_graph_node();
    cost_to_igraph_map[cost_graph_node_id] = node_id;

    tac = igraph->nodes[node_id].tac;
    for (i = 0; i < instr_rule_count; i++) {
        r = &(instr_rules[i]);
        if (r->operation == tac->operation) {
            if (DEBUG_INSTSEL_TILING) {
                printf("matched rule %-4d: ", i);
                print_rule(r, 0);
            }

            choice_node_id = new_cost_graph_node();
            cost_to_igraph_map[choice_node_id] = node_id;
            cost_rules[choice_node_id] = i;
            accumulated_cost[choice_node_id] = instr_rules[i].cost;

            add_graph_edge(cost_graph, cost_graph_node_id, choice_node_id);

            return cost_graph_node_id;
        }
    }

    dump_igraph(igraph);
    panic("Did not match any rules");
}

int tile_igraph_leaf_node(IGraph *igraph, int node_id) {
    int i,  cost_graph_node_id, choice_node_id, matched, matched_dst;
    Value *v;
    Rule *r;
    Tac *tac;

    if (DEBUG_INSTSEL_TILING) dump_igraph(igraph);

    cost_graph_node_id = new_cost_graph_node();
    cost_to_igraph_map[cost_graph_node_id] = node_id;

    v = igraph->nodes[node_id].value;

    // Find a matching instruction
    matched = 0;
    for (i = 0; i < instr_rule_count; i++) {
        r = &(instr_rules[i]);
        if (r->operation) continue;

        if (match_value_to_rule_src(v, r->src1)) {
            if (DEBUG_INSTSEL_TILING) {
                printf("matched rule %-4d: ", i);
                print_rule(r, 0);
            }
            add_to_set(igraph_labels[node_id], i);

            choice_node_id = new_cost_graph_node();
            cost_to_igraph_map[choice_node_id] = node_id;
            cost_rules[choice_node_id] = i;
            accumulated_cost[choice_node_id] = instr_rules[i].cost;

            add_graph_edge(cost_graph, cost_graph_node_id, choice_node_id);

            matched = 1;
        }
    }

    if (!matched) {
        dump_igraph(igraph);
        panic("Did not match any rules");
    }

    return cost_graph_node_id;
}

int tile_igraph_operation_node(IGraph *igraph, int node_id) {
    int i, j, operation, src1_id, src2_id, mv;
    int matched, matched_src;
    int choice_node_id, src1_cost_graph_node_id, src2_cost_graph_node_id;
    int cost_graph_node_id, min_cost, min_cost_src1, min_cost_src2, src, cost;
    int *cached_elements, rule_src1, rule_src2;
    Tac *tac;
    IGraphNode *inode, *inodes;
    GraphEdge *e;
    Value *src1, *src2, *v;
    Rule *r, *child_rule;

    if (DEBUG_INSTSEL_TILING) printf("tile_igraph_operation_node on node=%d\n", node_id);

    inodes = igraph->nodes;
    inode = &(igraph->nodes[node_id]);
    tac = inode->tac;
    operation = tac->operation;

    if (DEBUG_INSTSEL_TILING) {
        print_instruction(stdout, tac);
        dump_igraph(igraph);
    }

    src1_id = src2_id = 0;
    src1 = src2 = 0;

    e = igraph->graph->nodes[node_id].succ;
    if (e) {
        src1_id = e->to->id;
        src1 = inodes[src1_id].value;
        e = e->next_succ;

        if (e) {
            src2_id = e->to->id;
            src2 = inodes[src2_id].value;
        }
    }

    if (!src1_id) return tile_igraph_operand_less_node(igraph, node_id);

    cost_graph_node_id = new_cost_graph_node();
    cost_to_igraph_map[cost_graph_node_id] = node_id;

    src1_cost_graph_node_id = recursive_tile_igraphs(igraph, src1_id);
    if (src2_id) src2_cost_graph_node_id = recursive_tile_igraphs(igraph, src2_id);

    if (DEBUG_INSTSEL_TILING) {
        printf("back from recursing at node=%d\n\n", node_id);
        printf("src1 labels: "); print_set(igraph_labels[src1_id]); printf(" ");
        printf("\n");

        if (src2_id) {
            printf("src2 labels: ");
            print_set(igraph_labels[src2_id]);
            printf("\n");
        }
    }

    if (DEBUG_INSTSEL_TILING && tac->dst)
        printf("Want dst %s\n", non_terminal_string(non_terminal_for_value(tac->dst)));

    if (src1_id) cache_set_elements(igraph_labels[src1_id]);
    if (src2_id) cache_set_elements(igraph_labels[src2_id]);

    // Loop over all rules until a match is found
    matched = 0;
    for (i = 0; i < instr_rule_count; i++) {
        r = &(instr_rules[i]);

        if (r->operation != operation) continue;
        if (src2_id && !r->src2) continue;
        if (!src2_id && r->src2) continue;

        // If this is the top level, ensure the dst matches
        if (tac->dst && node_id == 0 && !match_value_to_rule_dst(tac->dst, r)) continue;

        // If the rule requires dst matching, do it.
        if (tac->dst && r->match_dst && !match_value_to_rule_dst(tac->dst, r)) continue;

        // Check dst of the subtree tile matches what is needed
        rule_src1 = r->src1;
        rule_src2 = r->src2;

        matched_src = 0;
        mv = igraph_labels[src1_id]->cached_element_count;
        cached_elements = igraph_labels[src1_id]->cached_elements;
        for (j = 0; j < mv; j++) {
            if (instr_rules[cached_elements[j]].non_terminal == r->src1) {
                matched_src = 1;
                break;
            }
        }
        if (!matched_src) continue;

        matched_src = 0;
        if (src2_id) {
            matched_src = 0;
            mv = igraph_labels[src2_id]->cached_element_count;
            cached_elements = igraph_labels[src2_id]->cached_elements;
            for (j = 0; j < mv; j++) {
                if (instr_rules[cached_elements[j]].non_terminal == r->src2) {
                    matched_src = 1;
                    break;
                }
            }
            if (!matched_src) continue;
        }

        // We have a match, do some accounting ...
        matched = 1;

        if (DEBUG_INSTSEL_TILING) {
            printf("matched rule %-4d: ", i);
            print_rule(r, 0);
        }
        add_to_set(igraph_labels[node_id], i);

        choice_node_id = new_cost_graph_node();
        cost_rules[choice_node_id] = i;

        add_graph_edge(cost_graph, cost_graph_node_id, choice_node_id);
        add_graph_edge(cost_graph, choice_node_id, src1_cost_graph_node_id);
        if (src2_id) add_graph_edge(cost_graph, choice_node_id, src2_cost_graph_node_id);

        // Find the minimal cost sub trees
        min_cost_src1 = min_cost_src2 = 100000000;

        for (j = 1; j <= 2; j++) {
            if (!src2_id && j == 2) continue; // Unary operation

            src = j == 1 ? r->src1 : r->src2;
            e = j == 1
                ? cost_graph->nodes[src1_cost_graph_node_id].succ
                : cost_graph->nodes[src2_cost_graph_node_id].succ;

            min_cost = 100000000;
            while (e) {
                child_rule = &(instr_rules[cost_rules[e->to->id]]);

                if (rules_match(src, child_rule, 0)) {
                    cost = accumulated_cost[e->to->id];
                    if (cost < min_cost) min_cost = cost;
                }

                e = e->next_succ;
            }

            if (j == 1)
                min_cost_src1 = min_cost;
            else
                min_cost_src2 = min_cost;
        }

        if (!src2_id) min_cost_src2 = 0;

        accumulated_cost[choice_node_id] = min_cost_src1 + min_cost_src2 + instr_rules[i].cost;
    }

    if (!matched) {
        printf("\nNo rules matched\n");
        if (tac->dst) printf("Want dst %s\n", non_terminal_string(non_terminal_for_value(tac->dst)));
        print_instruction(stdout, tac);
        dump_igraph(igraph);
        exit(1);
    }

    return cost_graph_node_id;
}

// Algorithm from page 618 of Engineering a compiler
// with additional minimal cost tiling determination as suggested on page 619.
int recursive_tile_igraphs(IGraph *igraph, int node_id) {
    Tac *tac;

    if (DEBUG_INSTSEL_TILING) printf("\nrecursive_tile_igraphs on node=%d\n", node_id);

    if (!igraph->nodes[node_id].tac)
        return tile_igraph_leaf_node(igraph, node_id);
    else
        return tile_igraph_operation_node(igraph, node_id);
}

// Add instructions to the intermediate representation by doing a post-order walk over the cost tree, picking the matching rules with lowest cost
Value *recursive_make_intermediate_representation(IGraph *igraph, int node_id, int parent_node_id, int parent_src) {
    int i, pv, choice_node_id, match;
    int least_expensive_choice_node_id, min_cost, cost, igraph_node_id, dst_type;
    GraphEdge *choice_edge, *e;
    Rule *parent_rule, *rule;
    IGraphNode *ign;
    Value *src, *dst, *src1, *src2, *x86_dst, *x86_v1, *x86_v2;
    X86Operation *x86op;
    Tac *tac;

    min_cost = 100000000;
    least_expensive_choice_node_id = -1;
    choice_edge = cost_graph->nodes[node_id].succ;
    while (choice_edge) {
        choice_node_id = choice_edge->to->id;

        if (parent_node_id != -1) {
            // Ensure src and dst match for non-root nodes
            parent_rule = &(instr_rules[cost_rules[parent_node_id]]);
            pv = (parent_src == 1) ? parent_rule->src1 : parent_rule->src2;
            match = rules_match(pv, &(instr_rules[cost_rules[choice_node_id]]), 0);
        }
        else
            match = 1;

        if (match) {
            cost = accumulated_cost[choice_node_id];
            if (cost < min_cost) {
                min_cost = cost;
                least_expensive_choice_node_id = choice_node_id;
            }
        }

        choice_edge = choice_edge->next_succ;
    }

    if (least_expensive_choice_node_id == -1)
        panic("Internal error: No matched choices in recursive_make_intermediate_representation");

    igraph_node_id = cost_to_igraph_map[least_expensive_choice_node_id];
    ign = &(igraph->nodes[igraph_node_id]);

    // Go down children, which return the value the result is in
    e = cost_graph->nodes[least_expensive_choice_node_id].succ;
    i = 1;
    src1 = ign->value;
    src2 = 0;

    while (e) {
        src = recursive_make_intermediate_representation(igraph, e->to->id, least_expensive_choice_node_id, i);
        if (i == 1)
            src1 = src;
        else
            src2 = src;

        e = e->next_succ;
        i++;
    }

    rule = &(instr_rules[cost_rules[least_expensive_choice_node_id]]);

    if (DEBUG_INSTSEL_TILING) {
        printf("Generating instructions for rule %-4d: ", rule->index);
        print_rule(rule, 0);
    }

    dst = 0;
    if (parent_node_id == -1) {
        // Use the root node tac or value as a result
        if (ign->tac) {
            if (DEBUG_INSTSEL_TILING)
                printf("  using root vreg for dst %p vreg=%d\n", ign->tac->dst, ign->tac->dst ? ign->tac->dst->vreg : -1);
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
            dst->vreg = new_vreg();
            dst->ssa_subscript = -1;

            if (DEBUG_INSTSEL_TILING) printf("  allocated new vreg for dst vreg=%d\n", dst->vreg);
        }
        else {
            if (DEBUG_INSTSEL_TILING) printf("  using passthrough dst\n");
            dst = src1; // Passthrough for cst or reg
        }
    }

    // In order for the spill code to generate correct size spills,
    // x86_size has to be set to match what the x86 instructions
    // are doing. This cannot be done from the type since the type
    // reflects what the parser has produced, and doesn't necessarily
    // match what the x86 code is doing.
    if (dst)  dst->x86_size  = make_x86_size_from_non_terminal(rule->non_terminal);
    if (src1) src1->x86_size = make_x86_size_from_non_terminal(rule->src1);
    if (src2) src2->x86_size = make_x86_size_from_non_terminal(rule->src2);

    x86op = rule->x86_operations;
    while (x86op) {
             if (x86op->dst == 0)    x86_dst = 0;
        else if (x86op->dst == SRC1) x86_dst = src1;
        else if (x86op->dst == SRC2) x86_dst = src2;
        else if (x86op->dst == DST)  x86_dst = dst;
        else panic1d("Unknown operand to x86 instruction: %d", x86op->v1);

             if (x86op->v1 == 0)    x86_v1 = 0;
        else if (x86op->v1 == SRC1) x86_v1 = src1;
        else if (x86op->v1 == SRC2) x86_v1 = src2;
        else if (x86op->v1 == DST)  x86_v1 = dst;
        else panic1d("Unknown operand to x86 instruction: %d", x86op->v1);

             if (x86op->v2 == 0)    x86_v2 = 0;
        else if (x86op->v2 == SRC1) x86_v2 = src1;
        else if (x86op->v2 == SRC2) x86_v2 = src2;
        else if (x86op->v2 == DST)  x86_v2 = dst;
        else panic1d("Unknown operand to x86 instruction: %d", x86op->v2);

        if (x86_dst) x86_dst = dup_value(x86_dst);
        if (x86_v1)  x86_v1  = dup_value(x86_v1);
        if (x86_v2)  x86_v2  = dup_value(x86_v2);
        tac = add_x86_instruction(x86op, x86_dst, x86_v1, x86_v2);
        if (DEBUG_INSTSEL_TILING) print_instruction(stdout, tac);

        x86op = x86op->next;
    }

    return dst;
}

void make_intermediate_representation(IGraph *igraph) {
    if (DEBUG_INSTSEL_TILING) {
        printf("\nMaking IR\n");
        dump_igraph(igraph);
        printf("\n");
    }

    recursive_make_intermediate_representation(igraph, 0, -1, -1);
}

void tile_igraphs(Function *function) {
    int i, j;
    IGraph *igraphs;
    Function *f;
    Tac *tac, *current_instruction_ir_start;

    igraphs = eis_igraphs;

    if (DEBUG_INSTSEL_TILING) {
        printf("\nAll trees\n-----------------------------------------------------\n");

        for (i = 0; i < eis_instr_count; i++) {
            if (!igraphs[i].node_count) continue;
            tac = igraphs[i].nodes[0].tac;
            if (tac && tac->dst) {
                print_value(stdout, tac->dst, 0);
                printf(" =\n");
            }
            if (igraphs[i].node_count) dump_igraph(&(igraphs[i]));
        }
    }

    ir_start = 0;
    vreg_count = function->vreg_count;

    for (i = 0; i < eis_instr_count; i++) {
        if (!igraphs[i].node_count) continue;
        tac = igraphs[i].nodes[0].tac;

        // Whitelist operations there are no rules for
        if (tac &&
            (tac->operation == IR_NOP ||
             tac->operation == IR_START_CALL ||
             tac->operation == IR_END_CALL ||
             tac->operation == IR_START_LOOP ||
             tac->operation == IR_END_LOOP)) {

            add_tac_to_ir(tac);
            continue;
        }

        igraph_labels = malloc(igraphs[i].node_count * sizeof(Set *));
        for (j = 0; j < igraphs[i].node_count; j++) igraph_labels[j] = new_set(instr_rule_count);

        if (DEBUG_INSTSEL_TILING) {
            printf("\nTiling\n-----------------------------------------------------\n");

            if (tac)
                print_instruction(stdout, tac);
            else
                print_value(stdout, igraphs[i].nodes[0].value, 0);
        }

        cost_graph = new_graph(MAX_INSTRUCTION_GRAPH_CHOICE_NODE_COUNT, MAX_INSTRUCTION_GRAPH_CHOICE_EDGE_COUNT);

        cost_rules = malloc(MAX_INSTRUCTION_GRAPH_CHOICE_NODE_COUNT * sizeof(int));
        memset(cost_rules, 0, MAX_INSTRUCTION_GRAPH_CHOICE_NODE_COUNT * sizeof(int));

        accumulated_cost = malloc(MAX_INSTRUCTION_GRAPH_CHOICE_NODE_COUNT * sizeof(int));
        memset(accumulated_cost, 0, MAX_INSTRUCTION_GRAPH_CHOICE_NODE_COUNT * sizeof(int));

        cost_graph_node_count = 0;

        cost_to_igraph_map = malloc(MAX_INSTRUCTION_GRAPH_CHOICE_NODE_COUNT * sizeof(int));
        memset(cost_to_igraph_map, 0, MAX_INSTRUCTION_GRAPH_CHOICE_NODE_COUNT * sizeof(int));

        recursive_tile_igraphs(&(igraphs[i]), 0);
        if (DEBUG_INSTSEL_TILING) print_cost_graph(cost_graph, cost_rules, accumulated_cost);

        if (tac && tac->label) {
            add_instruction(IR_NOP, 0, 0, 0);
            ir_start->label = tac->label;
        }

        current_instruction_ir_start = ir;
        make_intermediate_representation(&(igraphs[i]));
        if (DEBUG_INSTSEL_TILING) {
            f = new_function();
            f->ir = current_instruction_ir_start;
            print_ir(f, 0);
        }
    }

    function->vreg_count = vreg_count;

    if (DEBUG_INSTSEL_TILING) {
        printf("\nFinal IR for block:\n");
        f = new_function();
        f->ir = ir_start;
        print_ir(f, 0);
    }
}

void select_instructions(Function *function) {
    int i, block_count;
    Block *blocks;
    Graph *cfg;
    Tac *tac;
    Tac *new_ir_start, *new_ir_pos;

    blocks = function->blocks;
    cfg = function->cfg;
    block_count = cfg->node_count;

    set_assign_to_reg_lvalue_dsts(function);

    // new_ir_start is the start of the new IR
    new_ir_start = new_instruction(IR_NOP);
    new_ir_pos = new_ir_start;

    // Loop over all blocks
    for (i = 0; i < block_count; i++) {
        blocks[i].end->next = 0; // Will be re-entangled later

        make_igraphs(function, i);
        simplify_igraphs(function, i);
        if (!disable_merge_constants) merge_constants(function);
        tile_igraphs(function);

        new_ir_pos->next = ir_start;
        ir_start->prev = new_ir_pos;

        while (new_ir_pos->next) new_ir_pos = new_ir_pos->next;
    }

    function->ir = new_ir_start;

    if (DEBUG_INSTSEL_TILING) {
        printf("\nFinal IR for function:\n");
        print_ir(function, 0);
    }

}

// This removes instructions that copy a register to itself by replacing them with noops.
void remove_vreg_self_moves(Function *function) {
    Tac *tac;

    tac = function->ir;
    while (tac) {
        if (tac->operation == X_MOV && tac->dst && tac->dst->vreg && tac->src1 && tac->src1->vreg && tac->dst->vreg == tac->src1->vreg) {
            tac->operation = IR_NOP;
            tac->dst = 0;
            tac->src1 = 0;
            tac->x86_template = 0;
        }

        tac = tac->next;
    }
}

Tac *make_spill_instruction(Value *v) {
    Tac *tac;
    int x86_operation;
    char *x86_template;

    make_value_x86_size(v);

    if (v->x86_size == 1) {
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

    tac = new_instruction(x86_operation);
    tac->x86_template = x86_template;

    return tac;
}

void add_spill_load(Tac *ir, int src, int preg) {
    Tac *tac;
    Value *v;

    v = src == 1 ? ir->src1 : ir->src2;

    tac = make_spill_instruction(v);
    tac->src1 = v;
    tac->dst = new_value();
    tac->dst->type = TYPE_LONG;
    tac->dst->x86_size = 4;
    tac->dst->vreg = -1000;   // Dummy value
    tac->dst->preg = preg;

    if (src == 1)
        ir->src1 = dup_value(tac->dst);
    else
        ir->src2 = dup_value(tac->dst);

    insert_instruction(ir, tac, 1);
}

void add_spill_store(Tac *ir, Value *v, int preg) {
    Tac *tac;

    tac = make_spill_instruction(v);
    tac->src1 = new_value();
    tac->src1->type = TYPE_LONG;
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

void add_spill_code(Function *function) {
    Tac *tac;
    int dst_eq_src1, store_preg;

    if (DEBUG_INSTSEL_SPILLING) printf("\nAdding spill code\n");

    tac = function->ir;
    while (tac) {
        if (DEBUG_INSTSEL_SPILLING) print_instruction(stdout, tac);

        dst_eq_src1 = (tac->dst && tac->src1 && tac->dst->vreg == tac->src1->vreg);

        if (tac->src1 && tac->src1->preg == -1 && tac->src1->spilled)  {
            if (DEBUG_INSTSEL_SPILLING) printf("Adding spill load\n");
            add_spill_load(tac, 1, REG_R10);
            if (dst_eq_src1) {
                // Special case where src1 is the same as dst, in that case, r10 contains the result.
                add_spill_store(tac, tac->dst, REG_R10);
                tac = tac->next;
            }
        }

        if (tac->src2 && tac->src2->preg == -1 && tac->src2->spilled) {
            if (DEBUG_INSTSEL_SPILLING) printf("Adding spill load\n");
            add_spill_load(tac, 2, REG_R11);
        }

        if (tac->dst && tac->dst->preg == -1 && tac->dst->spilled) {
            if (DEBUG_INSTSEL_SPILLING) printf("Adding spill store\n");
            add_spill_store(tac, tac->dst, REG_R11);
            tac = tac->next;
        }

        tac = tac->next;
    }
}

void eis1(Function *function) {
    do_oar1(function);
    do_oar2(function);
    do_oar3(function);
    ir_vreg_offset = 0;
    select_instructions(function);
    do_oar1b(function);
    coalesce_live_ranges(function);
    remove_vreg_self_moves(function);
}

void eis2(Function *function) {
    do_oar4(function);
    add_spill_code(function);
    ir_vreg_offset = 0;
}

void experimental_instruction_selection(Symbol *function_symbol) {
    Function *function;
    function = function_symbol->function;

    eis1(function);
    eis2(function);
}
