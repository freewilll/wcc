#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "wc4.h"

enum {
    MAX_INSTRUCTION_GRAPH_EDGE_COUNT = 32,
    MAX_INSTRUCTION_GRAPH_CHOICE_NODE_COUNT = 128,
    MAX_INSTRUCTION_GRAPH_CHOICE_EDGE_COUNT = 128,
    MAX_CHOICE_TRAIL_COUNT = 32,
};

Graph *cost_graph;              // Graph of all possible options when tiling
int cost_graph_node_count;      // Number of nodes in cost graph
int *cost_to_igraph_map;        // Mapping of a cost graph node back to the instruction graph node
int *cost_rules;                // Mapping of cost graph node id to x86 instruction rule id
int *accumulated_cost;          // Total cost of sub tree of a cost graph node
Set **igraph_labels;            // Matched instruction rule ids for a igraph node id

int recursive_tile_igraphs(IGraph *igraph, int node_id);

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
        if (operation == IR_ASSIGN) printf("=\n");
        else if (operation == IR_ADD) printf("+\n");
        else if (operation == IR_SUB) printf("-\n");
        else if (operation == IR_MUL) printf("*\n");
        else if (operation == IR_DIV) printf("/\n");
        else if (operation == IR_INDIRECT) printf("indirect\n");
        else if (operation == IR_LOAD_CONSTANT) printf("load constant\n");
        else if (operation == IR_NOP) printf("noop\n");
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

void copy_inode(IGraphNode *src, IGraphNode *dst) {
    dst->tac = src->tac;
    dst->value = src->value;
}

IGraph *merge_assignment_igraphs(IGraph *g1, IGraph *g2, int vreg) {
    int i, replacement_vreg, replacement_value, replacement_is_constant;
    IGraphNode *in;
    Value *v;

    replacement_vreg = g2->nodes[1].value->vreg;
    replacement_value = g2->nodes[1].value->value;
    replacement_is_constant = g2->nodes[1].value->is_constant;
    for (i = 0; i < g1->node_count; i++) {
        in = &(g1->nodes[i]);
        if (in->value && in->value->vreg == vreg) {
            v = dup_value(in->value); // For no side effects
            in->value = v;
            in->value->vreg = replacement_vreg;
            in->value->value = replacement_value;
            in->value->is_constant = replacement_is_constant;

            if (DEBUG_INSTSEL_IGRAPHS_DEEP) {
                printf("\nreusing tweaked g1 for g:\n");
                dump_igraph(g1);
            }

            return g1;
        }
    }

    panic("Should never get here");
}

// Merge g2 into g1. The merge point is vreg
IGraph *merge_igraphs(IGraph *g1, IGraph *g2, int vreg) {
    int i, j, operation, node_count, d, join_from, join_to, from, to;
    IGraph *g;
    IGraphNode *inodes, *g1_inodes, *in, *inodes2, *in2;
    GraphNode *n, *n2;
    GraphEdge *e, *e2;
    Value *v;
    Tac *tac;

    if (DEBUG_INSTSEL_IGRAPHS_DEEP) {
        printf("g1:\n");
        dump_igraph(g1);
        printf("\ng2:\n");
        dump_igraph(g2);
        printf("\n");
    }

    if (g1->node_count == 2) {
        tac = g1->nodes[0].tac;
        operation = tac->operation;
        if (operation == IR_LOAD_VARIABLE ||
            (operation == IR_ASSIGN && tac->dst->vreg && tac->src1->vreg)) {

            // For no side effects
            g2->nodes[0].tac->dst = dup_value(g1->nodes[0].tac->dst);

            if (DEBUG_INSTSEL_IGRAPHS_DEEP) {
                printf("\nreusing g2 for g:\n");
                dump_igraph(g2);
            }

            return g2;
        }
        else if (operation == IR_LOAD_CONSTANT)
            return merge_assignment_igraphs(g2, g1, vreg);
    }

    if (g2->node_count == 2) {
        operation = g2->nodes[0].tac->operation;
        if (operation == IR_ASSIGN || operation == IR_LOAD_VARIABLE || operation == IR_LOAD_CONSTANT)
            return merge_assignment_igraphs(g1, g2, vreg);
    }

    g = malloc(sizeof(IGraph));
    memset(g, 0, sizeof(IGraph));

    node_count = g1->node_count + g2->node_count - 1;
    inodes = malloc(node_count * sizeof(IGraphNode));
    memset(inodes, 0, node_count * sizeof(IGraphNode));

    g->nodes = inodes;
    g->graph = new_graph(node_count, MAX_INSTRUCTION_GRAPH_EDGE_COUNT);
    g->node_count = node_count;
    join_from = -1;
    join_to = -1;

    if (g1->node_count == 0) panic("Unexpectedly got 0 g1->node_count");

    if (DEBUG_INSTSEL_IGRAPHS_DEEP) printf("g1->node_count=%d\n", g1->node_count);
    if (DEBUG_INSTSEL_IGRAPHS_DEEP) printf("g2->node_count=%d\n", g2->node_count);

    for (i = 0; i < g1->node_count; i++) {
        if (DEBUG_INSTSEL_IGRAPHS_DEEP) printf("Copying g1 %d to %d\n", i, i);
        copy_inode(&(g1->nodes[i]), &(inodes[i]));

        g1_inodes = g1->nodes;
        n = &(g1->graph->nodes[i]);

        e = n->succ;

        while (e) {
            in = &(g1_inodes[e->to->id]);
            if (in->value && in->value->vreg == vreg) {
                join_from = e->from->id;
                join_to = e->to->id;
                if (DEBUG_INSTSEL_IGRAPHS_DEEP) printf("Adding join edge %d -> %d\n", join_from, join_to);
                add_graph_edge(g->graph, join_from, join_to);
            }
            else {
                if (DEBUG_INSTSEL_IGRAPHS_DEEP) printf("Adding g1 edge %d -> %d\n", e->from->id, e->to->id);
                add_graph_edge(g->graph, e->from->id, e->to->id);
            }

            e = e->next_succ;
        }
    }

    if (join_from == -1 || join_to == -1) panic("Attempt to join two trees without a join node");

    if (DEBUG_INSTSEL_IGRAPHS_DEEP) printf("\n");
    for (i = 0; i < g2->node_count; i++) {
        d = (i == 0) ? join_to : g1->node_count + i - 1;
        copy_inode(&(g2->nodes[i]), &(inodes[d]));

        n2 = &(g2->graph->nodes[i]);
        e = n2->succ;

        while (e) {
            from = e->from->id;
            to = e->to->id;

            if (from == 0)
                from = join_to;
            else
                from += g1->node_count - 1;

            to += g1->node_count - 1;

            if (DEBUG_INSTSEL_IGRAPHS_DEEP) printf("Adding g2 edge %d -> %d\n", from, to);
            add_graph_edge(g->graph, from, to);
            e = e->next_succ;
        }
    }

    if (DEBUG_INSTSEL_IGRAPHS_DEEP) {
        printf("\ng:\n");
        dump_igraph(g);
    }

    return g;
}

void make_igraphs(Function *function) {
    int instr_count, i, j, node_count, vreg_count;
    int dst, src1, src2, g1_igraph_id;
    Tac *tac;
    IGraph *igraphs, *ig;
    IGraphNode *nodes;
    Graph *graph;
    VregIGraph* vreg_igraphs;
    Value *v;

    do_oar1(function);
    do_oar2(function);
    do_oar3(function);

    if (DEBUG_INSTSEL_IGRAPHS_DEEP) print_intermediate_representation(function, 0);

    vreg_count = function->vreg_count;

    // TODO loop over blocks
    instr_count = 0;
    tac = function->ir;
    while (tac) {
        instr_count++;
        tac = tac->next;
    }

    igraphs = malloc(instr_count * sizeof(IGraph));

    i = 0;
    tac = function->ir;
    while (tac) {
        node_count = 1;
        if (tac->src1) node_count++;
        if (tac->src2) node_count++;

        nodes = malloc(node_count * sizeof(IGraphNode));
        memset(nodes, 0, node_count * sizeof(IGraphNode));

        graph = new_graph(node_count, MAX_INSTRUCTION_GRAPH_EDGE_COUNT);

        nodes[0].tac = tac;
        node_count = 1;
        if (tac->src1) {nodes[node_count].value = tac->src1; add_graph_edge(graph, 0, node_count); node_count++;}
        if (tac->src2) {nodes[node_count].value = tac->src2; add_graph_edge(graph, 0, node_count); node_count++;}

        igraphs[i].graph = graph;
        igraphs[i].nodes = nodes;
        igraphs[i].node_count = node_count;

        tac = tac->next;
        i++;
    }

    vreg_igraphs = malloc((vreg_count + 1) * sizeof(VregIGraph));
    memset(vreg_igraphs, 0, (vreg_count + 1) * sizeof(VregIGraph));

    tac = function->ir;
    i = 0;
    while (tac->next) { tac = tac->next; i++; }

    while (tac) {
        // TODO one block at a time
        // TODO init vreg_igraphs with liveout

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

        if (dst && (src1 && dst == src1) || (src2 && dst == src2)) {
            print_instruction(stdout, tac);
            panic("Illegal assignment of src1/src2 to dst");
        }

        if (src1 && src2 && src1 == src2) {
            print_instruction(stdout, tac);
            panic("src1 == src2 not handled");
        }

        if (vreg_igraphs[dst].count == 1) {
            // TODO free memory

            g1_igraph_id = vreg_igraphs[dst].igraph_id;
            if (DEBUG_INSTSEL_IGRAPHS_DEEP) printf("\nMerging dst=%d src1=%d src2=%d ", dst, src1, src2);
            if (DEBUG_INSTSEL_IGRAPHS_DEEP) printf("in locs %d and %d on vreg=%d\n----------------------------------------------------------\n", g1_igraph_id, i, dst);
            ig = merge_igraphs(&(igraphs[g1_igraph_id]), &(igraphs[i]), dst);

            igraphs[g1_igraph_id].nodes = ig->nodes;
            igraphs[g1_igraph_id].graph = ig->graph;
            igraphs[g1_igraph_id].node_count = ig->node_count;


            igraphs[i].node_count = 0; // Nuke it, should not be needed, but it triggers a panic in case of a bug

            if (src1) vreg_igraphs[src1].igraph_id = g1_igraph_id;
            if (src2) vreg_igraphs[src2].igraph_id = g1_igraph_id;
        }

        if (dst) vreg_igraphs[dst].count = 0;

        i--;
        tac = tac->prev;
    }

    if (DEBUG_INSTSEL_IGRAPHS_DEEP)
        printf("\n=================================\n");

    if (DEBUG_INSTSEL_IGRAPHS) {
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

    function->eis_igraphs = igraphs;
    function->eis_instr_count = instr_count;
}

int rules_match(int parent_src, Rule *child) {
    return parent_src == child->non_terminal;
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
            match = rules_match(src, &(instr_rules[cost_rules[choice_node_id]]));
            if (match) {
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

int tile_igraph_leaf_node(IGraph *igraph, int node_id) {
    int i, vc, vr, cost_graph_node_id, choice_node_id, matched, matched_dst;
    Value *v;
    Rule *r;

    if (DEBUG_INSTSEL_TILING) dump_igraph(igraph);

    cost_graph_node_id = new_cost_graph_node();
    cost_to_igraph_map[cost_graph_node_id] = node_id;

    v = igraph->nodes[node_id].value;
    vc = v->is_constant;
    vr = v->vreg != 0;

    if (DEBUG_INSTSEL_TILING) printf("leaf vc=%d vr=%d\n", vc, vr);

    // Find a matching instruction
    matched = 0;
    for (i = 0; i < instr_rule_count; i++) {
        r = &(instr_rules[i]);
        if (r->operation) continue;

        if (node_id > 0)
            matched_dst = 1;
        else
            matched_dst = rules_match(REG, r);

        if (matched_dst && ((r->src1 == CST && vc) || (r->src1 == REG && vr))) {
            if (DEBUG_INSTSEL_TILING) printf("  matched rule %d\n", i);
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
    int i, j, operation, src1_id, src2_id;
    int matched, matched_src;
    int choice_node_id, src1_cost_graph_node_id, src2_cost_graph_node_id;
    int cost_graph_node_id, min_cost, min_cost_src1, min_cost_src2, src, cost;
    Tac *tac;
    IGraphNode *inode, *inodes;
    GraphEdge *e;
    Value *src1, *src2, *v;
    Rule *r, *child_rule;
    Set *l;

    if (DEBUG_INSTSEL_TILING) printf("\tile_igraph_operation_node on node=%d\n", node_id);

    inodes = igraph->nodes;
    inode = &(igraph->nodes[node_id]);
    tac = inode->tac;
    operation = tac->operation;

    if (DEBUG_INSTSEL_TILING) {
        print_instruction(stdout, tac);
        dump_igraph(igraph);
    }

    cost_graph_node_id = new_cost_graph_node();
    cost_to_igraph_map[cost_graph_node_id] = node_id;

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

    src1_cost_graph_node_id = recursive_tile_igraphs(igraph, src1_id);
    if (src2_id) src2_cost_graph_node_id = recursive_tile_igraphs(igraph, src2_id);

    if (DEBUG_INSTSEL_TILING) {
        printf("back from recursing at node=%d\n\n", node_id);
        printf("src1 labels: "); print_set(igraph_labels[src1_id]); printf(" ");

        if (src2_id) {
            printf("src2 labels: ");
            print_set(igraph_labels[src2_id]);
            printf("\n");
        }
    }

    // Loop over all rules until a match is found
    matched = 0;
    for (i = 0; i < instr_rule_count; i++) {
        r = &(instr_rules[i]);

        if (r->operation != operation) continue;
        if (src2_id && !r->src2) continue;
        if (!src2_id && r->src2) continue;

        // The top level parent node can only match a rule that
        // with a REG as a dst.
        if (node_id == 0 && !rules_match(REG, r)) continue;

        // Check dst of the subtree tile matches what is needed
        matched_src = 0;
        l = igraph_labels[src1_id];
        for (j = 0; j <= l->max_value; j++) {
            if (!l->elements[j]) continue;
            if (instr_rules[j].non_terminal == r->src1) { matched_src = 1; break; }
        }
        if (!matched_src) continue;

        matched_src = 0;
        if (src2_id) {
            l = igraph_labels[src2_id];
            for (j = 0; j <= l->max_value; j++) {
                if (!l->elements[j]) continue;
                if (instr_rules[j].non_terminal == r->src2) { matched_src = 1; break; }
            }
            if (!matched_src) continue;
        }

        // We have a match, do some accounting ...
        matched = 1;

        if (DEBUG_INSTSEL_TILING) printf("matched rule %d\n", i);
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

                if (rules_match(src, child_rule)) {
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
        print_instruction(stdout, tac);
        dump_igraph(igraph);
        panic("Did not match any rules");
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
Value *recursive_make_intermediate_representation(IGraph *igraph, int cost_graph_node_id, int parent_node_id, int parent_src) {
    int i, pv, choice_node_id, match;
    int least_expensive_choice_node_id, min_cost, cost, igraph_node_id, dst_type;
    GraphEdge *choice_edge, *e;
    Rule *parent_rule, *rule;
    IGraphNode *ign;
    Value *src, *dst, *src1, *src2, *x86_dst, *x86_v1, *x86_v2;
    X86Operation *x86op;

    min_cost = 100000000;
    least_expensive_choice_node_id = -1;
    choice_edge = cost_graph->nodes[cost_graph_node_id].succ;
    while (choice_edge) {
        choice_node_id = choice_edge->to->id;

        if (parent_node_id != -1) {
            // Ensure src and dst match for non-root nodes
            parent_rule = &(instr_rules[cost_rules[parent_node_id]]);
            pv = (parent_src == 1) ? parent_rule->src1 : parent_rule->src2;
            match = rules_match(pv, &(instr_rules[cost_rules[choice_node_id]]));
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

    igraph_node_id = cost_to_igraph_map[least_expensive_choice_node_id];
    ign = &(igraph->nodes[igraph_node_id]);

    // Go down children, which return the value the result is in
    e = cost_graph->nodes[least_expensive_choice_node_id].succ;
    i = 1;
    src1 = ign->value;
    src2 = 0;

    while (e) {
        src = recursive_make_intermediate_representation(igraph, e->to->id, choice_node_id, i);
        if (i == 1)
            src1 = src;
        else
            src2 = src;

        e = e->next_succ;
        i++;
    }

    rule = &(instr_rules[cost_rules[least_expensive_choice_node_id]]);

    if (DEBUG_INSTSEL_TILING) printf("processing rule %d\n", cost_rules[least_expensive_choice_node_id]);

    dst = 0;
    if (parent_node_id == -1) {
        // Use the root node tac or value as a result
        if (ign->tac)
            dst = ign->tac->dst;
        else
            dst = ign->value;
    }
    else {
        // Determine the result from the x86 definition
        if (rule->x86_operations) {
            // It's an operation on a non-root node. Allocate a vreg.
            dst = new_value();
            dst->vreg = new_vreg();
            dst->ssa_subscript = 0;

            // Infer the dst type from the operands by picking the type with the largest size.
            dst_type = 0;
            if (rule->src1) {
                if (!rule->src2)
                    dst_type = src1->type;
                else if (src1->type > src2->type)
                    dst_type = src1->type;
                else
                    dst_type = src2->type;
            }
            if (!dst_type) panic("Unable to determine x86 dst type");

            dst->type = dst_type;
        }
        else
            dst = src1; // Passthrough for cst or reg
    }

    // Add x86 operations to the IR
    x86op = rule->x86_operations;
    while (x86op) {
             if (x86op->v1 == SRC1 || x86op->v1 == CST1) x86_v1 = src1;
        else if (x86op->v1 == SRC2 || x86op->v1 == CST2) x86_v1 = src2;
        else if (x86op->v1 == DST)                       x86_v1 = dst;
        else panic1d("Unknown operand to x86 instruction: %d", x86op->v1);

             if (x86op->v2 == SRC1 || x86op->v2 == CST1) x86_v2 = src1;
        else if (x86op->v2 == SRC2 || x86op->v2 == CST2) x86_v2 = src2;
        else if (x86op->v2 == DST)                       x86_v2 = dst;
        else panic1d("Unknown operand to x86 instruction: %d", x86op->v2);

        if (DEBUG_INSTSEL_TILING) printf("  adding instruction for operation %d: %s\n", x86op->operation, x86op->template);
        add_instruction(x86op->operation, dst, x86_v1, x86_v2);
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
    int i, j, instr_count;
    IGraph *igraphs;
    Function *f;
    Tac *tac;

    igraphs = function->eis_igraphs;
    instr_count = function->eis_instr_count;

    for (i = 0; i < instr_count; i++) {
        if (!igraphs[i].node_count) continue;
        tac = igraphs[i].nodes[0].tac;
        if (tac && tac->operation == IR_NOP) continue;

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

        recursive_tile_igraphs(&(igraphs[i]), 0);
        if (DEBUG_INSTSEL_TILING) print_cost_graph(cost_graph, cost_rules, accumulated_cost);

        ir_start = 0;
        add_instruction(IR_NOP, 0, 0, 0);
        if (tac) ir_start->label = tac->label;

        vreg_count = function->vreg_count;
        make_intermediate_representation(&(igraphs[i]));
        if (DEBUG_INSTSEL_TILING) {
            f = new_function();
            f->ir = ir_start;
            print_intermediate_representation(f, 0);
        }
    }
}

void experimental_instruction_selection(Function *function) {
    make_igraphs(function);
    tile_igraphs(function);
}
