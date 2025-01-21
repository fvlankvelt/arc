#include "task.h"

#include <stdio.h>

#include "binding.h"
#include "filter.h"
#include "graph.h"
#include "image.h"
#include "mem.h"
#include "transform.h"
#include "guide.h"

#define MAX_ARGUMENT_VALUES 20

// #define handle_value(b, i, n) (!(i) || ((b) && (i) < (n)))
// for (int i_size = 0; handle_value(def->size, i_size, arg_vals.n_size); i_size++) {

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

typedef struct _param_binding {
    bool is_call;
    color_t color;
    direction_t direction;
    mirror_axis_t mirror_axis;
    image_points_t image_point;
    binding_call_t* call;
} param_binding_t;

/*
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
*/