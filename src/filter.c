#include "filter.h"

#include "guide.h"
#include "util.h"

#define MAX_ARGUMENT_VALUES 20

bool filter_by_color(const graph_t* graph, const node_t* node, const filter_arguments_t* args) {
    color_t color = args->color;
    if (color < 0) {
        derived_props_t sym_colors = get_derived_properties(graph);
        if (color == BACKGROUND_COLOR) {
            color = graph->background_color;
        } else if (color == MOST_COMMON_COLOR) {
            color = sym_colors.most_common_color;
        } else if (color == LEAST_COMMON_COLOR) {
            color = sym_colors.least_common_color;
        } else {
            assert(false);
            return false;
        }
    }
    if (graph->is_multicolor) {
        if (!args->exclude) {
            for (int i = 0; i < node->n_subnodes; i++) {
                if (get_subnode(node, i).color == color) {
                    return true;
                }
            }
            return false;
        } else {
            for (int i = 0; i < node->n_subnodes; i++) {
                if (get_subnode(node, i).color == color) {
                    return false;
                }
            }
            return true;
        }
    } else {
        if (!args->exclude) {
            return get_subnode(node, 0).color == color;
        } else {
            return get_subnode(node, 0).color != color;
        }
    }
    return false;
}

bool filter_by_size(const graph_t* graph, const node_t* node, const filter_arguments_t* args) {
    int size = args->size;
    if (size < 0) {
        if (size == ODD_SIZE) {
            if (args->exclude) {
                return node->n_subnodes % 2 == 0;
            } else {
                return node->n_subnodes % 2 != 0;
            }
        } else {
            derived_props_t props = get_derived_properties(graph);
            if (size == MAX_SIZE) {
                return node->n_subnodes == props.max_size;
            } else if (size == MIN_SIZE) {
                return node->n_subnodes == props.min_size;
            } else {
                assert(false);
                return false;
            }
        }
    }
    if (args->exclude) {
        return node->n_subnodes != size;
    } else {
        return node->n_subnodes == size;
    }
}

bool filter_by_degree(
    __attribute__((unused)) const graph_t* graph,
    const node_t* node,
    const filter_arguments_t* args) {
    if (args->exclude) {
        return node->n_edges != args->degree;
    } else {
        return node->n_edges == args->degree;
    }
}

bool filter_by_neighbor_size(
    const graph_t* graph, const node_t* node, const filter_arguments_t* args) {
    for (const edge_t* edge = node->edges; edge; edge = edge->next) {
        const node_t* peer = edge->peer;
        int size = args->size;
        if (size < 0) {
            if (size == ODD_SIZE) {
                if (args->exclude) {
                    if (peer->n_subnodes % 2 == 0) {
                        return true;
                    }
                } else {
                    if (peer->n_subnodes % 2 != 0) {
                        return true;
                    }
                }
            } else {
                derived_props_t props = get_derived_properties(graph);
                if (size == MAX_SIZE) {
                    if (peer->n_subnodes == props.max_size) {
                        return true;
                    }
                } else if (size == MIN_SIZE) {
                    if (peer->n_subnodes == props.min_size) {
                        return true;
                    }
                } else {
                    assert(false);
                    return false;
                }
            }
        } else {
            if (args->exclude) {
                if (node->n_subnodes != size) {
                    return true;
                }
            } else {
                if (node->n_subnodes == size) {
                    return true;
                }
            }
        }
    }
    return false;
}

bool filter_by_neighbor_color(
    const graph_t* graph, const node_t* node, const filter_arguments_t* args) {
    color_t color = args->color;
    if (color < 0) {
        derived_props_t sym_colors = get_derived_properties(graph);
        if (color == BACKGROUND_COLOR) {
            color = graph->background_color;
        } else if (color == MOST_COMMON_COLOR) {
            color = sym_colors.most_common_color;
        } else if (color == LEAST_COMMON_COLOR) {
            color = sym_colors.least_common_color;
        } else {
            assert(false);
            return false;
        }
    }
    for (const edge_t* edge = node->edges; edge; edge = edge->next) {
        const node_t* peer = edge->peer;
        subnode_t subnode = get_subnode(peer, 0);
        if (args->exclude) {
            if (subnode.color != color) {
                return true;
            }
        } else {
            if (subnode.color == color) {
                return true;
            }
        }
    }
    return false;
}

bool filter_by_neighbor_degree(
    __attribute__((unused)) const graph_t* graph,
    const node_t* node,
    const filter_arguments_t* args) {
    for (const edge_t* edge = node->edges; edge; edge = edge->next) {
        const node_t* peer = edge->peer;
        if (args->exclude) {
            if (peer->n_edges != args->degree) {
                return true;
            }
        } else {
            if (peer->n_edges == args->degree) {
                return true;
            }
        }
    }
    return false;
}

filter_func_t filter_funcs[] = {
    {
        .func = filter_by_color,
        .color = true,
        .exclude = true,
        .name = "filter_by_color",
    },
    {
        .func = filter_by_size,
        .size = true,
        .exclude = true,
        .name = "filter_by_size",
    },
    {
        .func = filter_by_degree,
        .degree = true,
        .exclude = true,
        .name = "filter_by_degree",
    },
    {
        .func = filter_by_neighbor_color,
        .color = true,
        .exclude = true,
        .name = "filter_by_neighbor_color",
    },
    {
        .func = filter_by_neighbor_size,
        .size = true,
        .exclude = true,
        .name = "filter_by_neighbor_size",
    },
    {
        .func = filter_by_neighbor_degree,
        .degree = true,
        .exclude = true,
        .name = "filter_by_neighbor_degree",
    },
    {
        .func = NULL,
    }};

bool apply_filter(const graph_t* graph, const node_t* node, const filter_call_t* call) {
    for (filter_call_t* current = (filter_call_t*)call; current;
         current = current->next_in_multi) {
        if (!current->filter->func(graph, node, &current->args)) {
            return false;
        }
    }
    return true;
}

bool filter_matches(const graph_t* graph, const filter_call_t* call) {
    bool found_node = false;
    for (const node_t* node = graph->nodes; node; node = node->next) {
        if (apply_filter(graph, node, call)) {
            found_node = true;
            break;
        }
    }
    if (!found_node) {
        return false;
    }
    return true;
}

struct {
    int n_size;
    int n_degree;
    int n_exclude;
    int n_color;
    int size[MAX_ARGUMENT_VALUES];
    int degree[MAX_ARGUMENT_VALUES];
    bool exclude[3];
    color_t color[13];
} filter_argument_values;

typedef struct {
    long size;
    long degree;
} filter_valid_arguments_t;

void get_filter_arguments(const graph_t* graph, filter_valid_arguments_t* valid) {
    valid->size = 1 << 0 | 1 << 1 | 1 << 2;
    valid->degree = 1 << 0 | 1 << 1 | 1 << 2;
    for (const node_t* node = graph->nodes; node; node = node->next) {
        for (int j = 3; j < filter_argument_values.n_size; j++) {
            if (filter_argument_values.size[j] == node->n_subnodes) {
                valid->size |= 1 << j;
                break;
            }
        }
        for (int j = 3; j < filter_argument_values.n_degree; j++) {
            if (filter_argument_values.degree[j] == node->n_edges) {
                valid->degree |= 1 << j;
                break;
            }
        }
    }
}

void init_filter(guide_builder_t* builder) {
    int n_filters = 0;
    while (filter_funcs[n_filters].func != NULL) {
        n_filters++;
    }
    add_choice(builder, n_filters, "filter");

    filter_argument_values.n_size = MAX_ARGUMENT_VALUES;
    int* size = filter_argument_values.size;
    size[0] = MAX_SIZE;
    size[1] = MIN_SIZE;
    size[2] = ODD_SIZE;
    for (int i = 3; i < MAX_ARGUMENT_VALUES; i++) {
        size[i] = i - 2;
    }
    add_choice(builder, filter_argument_values.n_size, "filter:size");

    filter_argument_values.n_degree = MAX_ARGUMENT_VALUES;
    int* degree = filter_argument_values.degree;
    degree[0] = MAX_SIZE;
    degree[1] = MIN_SIZE;
    degree[2] = ODD_SIZE;
    for (int i = 3; i < MAX_ARGUMENT_VALUES; i++) {
        degree[i] = i - 2;
    }
    add_choice(builder, filter_argument_values.n_degree, "filter:degree");

    filter_argument_values.n_exclude = 2;
    bool* exclude = filter_argument_values.exclude;
    exclude[0] = true;
    exclude[1] = false;
    add_choice(builder, filter_argument_values.n_exclude, "filter:exclude");

    filter_argument_values.n_color = 2;
    color_t* color = filter_argument_values.color;
    color[0] = MOST_COMMON_COLOR;
    color[1] = LEAST_COMMON_COLOR;
    for (color_t c = 0; c < 10; c++) {
        color[filter_argument_values.n_color++] = c;
    }
    add_choice(builder, filter_argument_values.n_color, "filter:color");
}

/**
 * Sample a filter, may return NULL when the created sample turned out to be invalid
 */
filter_call_t* sample_filter(task_t* task, const graph_t* graph, trail_t** p_trail) {
    filter_valid_arguments_t arg_vals;
    get_filter_arguments(graph, &arg_vals);

    filter_call_t* call = new_item(task->_mem_filter_calls);
    call->next_in_multi = NULL;
    trail_t* trail = *p_trail;

    const categorical_t* func_dist = next_choice(trail);
    int i_func = choose(func_dist);
    filter_func_t* func = &filter_funcs[i_func];
    call->filter = func;
    trail = observe_choice(trail, i_func);

    const categorical_t* size_dist = next_choice(trail);
    if (func->size) {
        int i_size = choose_from(size_dist, arg_vals.size);
        call->args.size = filter_argument_values.size[i_size];
        trail = observe_choice(trail, i_size);
    } else {
        trail = observe_choice(trail, -1);
    }

    const categorical_t* degree_dist = next_choice(trail);
    if (func->degree) {
        int i_degree = choose_from(degree_dist, arg_vals.degree);
        call->args.degree = filter_argument_values.degree[i_degree];
        trail = observe_choice(trail, i_degree);
    } else {
        trail = observe_choice(trail, -1);
    }

    const categorical_t* exclude_dist = next_choice(trail);
    if (func->exclude) {
        int i_exclude = choose(exclude_dist);
        call->args.exclude = filter_argument_values.exclude[i_exclude];
        trail = observe_choice(trail, i_exclude);
    } else {
        trail = observe_choice(trail, -1);
    }

    const categorical_t* color_dist = next_choice(trail);
    if (func->color) {
        int i_color = choose(color_dist);
        call->args.color = filter_argument_values.color[i_color];
        trail = observe_choice(trail, i_color);
    } else {
        trail = observe_choice(trail, -1);
    }

    if (!filter_matches(graph, call)) {
        while (trail != *p_trail) {
            trail = backtrack(trail);
        }
        free_item(task->_mem_filter_calls, call);
        return NULL;
    } else {
        *p_trail = trail;
        return call;
    }
}

trail_t * observe_filter(trail_t* trail, __attribute__((unused)) filter_call_t* call) {
    trail = observe_choice(trail, -1);
    trail = observe_choice(trail, -1);
    trail = observe_choice(trail, -1);
    trail = observe_choice(trail, -1);
    trail = observe_choice(trail, -1);
    return trail;
}