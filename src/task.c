#include "task.h"

#include "binding.h"
#include "filter.h"
#include "graph.h"
#include "image.h"
#include "mem.h"
#include "transform.h"

#define MAX_ARGUMENT_VALUES 20

// #define handle_value(b, i, n) (!(i) || ((b) && (i) < (n)))
// for (int i_size = 0; handle_value(def->size, i_size, arg_vals.n_size); i_size++) {

#define for_each_or_once(cond, arr, item, len)                      \
    typeof(*arr) item = arr[0];                                     \
    for (int i_##item = 0; !i_##item || ((cond) && i_##item < len); \
         i_##item++, item = arr[i_##item + 1])

task_t* new_task() {
    task_t* task = malloc(sizeof(task_t));
    task->n_train = 0;
    task->n_test = 0;
    task->_mem_filter_calls = new_block(256, sizeof(filter_call_t));
    task->_mem_binding_calls = new_block(256, sizeof(binding_call_t));
    task->_mem_transform_calls = new_block(256, sizeof(transform_call_t));
    return task;
}

void free_task(task_t* task) {
    free_block(task->_mem_transform_calls);
    free_block(task->_mem_binding_calls);
    free_block(task->_mem_filter_calls);
    free(task);
}

bool filter_matches(
    const task_t* task, const graph_t** abstracted_graphs, const filter_call_t* call) {
    for (int i_train = 0; i_train < task->n_train; i_train++) {
        const graph_t* graph = abstracted_graphs[i_train];
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
    }
    return true;
}

filter_call_t* get_candidate_filters(task_t* task, const abstraction_t* abstraction) {
    const graph_t* abstracted_graphs[task->n_train];
    for (int i_train = 0; i_train < task->n_train; i_train++) {
        abstracted_graphs[i_train] = abstraction->func(task->train_input[i_train]);
    }

    struct {
        int n_size;
        int n_degree;
        int n_exclude;
        int n_color;
        int size[MAX_ARGUMENT_VALUES];
        int degree[MAX_ARGUMENT_VALUES];
        bool exclude[2];
        color_t color[13];
    } arg_vals = {0};

    arg_vals.size[0] = MAX_SIZE;
    arg_vals.size[1] = MIN_SIZE;
    arg_vals.size[2] = ODD_SIZE;
    arg_vals.n_size = 3;
    arg_vals.degree[0] = MAX_SIZE;
    arg_vals.degree[1] = MIN_SIZE;
    arg_vals.degree[2] = ODD_SIZE;
    arg_vals.n_degree = 3;
    for (int i_train = 0; i_train < task->n_train; i_train++) {
        const graph_t* graph = abstracted_graphs[i_train];
        for (const node_t* node = graph->nodes; node; node = node->next) {
            bool found = false;
            for (int j = 0; j < arg_vals.n_size; j++) {
                if (arg_vals.size[j] == node->n_subnodes) {
                    found = true;
                    break;
                }
            }
            if (!found && arg_vals.n_size < MAX_ARGUMENT_VALUES) {
                arg_vals.size[arg_vals.n_size++] = node->n_subnodes;
            }

            found = false;
            for (int j = 0; j < arg_vals.n_degree; j++) {
                if (arg_vals.degree[j] == node->n_edges) {
                    found = true;
                    break;
                }
            }
            if (!found && arg_vals.n_degree < MAX_ARGUMENT_VALUES) {
                arg_vals.degree[arg_vals.n_degree++] = node->n_edges;
            }
        }
    }

    arg_vals.n_exclude = 2;
    arg_vals.exclude[0] = true;
    arg_vals.exclude[1] = false;

    arg_vals.n_color = 2;
    arg_vals.color[0] = MOST_COMMON_COLOR;
    arg_vals.color[1] = LEAST_COMMON_COLOR;
    for (color_t color = 0; color < 10; color++) {
        arg_vals.color[arg_vals.n_color++] = color;
    }

    // TODO: dedupe filters that return the same set of nodes
    filter_call_t* result = NULL;
    for (int i = 0; filter_funcs[i].func; i++) {
        const filter_func_t* def = &filter_funcs[i];
        for_each_or_once(def->size, arg_vals.size, size, arg_vals.n_size) {
            for_each_or_once(def->degree, arg_vals.degree, degree, arg_vals.n_degree) {
                for_each_or_once(def->exclude, arg_vals.exclude, exclude, arg_vals.n_exclude) {
                    for_each_or_once(def->color, arg_vals.color, color, arg_vals.n_color) {
                        filter_call_t* call = new_item(task->_mem_filter_calls);
                        (*call) = (filter_call_t){
                            .filter = def,
                            .args =
                                (filter_arguments_t){
                                    .size = size,
                                    .degree = degree,
                                    .exclude = exclude,
                                    .color = color,
                                },
                        };
                        bool match_all = filter_matches(task, abstracted_graphs, call);
                        if (match_all) {
                            call->next = result;
                            result = call;
                        } else {
                            free_item(task->_mem_filter_calls, call);
                        }
                    }
                }
            }
        }
    }

    for (filter_call_t* first = result; first; first = first->next) {
        for (filter_call_t* second = first->next; second; second = second->next) {
            filter_call_t* candidate = new_item(task->_mem_filter_calls);
            (*candidate) = (filter_call_t){
                .next = first,
                .filter = second->filter,
                .args = second->args,
            };
            bool match_all = filter_matches(task, abstracted_graphs, candidate);
            if (match_all) {
                candidate->next = result;
                result = candidate;
            } else {
                free_item(task->_mem_filter_calls, candidate);
            }
        }
    }

    return result;
}

bool binding_matches(
    const task_t* task,
    const graph_t** abstracted_graphs,
    const filter_call_t* filter_call,
    const binding_call_t* binding_call) {
    for (int i_train = 0; i_train < task->n_train; i_train++) {
        const graph_t* graph = abstracted_graphs[i_train];
        bool found_node = false;
        for (const node_t* node = graph->nodes; node; node = node->next) {
            if (apply_filter(graph, node, filter_call)) {
                node_t* match = get_binding_node(graph, node, binding_call);
                if (match) {
                    found_node = true;
                    break;
                }
            }
        }
        if (!found_node) {
            return false;
        }
    }
    return true;
}

binding_call_t* get_bindings(
    const task_t* task, const filter_call_t* filter, const abstraction_t* abstraction) {
    const graph_t* abstracted_graphs[task->n_train];
    for (int i_train = 0; i_train < task->n_train; i_train++) {
        abstracted_graphs[i_train] = abstraction->func(task->train_input[i_train]);
    }

    struct {
        int n_size;
        int n_degree;
        int n_exclude;
        int n_color;
        int size[MAX_ARGUMENT_VALUES];
        int degree[MAX_ARGUMENT_VALUES];
        bool exclude[2];
        color_t color[13];
    } arg_vals = {0};

    arg_vals.size[0] = MAX_SIZE;
    arg_vals.size[1] = MIN_SIZE;
    arg_vals.size[2] = ODD_SIZE;
    arg_vals.n_size = 3;
    arg_vals.degree[0] = MAX_SIZE;
    arg_vals.degree[1] = MIN_SIZE;
    arg_vals.degree[2] = ODD_SIZE;
    arg_vals.n_degree = 3;
    for (int i_train = 0; i_train < task->n_train; i_train++) {
        const graph_t* graph = abstracted_graphs[i_train];
        for (const node_t* node = graph->nodes; node; node = node->next) {
            bool found = false;
            for (int j = 0; j < arg_vals.n_size; j++) {
                if (arg_vals.size[j] == node->n_subnodes) {
                    found = true;
                    break;
                }
            }
            if (!found && arg_vals.n_size < MAX_ARGUMENT_VALUES) {
                arg_vals.size[arg_vals.n_size++] = node->n_subnodes;
            }

            found = false;
            for (int j = 0; j < arg_vals.n_degree; j++) {
                if (arg_vals.degree[j] == node->n_edges) {
                    found = true;
                    break;
                }
            }
            if (!found && arg_vals.n_degree < MAX_ARGUMENT_VALUES) {
                arg_vals.degree[arg_vals.n_degree++] = node->n_edges;
            }
        }
    }

    arg_vals.n_exclude = 2;
    arg_vals.exclude[0] = true;
    arg_vals.exclude[1] = false;

    arg_vals.n_color = 2;
    arg_vals.color[0] = MOST_COMMON_COLOR;
    arg_vals.color[1] = LEAST_COMMON_COLOR;
    for (color_t color = 0; color < 10; color++) {
        arg_vals.color[arg_vals.n_color++] = color;
    }

    // TODO: dedupe filters that return the same set of nodes
    binding_call_t* result = NULL;
    for (int i = 0; binding_funcs[i].func; i++) {
        const binding_func_t* def = &binding_funcs[i];
        for_each_or_once(def->size, arg_vals.size, size, arg_vals.n_size) {
            for_each_or_once(def->degree, arg_vals.degree, degree, arg_vals.n_degree) {
                for_each_or_once(def->exclude, arg_vals.exclude, exclude, arg_vals.n_exclude) {
                    for_each_or_once(def->color, arg_vals.color, color, arg_vals.n_color) {
                        binding_call_t* call = new_item(task->_mem_binding_calls);
                        (*call) = (binding_call_t){
                            .binding = def,
                            .args =
                                (binding_arguments_t){
                                    .size = size,
                                    .degree = degree,
                                    .exclude = exclude,
                                    .color = color,
                                },
                        };
                        bool match_all = binding_matches(task, abstracted_graphs, filter, call);
                        if (match_all) {
                            call->next = result;
                            result = call;
                        } else {
                            free_item(task->_mem_binding_calls, call);
                        }
                    }
                }
            }
        }
    }

    return result;
}

transform_call_t* generate_parameters(
    const task_t* task, const filter_call_t* call, const transform_func_t* transform) {
    struct {
        int n_color;
        int n_direction;
        int n_rotation;
        int n_overlap;
        color_t color[13];
        direction_t direction[9];
        rotation_t rotation[3];
        bool overlap[2];

        binding_call_t* color_call[MAX_ARGUMENT_VALUES];
        binding_call_t* direction_call[MAX_ARGUMENT_VALUES];
        binding_call_t* mirror_axis_call[MAX_ARGUMENT_VALUES];
        binding_call_t* point_call[MAX_ARGUMENT_VALUES];
    } arg_vals = {0};

    arg_vals.n_color = 2;
    arg_vals.color[0] = MOST_COMMON_COLOR;
    arg_vals.color[1] = LEAST_COMMON_COLOR;
    for (color_t color = 0; color < 10; color++) {
        arg_vals.color[arg_vals.n_color++] = color;
    }

    arg_vals.n_direction = DOWN_RIGHT + 1;
    for (direction_t dir = UP; dir <= DOWN_RIGHT; dir++) {
        arg_vals.direction[dir] = dir;
    }

    arg_vals.n_rotation = COUNTER_CLOCK_WISE + 1;
    for (rotation_t rot = CLOCK_WISE; rot <= COUNTER_CLOCK_WISE; rot++) {
        arg_vals.rotation[rot] = rot;
    }

    arg_vals.n_overlap = 2;
    arg_vals.overlap[0] = true;
    arg_vals.overlap[1] = false;

    transform_call_t* result = NULL;
    return result;
}