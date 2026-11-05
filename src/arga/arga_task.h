#ifndef ARC_ARGA_TASK_H
#define ARC_ARGA_TASK_H

#include "graph.h"
#include "../util/mem.h"
#include "../util/task.h"

typedef struct _arga_task {
    task_t * task;

    const graph_t* train_input[MAX_TRAIN_EXAMPLES];
    const graph_t* train_output[MAX_TRAIN_EXAMPLES];
    const graph_t* test_input[MAX_TEST_INPUT];
    const graph_t* test_output[MAX_TEST_INPUT];

    // workspace
    mem_block_t* _mem_filter_calls;
    mem_block_t* _mem_binding_calls;
    mem_block_t* _mem_transform_calls;
} arga_task_t;

arga_task_t* new_arga_task(task_t * task);
void free_arga_task(arga_task_t* arga_task);

#endif //ARC_ARGA_TASK_H
