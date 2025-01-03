#include "filter.h"

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

filter_func_t filter_funcs[] = {
    {
        .func = filter_by_color,
        .color = true,
        .exclude = true,
    },
    {
        .func = filter_by_size,
        .size = true,
        .exclude = true,
    },
    {
        .func = filter_by_degree,
        .degree = true,
        .exclude = true,
    },
    {
        .func = NULL,
    }};

bool apply_filter(const graph_t* graph, const node_t* node, const filter_call_t* call) {
    for (filter_call_t* current = (filter_call_t*)call; current; current = current->next) {
        if (!current->filter->func(graph, node, &current->args)) {
            return false;
        }
    }
    return true;
}