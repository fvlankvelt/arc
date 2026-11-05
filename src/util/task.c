#include "task.h"
#include "mem.h"

task_t* new_task() {
    task_t* task = malloc(sizeof(task_t));
    task->n_train = 0;
    task->n_test = 0;
    return task;
}

void free_task(task_t* task) {
    for (int i_train = 0; i_train < task->n_train; i_train++) {
        free(task->train_input[i_train]);
        free(task->train_output[i_train]);
    }
    for (int i_test = 0; i_test < task->n_test; i_test++) {
        free(task->test_input[i_test]);
        free(task->test_output[i_test]);
    }
    free(task);
}