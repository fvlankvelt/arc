#ifndef __TASK_H__
#define __TASK_H__

#define MAX_TRAIN_EXAMPLES 10
#define MAX_TEST_INPUT 5

typedef char color_t;
#define BACKGROUND_COLOR ((color_t) - 1)
#define MOST_COMMON_COLOR ((color_t) - 2)
#define LEAST_COMMON_COLOR ((color_t) - 3)
// #define SAME_COLOR ((color_t) - 4)

typedef struct _grid {
    int width;
    int height;
    color_t * cells;
} grid_t;

typedef struct _task {
    int n_train;
    int n_test;
    grid_t* train_input[MAX_TRAIN_EXAMPLES];
    grid_t* train_output[MAX_TRAIN_EXAMPLES];
    grid_t* test_input[MAX_TEST_INPUT];
    grid_t* test_output[MAX_TEST_INPUT];
} task_t;

task_t* new_task();
void free_task(task_t* task);

#endif  // __TASK_H__