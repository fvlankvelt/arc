#include "transform.h"

#include <stdbool.h>
#include <string.h>

#include "binding.h"
#include "collection.h"

typedef struct _dcoord {
    signed short dx;
    signed short dy;
} dcoord_t;

dcoord_t deltas[] = {
    {0, -1},
    {0, 1},
    {-1, 0},
    {1, 0},
    {-1, -1},
    {-1, 1},
    {1, -1},
    {1, 1},
};

int rotations[3][4] = {
    // CLOCK_WISE = 0,
    {0, -1, 1, 0},
    // COUNTER_CLOCK_WISE,
    {0, 1, -1, 0},
    // DOUBLE_CLOCK_WISE
    {-1, 0, 0, -1},
};

typedef struct _subnode_iter {
    int n_subnodes;
    int i;
    const subnode_block_t* block;
} subnode_iter_t;

coordinate_t init_subnode_iter(
    subnode_iter_t* iter, const subnode_block_t* subnodes, int n_subnodes) {
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

bool check_collision(
    const graph_t* graph, node_t* node, subnode_block_t* subnodes, int n_subnodes) {
    int size = bitset_size(graph);
    long bitset[size];
    for (int i = 0; i < size; i++) {
        bitset[i] = 0;
    }
    subnode_iter_t iter;
    for (coordinate_t coord = init_subnode_iter(&iter, subnodes, n_subnodes);
         is_valid_subnode(&iter);
         coord = next_coordinate(&iter)) {
        if (!check_bounds(graph, coord)) {
            printf("NOOO!!!\n");
        }
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

color_t get_color(const graph_t* graph, color_t color) {
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
        subnode_t subnode = get_subnode(node, i);
        subnode.color = color;
        set_subnode(node, i, subnode);
    }
}

/*
 * move node by 1 pixel in a given direction
 */
void move_node(
    __attribute__((unused)) graph_t* graph, node_t* node, transform_arguments_t* args) {
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
    subnode_block_t* new_subnodes = new_subnode_block(graph);
    for (int i = 0; i < node->n_subnodes; i++) {
        subnode_t subnode = get_subnode(node, i);
        coordinate_t new_coord = subnode.coord;
        for (int r = 0; r < max_range; r++) {
            bool added = add_subnode_to_block(
                graph, new_subnodes, (subnode_t){new_coord, subnode.color}, &n_new_subnodes);
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

/**
 * move node in a given direction until it hits another node or the edge of the image
 */
void move_node_max(graph_t* graph, node_t* node, transform_arguments_t* args) {
    dcoord_t delta = deltas[args->direction];
    subnode_block_t block = {NULL};
    int n = 0;
    for (; n < 1000; n++) {
        bool hit = false;
        for (int i = 0; !hit && i < node->n_subnodes; i++) {
            subnode_t subnode = get_subnode(node, i);
            coordinate_t new_coord = subnode.coord;
            new_coord.pri += n * delta.dx;
            new_coord.sec += n * delta.dy;
            block.subnode[0] = new_coord;
            if (!check_bounds(graph, new_coord) || !check_collision(graph, node, &block, 1)) {
                hit = true;
                break;
            }
        }
        if (hit) {
            n--;
            break;
        }
    }
    if (n <= 0) {
        return;
    }
    for (int i = 0; i < node->n_subnodes; i++) {
        subnode_t subnode = get_subnode(node, i);
        subnode.coord.pri += n * delta.dx;
        subnode.coord.sec += n * delta.dy;
        set_subnode(node, i, subnode);
    }
}

/**
 * rotates node around its center point in a given rotational direction
 */
void rotate_node(graph_t* graph, node_t* node, transform_arguments_t* args) {
    int* r = rotations[args->rotation_dir];
    int sum_x = 0, sum_y = 0;
    for (int i = 0; i < node->n_subnodes; i++) {
        const subnode_t subnode = get_subnode(node, i);
        sum_x += subnode.coord.pri;
        sum_y += subnode.coord.sec;
    }
    coordinate_t center = {
        sum_x / node->n_subnodes,
        sum_y / node->n_subnodes,
    };
    for (int i = 0; i < node->n_subnodes; i++) {
        subnode_t subnode = get_subnode(node, i);
        coordinate_t d = {
            subnode.coord.pri - center.pri,
            subnode.coord.sec - center.sec,
        };
        subnode.coord = (coordinate_t){
            center.pri + r[0] * d.pri + r[1] * d.sec,
            center.sec + r[2] * d.pri + r[3] * d.sec,
        };
        if (check_bounds(graph, subnode.coord)) {
            set_subnode(node, i, subnode);
        } else {
            // ERROR?
        }
    }
}

transform_func_t transformations[] = {
    {
        .func = update_color,
        .color = true,
        .name = "update_color",
    },
    {
        .func = move_node,
        .direction = true,
        .name = "move_node",
    },
    {
        .func = extend_node,
        .direction = true,
        .overlap = true,
        .name = "extend_node",
    },
    {
        .func = move_node_max,
        .direction = true,
        .name = "move_node_max",
    },
    /*
    {
        .func = rotate_node,
        .rotation_dir = true,
        .name = "rotate_node",
    },
    */
    {
        .func = NULL,
    },
};

bool get_relative_pos(const node_t* node, const node_t* other, direction_t* direction) {
    for (int i = 0; i < node->n_subnodes; i++) {
        subnode_t sub_node = get_subnode(node, i);
        for (int j = 0; j < other->n_subnodes; j++) {
            subnode_t sub_other = get_subnode(other, j);
            if (sub_node.coord.pri == sub_other.coord.pri) {
                if (sub_node.coord.sec < sub_other.coord.sec) {
                    *direction = RIGHT;
                    return true;
                } else if (sub_node.coord.sec > sub_other.coord.sec) {
                    *direction = LEFT;
                    return true;
                }
            } else if (sub_node.coord.sec == sub_other.coord.sec) {
                if (sub_node.coord.pri < sub_other.coord.pri) {
                    *direction = UP;
                    return true;
                } else if (sub_node.coord.pri > sub_other.coord.pri) {
                    *direction = DOWN;
                    return true;
                }
            }
        }
    }
    return false;
}

coordinate_t get_centroid(const node_t* node) {
    int sum_pri = node->n_subnodes / 2;
    int sum_sec = node->n_subnodes / 2;
    for (int i = 0; i < node->n_subnodes; i++) {
        subnode_t subnode = get_subnode(node, i);
        sum_pri += subnode.coord.pri;
        sum_sec += subnode.coord.sec;
    }
    return (coordinate_t){
        .pri = sum_pri / node->n_subnodes,
        .sec = sum_sec / node->n_subnodes,
    };
}

mirror_axis_t get_mirror_axis(const node_t* node, const node_t* other) {
    coordinate_t centroid = get_centroid(other);
    for (const edge_t* edge = node->edges; edge; edge = edge->next) {
        if (edge->peer == other) {
            switch (edge->direction) {
                case EDGE_VERTICAL:
                    return (mirror_axis_t){
                        .orientation = ORIENTATION_VERTICAL,
                        .coord = centroid,
                    };
                default:
                    return (mirror_axis_t){
                        .orientation = ORIENTATION_HORIZONTAL,
                        .coord = centroid,
                    };
            }
        }
    }
    // TODO: is this really the intention?
    return (mirror_axis_t){
        .orientation = ORIENTATION_HORIZONTAL,
        .coord = centroid,
    };
}

void copy_arguments(const transform_arguments_t* from, transform_arguments_t* to) {
    memcpy(to, from, sizeof(transform_arguments_t));
}

bool apply_binding(
    const graph_t* graph,
    const node_t* node,
    const transform_dynamic_arguments_t* dynamic,
    transform_arguments_t* args) {
    if (dynamic->color) {
        binding_call_t* binding = dynamic->color;
        node_t* target = get_binding_node(graph, node, binding);
        if (target) {
            args->color = get_subnode(target, 0).color;
        } else {
            return false;
        }
    }
    if (dynamic->direction) {
        binding_call_t* binding = dynamic->direction;
        node_t* target = get_binding_node(graph, node, binding);
        if (!target || !get_relative_pos(node, target, &args->direction)) {
            return false;
        }
    }
    if (dynamic->mirror_axis) {
        binding_call_t* binding = dynamic->mirror_axis;
        node_t* target = get_binding_node(graph, node, binding);
        if (target) {
            args->mirror_axis = get_mirror_axis(node, target);
        } else {
            return false;
        }
    }
    // TODO: handle get_centroid
    /*
    if (dynamic->point) {
        binding_call_t* binding = dynamic->point;
        node_t* target = get_binding_node(graph, node, binding);
        args->point = get_centroid(target);
    }
    */
    return true;
}

#define MAX_ARGUMENT_VALUES 20

struct {
    int n_color;
    int n_direction;
    // int n_mirror_axis;
    // int n_point;

    int n_rotation;
    int n_overlap;
    // int n_mirror_direction;
    // int n_object_id;
    // int n_relative_pos;

    color_t color[13];
    direction_t direction[DOWN_RIGHT + 2];

    rotation_t rotation[3];
    bool overlap[3];
    // mirror_t mirror_direction[4];
    // short object_id[MAX_ARGUMENT_VALUES];
    // relative_position_t relative_pos[3];
} transform_argument_values;

void init_transform(guide_builder_t* builder) {
    int n_funcs = 0;
    while (transformations[n_funcs].func != NULL) {
        n_funcs++;
    }
    add_choice(builder, n_funcs, "transform:func");

    transform_argument_values.n_color = 3;
    color_t* colors = transform_argument_values.color;
    // 0: DYNAMIC
    colors[1] = MOST_COMMON_COLOR;
    colors[2] = LEAST_COMMON_COLOR;
    for (color_t color = 0; color < 10; color++) {
        colors[transform_argument_values.n_color++] = color;
    }
    add_choice(builder, transform_argument_values.n_color, "transform:color");
    add_binding(builder, "transform:bind_color");

    transform_argument_values.n_direction = DOWN_RIGHT + 2;
    direction_t* directions = transform_argument_values.direction;
    // 0: DYNAMIC
    for (direction_t dir = UP; dir <= DOWN_RIGHT; dir++) {
        directions[dir + 1] = dir;
    }
    add_choice(builder, transform_argument_values.n_direction, "transform:direction");
    add_binding(builder, "transform:bind_direction");

    transform_argument_values.n_rotation = 3;
    rotation_t* rotation = transform_argument_values.rotation;
    rotation[0] = CLOCK_WISE;
    rotation[1] = COUNTER_CLOCK_WISE;
    rotation[2] = DOUBLE_CLOCK_WISE;
    add_choice(builder, 3, "transform:rotation");

    transform_argument_values.n_overlap = 2;
    bool* overlap = transform_argument_values.overlap;
    overlap[0] = true;
    overlap[1] = false;
    add_choice(builder, 2, "transform:overlap");
}

transform_call_t* sample_transform(
    task_t* task, const graph_t* graph, filter_call_t* filter, trail_t** p_trail) {
    transform_call_t* call = new_item(task->_mem_transform_calls);
    trail_t* trail = *p_trail;

    const categorical_t* func_dist = next_choice(trail);
    int i_func = choose(func_dist);
    transform_func_t* func = &transformations[i_func];
    call->transform = func;
    trail = observe_choice(trail, i_func);

    const categorical_t* color_dist = next_choice(trail);
    if (call->transform->color) {
        int i_color = choose(color_dist);
        trail = observe_choice(trail, i_color);
        if (i_color == 0) {
            binding_call_t* binding = sample_binding(task, graph, filter, &trail);
            if (!binding) {
                goto fail;
            }
            call->dynamic.color = binding;
        } else {
            trail = observe_binding(trail, NULL);
            call->arguments.color = transform_argument_values.color[i_color];
        }
    } else {
        trail = observe_choice(trail, -1);
        trail = observe_binding(trail, NULL);
    }

    const categorical_t* direction_dist = next_choice(trail);
    if (call->transform->direction) {
        int i_direction = choose(direction_dist);
        trail = observe_choice(trail, i_direction);
        if (i_direction == 0) {
            binding_call_t* binding = sample_binding(task, graph, filter, &trail);
            if (!binding) {
                goto fail;
            }
            call->dynamic.direction = binding;
        } else {
            trail = observe_binding(trail, NULL);
            call->arguments.direction = transform_argument_values.direction[i_direction];
        }
    } else {
        trail = observe_choice(trail, -1);
        trail = observe_binding(trail, NULL);
    }

    const categorical_t* rotation_dist = next_choice(trail);
    if (call->transform->rotation_dir) {
        int i_rotation = choose(rotation_dist);
        trail = observe_choice(trail, i_rotation);
        call->arguments.rotation_dir = transform_argument_values.rotation[i_rotation];
    } else {
        trail = observe_choice(trail, -1);
    }

    const categorical_t* overlap_dist = next_choice(trail);
    if (call->transform->overlap) {
        int i_overlap = choose(overlap_dist);
        trail = observe_choice(trail, i_overlap);
        call->arguments.overlap = transform_argument_values.overlap[i_overlap];
    } else {
        trail = observe_choice(trail, -1);
    }

    *p_trail = trail;

    return call;

fail:
    while (trail != *p_trail) {
        trail = backtrack(trail);
    }
    free_item(task->_mem_transform_calls, call);
    return NULL;
}

trail_t* observe_transform(trail_t* trail, const transform_call_t* call) {
    /* const categorical_t* func_dist = */ next_choice(trail);
    transform_func_t* func;
    for (int i_func = 0; transformations[i_func].func; i_func++) {
        func = &transformations[i_func];
        if (call->transform == func) {
            trail = observe_choice(trail, i_func);
            break;
        }
    }

    /* const categorical_t* color_dist = */ next_choice(trail);
    if (call->dynamic.color) {
        trail = observe_choice(trail, 0);
        trail = observe_binding(trail, call->dynamic.color);
    } else if (call->transform->color) {
        for (int i_color = 1; i_color < transform_argument_values.n_color; i_color++) {
            if (call->arguments.color == transform_argument_values.color[i_color]) {
                trail = observe_choice(trail, i_color);
                break;
            }
        }
        trail = observe_binding(trail, NULL);
    } else {
        trail = observe_choice(trail, -1);
        trail = observe_binding(trail, NULL);
    }

    /* const categorical_t* direction_dist = */ next_choice(trail);
    if (call->dynamic.direction) {
        trail = observe_choice(trail, 0);
        trail = observe_binding(trail, call->dynamic.direction);
    } else if (call->transform->direction) {
        for (int i_direction = 1; i_direction < transform_argument_values.n_direction;
             i_direction++) {
            if (call->arguments.direction == transform_argument_values.direction[i_direction]) {
                trail = observe_choice(trail, i_direction);
                break;
            }
        }
        trail = observe_binding(trail, NULL);
    } else {
        trail = observe_choice(trail, -1);
        trail = observe_binding(trail, NULL);
    }

    /* const categorical_t* rotation_dist = */ next_choice(trail);
    if (call->transform->rotation_dir) {
        for (int i_rotation = 0; i_rotation < transform_argument_values.n_rotation;
             i_rotation++) {
            if (call->arguments.rotation_dir ==
                transform_argument_values.rotation[i_rotation]) {
                trail = observe_choice(trail, i_rotation);
                break;
            }
        }
    } else {
        trail = observe_choice(trail, -1);
    }

    /* const categorical_t* overlap_dist = */ next_choice(trail);
    if (call->transform->overlap) {
        for (int i_overlap = 0; i_overlap < transform_argument_values.n_overlap; i_overlap++) {
            if (call->arguments.overlap == transform_argument_values.overlap[i_overlap]) {
                trail = observe_choice(trail, i_overlap);
                break;
            }
        }
    } else {
        trail = observe_choice(trail, -1);
    }

    return trail;
}