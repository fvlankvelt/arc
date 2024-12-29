#include <stdio.h>
#include <stdlib.h>

#include "io.h"

int main(int argc, char *argv[]) {
    if (argc < 2) {
        task_def_t * tasks = list_tasks();
        for (task_def_t * task_def = tasks; task_def; task_def = task_def->next) {
            printf("%s\n", task_def->name);
        }
        return 0;
    } else {
        const char * source = read_task(argv[1]);
        if (source) {
            task_t * task = parse_task(source);
            printf("n_train: %d\n", task->n_train);
            filter_call_t * filters = get_candidate_filters(task, &abstractions[0]);
            for (filter_call_t * filter = filters; filter; filter = filter->next) {
                transform_call_t * transform = generate_parameters(task, filter, &abstractions[0]);
                int n = 0;
                for (transform_call_t * call = transform; call; call = call->next) {
                    n++;
                }
                printf("  %d\n", n);
            }
            free_task(task);
            free((char *) source);
        }
    }
}