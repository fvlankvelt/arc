#ifndef __TASK_H__
#define __TASK_H__

#include "graph.h"
#include "mem.h"

#define MAX_TRAIN_EXAMPLES 10
#define MAX_TEST_INPUT 5

typedef struct _task {
    int n_train;
    int n_test;
    const graph_t* train_input[MAX_TRAIN_EXAMPLES];
    const graph_t* train_output[MAX_TRAIN_EXAMPLES];
    const graph_t* test_input[MAX_TEST_INPUT];
    const graph_t* test_output[MAX_TEST_INPUT];

    // workspace
    mem_block_t* _mem_filter_calls;
    mem_block_t* _mem_binding_calls;
    mem_block_t* _mem_transform_calls;
} task_t;

task_t* new_task();
void free_task(task_t* task);

#endif  // __TASK_H__