#include "image.h"
#include "task.h"
#include "test.h"

BEGIN_TEST(test_get_candidate_filters) {
    task_t* task = new_task();
    // clang-format off
    color_t grid[] = {
      2, 2, 0,
      2, 0, 0,
      2, 0, 2,
    };
    // clang-format on
    task->n_train = 1;
    task->train_input[0] = graph_from_grid(grid, 3, 3);
    task->train_output[0] = graph_from_grid(grid, 3, 3);

    filter_call_t* calls = get_candidate_filters(task, &abstractions[0]);
    ASSERT(calls, "no filter calls found");
    int count = 0;
    for (filter_call_t* call = calls; call; call = call->next) {
        count++;
    }
    ASSERT(count == 210, "Number of filters does not match");

    transform_call_t* transform = generate_parameters(task, calls, &abstractions[0]);
    count = 0;
    for (transform_call_t* call = transform; call; call = call->next) {
        count++;
    }
    ASSERT(count == 52, "Number of transforms does not match");
}
END_TEST()

DEFINE_SUITE(test_task, { RUN_TEST(test_get_candidate_filters); })
