#include "image.h"
#include "task.h"
#include "test.h"

DEFINE_TEST(test_get_candidate_filters, ({
    task_t * task = new_task();
    // clang-format off
    color_t grid[] = {
      2, 2, 0,
      2, 0, 0,
      2, 0, 2,
    };
    // clang-format on
    graph_t* graph = graph_from_grid(grid, 3, 3);
    task->n_train = 1;
    task->train_input[0] = graph;

    filter_call_t * calls = get_candidate_filters(task, &abstractions[0]);
    ASSERT(calls, "no filter calls found");
    int count = 0;
    for (filter_call_t * call = calls; call; call = call->next) {
        count++;
    }
    ASSERT(count == 231, "Number of calls does not match");
    free_graph(graph);
}))

RUN_SUITE(test_task, {
    RUN_TEST(test_get_candidate_filters);
})
