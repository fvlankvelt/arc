#include "binding.h"

#include <string.h>

#include "filter.h"
#include "graph.h"

node_t* bind_node_by_size(
    const graph_t* graph,
    __attribute__((unused)) const node_t* node,
    const binding_arguments_t* params) {
    filter_arguments_t args;
    args.size = params->size;
    args.exclude = params->exclude;
    for (node_t* node = graph->nodes; node; node = node->next) {
        if (filter_by_size(graph, node, &args)) {
            return node;
        }
    }
    return NULL;
}

node_t* bind_neighbor_by_size(
    const graph_t* graph, const node_t* node, const binding_arguments_t* params) {
    filter_arguments_t args;
    args.size = params->size;
    args.exclude = params->exclude;
    for (const edge_t* edge = node->edges; edge; edge = edge->next) {
        if (filter_by_size(graph, edge->peer, &args)) {
            return edge->peer;
        }
    }
    return NULL;
}

node_t* bind_neighbor_by_color(
    const graph_t* graph, const node_t* node, const binding_arguments_t* params) {
    filter_arguments_t args;
    args.color = params->color;
    args.exclude = params->exclude;
    for (const edge_t* edge = node->edges; edge; edge = edge->next) {
        node_t* peer = edge->peer;
        if (filter_by_color(graph, peer, &args)) {
            return peer;
        }
    }
    return NULL;
}

node_t* bind_neighbor_by_degree(
    const graph_t* graph, const node_t* node, const binding_arguments_t* params) {
    filter_arguments_t args;
    args.degree = params->degree;
    args.exclude = params->exclude;
    for (const edge_t* edge = node->edges; edge; edge = edge->next) {
        node_t* peer = edge->peer;
        if (filter_by_degree(graph, peer, &args)) {
            return peer;
        }
    }
    return NULL;
}

binding_func_t binding_funcs[] = {
    {
        .func = bind_node_by_size,
        .size = true,
        .exclude = true,
    },
    {
        .func = bind_neighbor_by_size,
        .size = true,
        .exclude = true,
    },
    {
        .func = bind_neighbor_by_color,
        .exclude = true,
        .color = true,
    },
    {
        .func = bind_neighbor_by_degree,
        .exclude = true,
        .degree = true,
    },
    {
        .func = NULL,
    }};

node_t* get_binding_node(const graph_t* graph, const node_t* node, const binding_call_t* call) {
    return call->binding->func(graph, node, &call->args);
}

bool binding_matches(
    const graph_t* graph,
    const filter_call_t* filter_call,
    const binding_call_t* binding_call) {
    for (const node_t* node = graph->nodes; node; node = node->next) {
        if (apply_filter(graph, node, filter_call)) {
            node_t* match = get_binding_node(graph, node, binding_call);
            if (match) {
                return true;
            }
        }
    }
    return false;
}

#define MAX_ARGUMENT_VALUES 20

struct {
    int n_bindings;
    int n_size;
    int n_degree;
    int n_exclude;
    int n_color;
    int size[MAX_ARGUMENT_VALUES];
    int degree[MAX_ARGUMENT_VALUES];
    bool exclude[3];
    color_t color[13];
} binding_argument_values;

typedef struct {
    long size;
    long degree;
} binding_valid_arguments_t;

void get_binding_arguments(const graph_t* graph, binding_valid_arguments_t* valid) {
    valid->size = 1 << 0 | 1 << 1 | 1 << 2;
    valid->degree = 1 << 0 | 1 << 1 | 1 << 2;
    for (const node_t* node = graph->nodes; node; node = node->next) {
        // FIXME: sizes are fixed, no need to look them up
        for (int j = 3; j < binding_argument_values.n_size; j++) {
            if (binding_argument_values.size[j] == node->n_subnodes) {
                valid->size |= 1 << j;
                break;
            }
        }
        for (int j = 3; j < binding_argument_values.n_degree; j++) {
            if (binding_argument_values.degree[j] == node->n_edges) {
                valid->degree |= 1 << j;
                break;
            }
        }
    }
}

void init_binding(__attribute__((unused)) guide_builder_t* builder) {
    binding_argument_values.n_bindings = 0;
    while (binding_funcs[binding_argument_values.n_bindings].func != NULL) {
        binding_argument_values.n_bindings++;
    }

    binding_argument_values.n_size = MAX_ARGUMENT_VALUES;
    int* size = binding_argument_values.size;
    size[0] = MAX_SIZE;
    size[1] = MIN_SIZE;
    size[2] = ODD_SIZE;
    for (int i = 3; i < MAX_ARGUMENT_VALUES; i++) {
        size[i] = i - 2;
    }

    binding_argument_values.n_degree = MAX_ARGUMENT_VALUES;
    int* degree = binding_argument_values.degree;
    degree[0] = MAX_SIZE;
    degree[1] = MIN_SIZE;
    degree[2] = ODD_SIZE;
    for (int i = 3; i < MAX_ARGUMENT_VALUES; i++) {
        degree[i] = i - 2;
    }

    binding_argument_values.n_exclude = 2;
    bool* exclude = binding_argument_values.exclude;
    exclude[0] = true;
    exclude[1] = false;

    binding_argument_values.n_color = 2;
    color_t* color = binding_argument_values.color;
    color[0] = MOST_COMMON_COLOR;
    color[1] = LEAST_COMMON_COLOR;
    for (color_t c = 0; c < 10; c++) {
        color[binding_argument_values.n_color++] = c;
    }
}

void add_binding_choice(
    guide_builder_t* builder, int n_choices, const char* prefix, const char* suffix) {
    char* buffer = malloc(strlen(prefix) + strlen(suffix) + 2);
    sprintf(buffer, "%s:%s", prefix, suffix);
    add_choice(builder, n_choices, buffer);
}

void add_binding(guide_builder_t* builder, const char* prefix) {
    add_binding_choice(builder, binding_argument_values.n_bindings, prefix, "func");
    add_binding_choice(builder, binding_argument_values.n_size, prefix, "size");
    add_binding_choice(builder, binding_argument_values.n_degree, prefix, "degree");
    add_binding_choice(builder, binding_argument_values.n_exclude, prefix, "exclude");
    add_binding_choice(builder, binding_argument_values.n_color, prefix, "color");
}

binding_call_t* sample_binding(
    task_t* task, const graph_t* graph, const filter_call_t* filter, trail_t** p_trail) {
    binding_valid_arguments_t arg_vals;
    get_binding_arguments(graph, &arg_vals);

    binding_call_t* call = new_item(task->_mem_binding_calls);
    trail_t* trail = *p_trail;

    const categorical_t* func_dist = next_choice(trail);
    int i_func = choose(func_dist);
    binding_func_t* func = &binding_funcs[i_func];
    call->binding = func;
    trail = observe_choice(trail, i_func);

    const categorical_t* size_dist = next_choice(trail);
    if (func->size) {
        int i_size = choose_from(size_dist, arg_vals.size);
        call->args.size = binding_argument_values.size[i_size];
        trail = observe_choice(trail, i_size);
    } else {
        trail = observe_choice(trail, -1);
    }

    const categorical_t* degree_dist = next_choice(trail);
    if (func->degree) {
        int i_degree = choose_from(degree_dist, arg_vals.degree);
        call->args.degree = binding_argument_values.degree[i_degree];
        trail = observe_choice(trail, i_degree);
    } else {
        trail = observe_choice(trail, -1);
    }

    const categorical_t* exclude_dist = next_choice(trail);
    if (func->exclude) {
        int i_exclude = choose(exclude_dist);
        call->args.exclude = binding_argument_values.exclude[i_exclude];
        trail = observe_choice(trail, i_exclude);
    } else {
        trail = observe_choice(trail, -1);
    }

    const categorical_t* color_dist = next_choice(trail);
    if (func->color) {
        int i_color = choose(color_dist);
        call->args.color = binding_argument_values.color[i_color];
        trail = observe_choice(trail, i_color);
    } else {
        trail = observe_choice(trail, -1);
    }

    if (!binding_matches(graph, filter, call)) {
        while (trail != *p_trail) {
            trail = backtrack(trail);
        }
        free_item(task->_mem_binding_calls, call);
        return NULL;
    } else {
        *p_trail = trail;
        return call;
    }
}

trail_t* observe_binding(trail_t* trail, const binding_call_t* call) {
    const categorical_t* func_dist = next_choice(trail);
    binding_func_t* func = NULL;
    if (call) {
        for (int i_func = 0; &binding_funcs[i_func]; i_func++) {
            func = &binding_funcs[i_func];
            if (call->binding == func) {
                trail = observe_choice(trail, i_func);
                break;
            }
        }
        assert(func);
    } else {
        trail = observe_choice(trail, -1);
    }

    const categorical_t* size_dist = next_choice(trail);
    if (func && func->size) {
        for (int i_size = 0; i_size < binding_argument_values.n_size; i_size++) {
            if (call->args.size == binding_argument_values.size[i_size]) {
                trail = observe_choice(trail, i_size);
                break;
            }
        }
    } else {
        trail = observe_choice(trail, -1);
    }

    const categorical_t* degree_dist = next_choice(trail);
    if (func && func->degree) {
        for (int i_degree = 0; i_degree < binding_argument_values.n_degree; i_degree++) {
            if (call->args.degree == binding_argument_values.degree[i_degree]) {
                trail = observe_choice(trail, i_degree);
                break;
            }
        }
    } else {
        trail = observe_choice(trail, -1);
    }

    const categorical_t* exclude_dist = next_choice(trail);
    if (func && func->exclude) {
        for (int i_exclude = 0; i_exclude < binding_argument_values.n_exclude; i_exclude++) {
            if (call->args.exclude == binding_argument_values.exclude[i_exclude]) {
                trail = observe_choice(trail, i_exclude);
                break;
            }
        }
    } else {
        trail = observe_choice(trail, -1);
    }

    const categorical_t* color_dist = next_choice(trail);
    if (func && func->color) {
        for (int i_color = 0; i_color < binding_argument_values.n_color; i_color++) {
            if (call->args.color == binding_argument_values.color[i_color]) {
                trail = observe_choice(trail, i_color);
                break;
            }
        }
    } else {
        trail = observe_choice(trail, -1);
    }

    return trail;
}
