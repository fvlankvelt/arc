#include "arga_task.h"

#include "binding.h"
#include "filter.h"
#include "transform.h"

#include "../util/task.h"

graph_t * convert_grid(const grid_t * grid) {
    return graph_from_grid(grid->cells, grid->height, grid->width);
}

arga_task_t* new_arga_task(task_t* task) {
    arga_task_t* arga_task = (arga_task_t*)malloc(sizeof(arga_task_t));
    arga_task->task = task;
    for (int i_train = 0; i_train < task->n_train; i_train++) {
        arga_task->train_input[i_train] = convert_grid(task->train_input[i_train]);
        arga_task->train_output[i_train] = convert_grid(task->train_output[i_train]);
    }
    for (int i_test = 0; i_test < task->n_test; i_test++) {
        arga_task->test_input[i_test] = convert_grid(task->test_input[i_test]);
        arga_task->test_output[i_test] = convert_grid(task->test_output[i_test]);
    }
    arga_task->_mem_filter_calls = new_block(256, sizeof(filter_call_t));
    arga_task->_mem_binding_calls = new_block(256, sizeof(binding_call_t));
    arga_task->_mem_transform_calls = new_block(256, sizeof(transform_call_t));
    return arga_task;
}

void free_arga_task(arga_task_t* arga_task) {
    for (int i_train = 0; i_train < arga_task->task->n_train; i_train++) {
        free_graph((graph_t*)arga_task->train_input[i_train]);
        free_graph((graph_t*)arga_task->train_output[i_train]);
    }
    for (int i_test = 0; i_test < arga_task->task->n_test; i_test++) {
        free_graph((graph_t*)arga_task->test_input[i_test]);
        free_graph((graph_t*)arga_task->test_output[i_test]);
    }
    free_block(arga_task->_mem_transform_calls);
    free_block(arga_task->_mem_binding_calls);
    free_block(arga_task->_mem_filter_calls);
    free(arga_task);
}
