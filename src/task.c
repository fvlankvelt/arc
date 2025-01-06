#include "task.h"

#include <stdio.h>

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
    for (int i_train = 0; i_train < task->n_train; i_train++) {
        free_graph((graph_t*)task->train_input[i_train]);
        free_graph((graph_t*)task->train_output[i_train]);
    }
    for (int i_test = 0; i_test < task->n_test; i_test++) {
        free_graph((graph_t*)task->test_input[i_test]);
        free_graph((graph_t*)task->test_output[i_test]);
    }
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
        bool exclude[3];
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
                .next_in_multi = first,
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

    for (int i_train = 0; i_train < task->n_train; i_train++) {
        free_graph((graph_t*)abstracted_graphs[i_train]);
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
        bool exclude[3];
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

    for (int i_train = 0; i_train < task->n_train; i_train++) {
        free_graph((graph_t*)abstracted_graphs[i_train]);
    }

    return result;
}

typedef struct _param_binding {
    bool is_call;
    color_t color;
    direction_t direction;
    mirror_axis_t mirror_axis;
    image_points_t image_point;
    binding_call_t* call;
} param_binding_t;

transform_call_t* generate_parameters(
    const task_t* task, const filter_call_t* filter, const abstraction_t* abstraction) {
    struct {
        int n_color;
        int n_direction;
        int n_mirror_axis;
        int n_point;

        int n_rotation;
        int n_overlap;
        int n_mirror_direction;
        int n_object_id;
        int n_relative_pos;

        // fixed values
        rotation_t rotation[3];
        bool overlap[3];
        mirror_t mirror_direction[4];
        short object_id[MAX_ARGUMENT_VALUES];
        relative_position_t relative_pos[3];

        // dynamic values
        param_binding_t color[MAX_ARGUMENT_VALUES];
        param_binding_t direction[MAX_ARGUMENT_VALUES];
        param_binding_t mirror_axis[MAX_ARGUMENT_VALUES];
        param_binding_t point[MAX_ARGUMENT_VALUES];
    } arg_vals = {0};

    arg_vals.n_rotation = COUNTER_CLOCK_WISE + 1;
    for (rotation_t rot = CLOCK_WISE; rot <= COUNTER_CLOCK_WISE; rot++) {
        arg_vals.rotation[rot] = rot;
    }

    arg_vals.n_overlap = 2;
    arg_vals.overlap[0] = true;
    arg_vals.overlap[1] = false;

    arg_vals.n_mirror_direction = MIRROR_DIAGONAL_RIGHT + 1;
    for (mirror_t mirror = MIRROR_VERTICAL; mirror <= MIRROR_DIAGONAL_RIGHT; mirror++) {
        arg_vals.mirror_direction[mirror] = mirror;
    }

    arg_vals.n_object_id = 0;

    arg_vals.n_relative_pos = MIDDLE + 1;
    for (relative_position_t rel_pos = SOURCE; rel_pos <= MIDDLE; rel_pos++) {
        arg_vals.relative_pos[rel_pos] = rel_pos;
    }

    arg_vals.n_color = 2;
    arg_vals.color[0] = (param_binding_t){
        .is_call = false,
        .color = MOST_COMMON_COLOR,
    };
    arg_vals.color[1] = (param_binding_t){
        .is_call = false,
        .color = LEAST_COMMON_COLOR,
    };
    for (color_t color = 0; color < 10; color++) {
        arg_vals.color[arg_vals.n_color++] = (param_binding_t){
            .is_call = false,
            .color = color,
        };
    }

    arg_vals.n_direction = DOWN_RIGHT + 1;
    for (direction_t dir = UP; dir <= DOWN_RIGHT; dir++) {
        arg_vals.direction[dir] = (param_binding_t){
            .is_call = false,
            .direction = dir,
        };
    }

    arg_vals.n_mirror_axis = 0;
    arg_vals.n_point = IMAGE_BOTTOM_RIGHT + 1;
    for (image_points_t point = IMAGE_TOP; point <= IMAGE_BOTTOM_RIGHT; point++) {
        arg_vals.point[point] = (param_binding_t){
            .is_call = false,
            .image_point = point,
        };
    }

    binding_call_t* bindings = get_bindings(task, filter, abstraction);
    for (binding_call_t* binding = bindings; binding; binding = binding->next) {
        if (arg_vals.n_color < MAX_ARGUMENT_VALUES) {
            arg_vals.color[arg_vals.n_color++] = (param_binding_t){
                .is_call = true,
                .call = binding,
            };
        }
        if (arg_vals.n_direction < MAX_ARGUMENT_VALUES) {
            arg_vals.direction[arg_vals.n_direction++] = (param_binding_t){
                .is_call = true,
                .call = binding,
            };
        }
        if (arg_vals.n_mirror_axis < MAX_ARGUMENT_VALUES) {
            arg_vals.mirror_axis[arg_vals.n_mirror_axis++] = (param_binding_t){
                .is_call = true,
                .call = binding,
            };
        }
        if (arg_vals.n_point < MAX_ARGUMENT_VALUES) {
            arg_vals.point[arg_vals.n_point++] = (param_binding_t){
                .is_call = true,
                .call = binding,
            };
        }
    }

    transform_call_t* result = NULL;
    for (int i_func = 0; transformations[i_func].func; i_func++) {
        transform_func_t* func = &transformations[i_func];
        for_each_or_once(func->color, arg_vals.color, color, arg_vals.n_color) {
            for_each_or_once(
                func->direction, arg_vals.direction, direction, arg_vals.n_direction) {
                for_each_or_once(func->overlap, arg_vals.overlap, overlap, arg_vals.n_overlap) {
                    for_each_or_once(
                        func->rotation_dir, arg_vals.rotation, rotation, arg_vals.n_rotation) {
                        for_each_or_once(
                            func->mirror_axis,
                            arg_vals.mirror_axis,
                            mirror_axis,
                            arg_vals.n_mirror_axis) {
                            for_each_or_once(
                                func->mirror_direction,
                                arg_vals.mirror_direction,
                                mirror_direction,
                                arg_vals.n_mirror_direction) {
                                for_each_or_once(
                                    func->object_id,
                                    arg_vals.object_id,
                                    object_id,
                                    arg_vals.n_object_id) {
                                    for_each_or_once(
                                        func->point, arg_vals.point, point, arg_vals.n_point) {
                                        for_each_or_once(
                                            func->relative_pos,
                                            arg_vals.relative_pos,
                                            relative_pos,
                                            arg_vals.n_relative_pos) {
                                            transform_call_t* call =
                                                new_item(task->_mem_transform_calls);
                                            call->transform = func;
                                            call->arguments = (transform_arguments_t){
                                                .color = color.color,
                                                .direction = direction.direction,
                                                .mirror_axis = mirror_axis.mirror_axis,
                                                .point = point.image_point,
                                                .overlap = overlap,
                                                .rotation_dir = rotation,
                                                .mirror_direction = mirror_direction,
                                                .object_id = object_id,
                                                .relative_pos = relative_pos,
                                            };
                                            call->dynamic = (transform_dynamic_arguments_t){
                                                .color = color.call,
                                                .direction = direction.call,
                                                .mirror_axis = mirror_axis.call,
                                                .point = point.call,
                                            };
                                            call->next = result;
                                            result = call;
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    return result;
}