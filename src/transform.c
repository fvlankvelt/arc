#include <stdbool.h>

#include "binding.h"
#include "transform.h"
#include "collection.h"

typedef struct _dcoord {
    signed short dx;
    signed short dy;
} dcoord_t;

dcoord_t deltas[] = {
    {0, -1}, {0, 1}, {-1, 0}, {1, 0}, {-1, -1}, {-1, 1}, {1, -1}, {1, 1},
};

typedef struct _subnode_iter {
    int n_subnodes;
    int i;
    const subnode_block_t* block;
} subnode_iter_t;

coordinate_t init_subnode_iter(subnode_iter_t* iter, const subnode_block_t* subnodes, int n_subnodes) {
    iter->n_subnodes = n_subnodes;
    iter->block = subnodes;
    iter->i = 0;
    return iter->block->subnode[0];
}

static inline bool is_valid_subnode(const subnode_iter_t* iter) {
    return iter->i < iter->n_subnodes;
}

coordinate_t next_coordinate(subnode_iter_t* iter) {
    coordinate_t coord = iter->block->subnode[iter->i % SUBNODE_BLOCK_SIZE];
    if (iter->i == SUBNODE_BLOCK_SIZE - 1) {
        iter->block = iter->block->next;
    }
    iter->i++;
    return coord;
}

static inline bool check_bounds(const graph_t* graph, coordinate_t coord) {
    return coord.pri >= 0 && coord.sec >= 0 && coord.pri < graph->width &&
           coord.sec < graph->height;
}

bool check_collision(const graph_t* graph, node_t* node, subnode_block_t* subnodes,
                            int n_subnodes) {
    int size = bitset_size(graph);
    long bitset[size];
    for (int i = 0; i < size; i++) {
        bitset[i] = 0;
    }
    subnode_iter_t iter;
    for (coordinate_t coord = init_subnode_iter(&iter, subnodes, n_subnodes);
         is_valid_subnode(&iter);
         coord = next_coordinate(&iter)) {
        add_coordinate(graph, bitset, coord);
    }
    for (const node_t* other = graph->nodes; other; other = other->next) {
        if (other == node) {
            continue;
        }
        for (int i = 0; i < other->n_subnodes; i++) {
            subnode_t other_subnode = get_subnode(other, i);
            if (coordinate_in_set(graph, bitset, other_subnode.coord)) {
                return false;
            }
        }
    }
    return true;
}

color_t get_color(const graph_t * graph, color_t color) {
    derived_props_t props = get_derived_properties(graph);
    switch (color) {
        case LEAST_COMMON_COLOR:
            return props.least_common_color;
        case MOST_COMMON_COLOR:
            return props.most_common_color;
        default:
    }
    return color;
}

/*
 * update node color to given color
 */
void update_color(graph_t* graph, node_t* node, transform_arguments_t* args) {
    color_t color = get_color(graph, args->color);
    for (int i = 0; i < node->n_subnodes; i++) {
        subnode_t subnode = get_subnode(node, 0);
        subnode.color = color;
        set_subnode(node, i, subnode);
    }
}

/*
 * move node by 1 pixel in a given direction
 */
void move_node(graph_t* graph, node_t* node, transform_arguments_t* args) {
    dcoord_t delta = deltas[args->direction];
    for (int i = 0; i < node->n_subnodes; i++) {
        subnode_t subnode = get_subnode(node, i);
        subnode.coord.pri += delta.dx;
        subnode.coord.sec += delta.dy;
        set_subnode(node, i, subnode);
    }
}

/**
 * extend node in a given direction,
 * if overlap is true, extend node even if it overlaps with another node
 * if overlap is false, stop extending before it overlaps with another node
 */
void extend_node(graph_t* graph, node_t* node, transform_arguments_t* args) {
    dcoord_t delta = deltas[args->direction];
    int max_range = graph->width > graph->height ? graph->width : graph->height;
    subnode_block_t block = {NULL};
    int n_new_subnodes = 0;
    subnode_block_t * new_subnodes = new_subnode_block(graph);
    for (int i = 0; i < node->n_subnodes; i++) {
        subnode_t subnode = get_subnode(node, i);
        coordinate_t new_coord = subnode.coord;
        for (int r = 0; r < max_range; r++) {
            bool added = add_subnode_to_block(graph, new_subnodes, (subnode_t) {
                new_coord, subnode.color
            }, &n_new_subnodes);
            assert(added);
            new_coord.pri += delta.dx;
            new_coord.sec += delta.dy;
            block.subnode[0] = new_coord;
            if (!check_bounds(graph, new_coord)) {
                // stop at edge of image
                break;
            } else if (!args->overlap && !check_collision(graph, node, &block, 1)) {
                // stop when colliding
                break;
            }
        }
    }
    set_subnodes(graph, node, new_subnodes, n_new_subnodes);
}
 
transform_func_t transformations[] = {
    {
        .func = update_color,
        .flags = {
            .color = true,
        }
    },
    {
        .func = move_node,
        .flags = {
            .direction = true,
        }
    },
    {
        .func = extend_node,
        .flags = {
            .direction = true,
            .overlap = true,
        }
    },
};

direction_t get_relative_pos(const node_t * node, const node_t * other) {
    for (int i = 0; i < node->n_subnodes; i++) {
        subnode_t sub_node = get_subnode(node, i);
        for (int j = 0; j < other->n_subnodes; j++) {
            subnode_t sub_other = get_subnode(other, i);
            if (sub_node.coord.pri == sub_other.coord.pri) {
                if (sub_node.coord.sec < sub_other.coord.sec) {
                    return RIGHT;
                } else if (sub_node.coord.sec > sub_other.coord.sec) {
                    return LEFT;
                }
            } else if (sub_node.coord.sec == sub_other.coord.sec) {
                if (sub_node.coord.pri < sub_other.coord.pri) {
                    return UP;
                } else if (sub_node.coord.pri > sub_other.coord.pri) {
                    return DOWN;
                }
            }
        }
    }
    return NO_DIRECTION;
}

void apply_binding(const graph_t * graph, const node_t * node, const transform_binding_t * arg_binding, transform_arguments_t *args) {
    if (arg_binding->constant_flags.color) {
        args->color = get_color(graph, arg_binding->constant_values.color);
    } else {
        binding_func_t * binding = arg_binding->color_call.binding;
        const binding_arguments_t * binding_args = &arg_binding->color_call.args;
        node_t * target = binding->func(graph, binding_args);
        args->color = get_subnode(target, 0).color;
    }
    if (arg_binding->constant_flags.direction) {
        args->direction = arg_binding->constant_values.direction;
    } else {
        binding_func_t * binding = arg_binding->direction_call.binding;
        const binding_arguments_t * binding_args = &arg_binding->direction_call.args;
        node_t * target = binding->func(graph, binding_args);
        args->direction = get_relative_pos(node, target);
    }
    args->overlap = arg_binding->constant_values.overlap;
}

/*
typedef struct _multiset_entry {
    struct _multiset_entry * next;
    node_t * node;
} multiset_entry_t;

typedef struct _multiset {
    multiset_entry_t * index[1024];
    multiset_entry_t entries[1024];
} multiset_t;

void update_edges(graph_t * graph) {
    multiset_t multiset;
    for (int i = 0; i < 1024; i++) {
        multiset.index[i] = NULL;
    }
    int subnode_count = 0;
    for (node_t * node = graph->nodes; node; node = node->next) {
        for (int i = 0; i < node->n_subnodes; i++) {
            coordinate_t coord = get_subnode(node, i).coord;
            int idx = 32 * coord.pri + coord.sec;
            multiset_entry_t * current = multiset.index[idx];
            multiset_entry_t * entry = &multiset.entries[subnode_count++];
            entry->next = current;
            multiset.index[idx] = entry;
        }
    }
    for (int i = 0; i < 1024; i++) {
        for (multiset_entry_t * entry = multiset.index[i]; entry; entry = entry->next) {
            for (multiset_entry_t * other = entry->next; other; other = other->next) {
                if (!has_edge(entry->node, other->node)) {
                    add_edge(graph, entry->node, other->node, OVERLAPPING);
                }
            }
        }
    }
}
*/