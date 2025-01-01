#include "io.h"
#include "test.h"

BEGIN_TEST(test_read_task) {
    task_t* task = parse_task("{\"train\":[],\"test\":[]}");
    ASSERT(task->n_train == 0, "Incorrect number of training examples");
    ASSERT(task->n_test == 0, "Incorrect number of test examples");
    free_task(task);

    task = parse_task(
        "{\"train\":[{\"input\":[[0, 1, 2], [2, 1, 0]],\"output\":[[1, 2], [2, "
        "1]]}],\"test\":[]}");
    ASSERT(task->n_train == 1, "Incorrect number of training examples");
    const graph_t* input = task->train_input[0];
    ASSERT(input->width == 3, "Incorrect width of grid");
    ASSERT(input->height == 2, "Incorrect height of grid");
    const graph_t* output = task->train_output[0];
    ASSERT(task->n_test == 0, "Incorrect number of test examples");
    free_task(task);
}
END_TEST()

BEGIN_TEST(test_list_tasks) {
    task_def_t* tasks = list_tasks();
    ASSERT(tasks, "No tasks found")
    free_task_list(tasks);
}
END_TEST()

DEFINE_SUITE(test_io, {
    RUN_TEST(test_read_task);
    RUN_TEST(test_list_tasks);
})