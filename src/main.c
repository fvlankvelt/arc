#include <stdio.h>
#include <stdlib.h>

#include "io.h"

int main(int argc, char* argv[]) {
    if (argc < 2) {
        task_def_t* tasks = list_tasks();
        for (task_def_t* task_def = tasks; task_def; task_def = task_def->next) {
            printf("%s\n", task_def->name);
        }
        return 0;
    } else {
        const char* source = read_task(argv[1]);
        if (source) {
            task_t* task = parse_task(source);
            printf("n_train: %d, n_test: %d\n", task->n_train, task->n_test);
            filter_call_t* filters = get_candidate_filters(task, &abstractions[0]);
            for (filter_call_t* filter = filters; filter; filter = filter->next) {
                transform_call_t* transform =
                    generate_parameters(task, filter, &abstractions[0]);
                int n = 0;
                for (transform_call_t* call = transform; call; call = call->next) {
                    bool all_correct = true;
                    for (int i_test = 0; i_test < task->n_test; i_test++) {
                        const graph_t* test_in = task->test_input[i_test];
                        graph_t* abs = (&abstractions[0])->func(test_in);
                        bool transformed = false;
                        for (node_t* node = abs->nodes; node; node = node->next) {
                            if (filter->filter->func(abs, node, &filter->args)) {
                                transform_arguments_t transform_args = call->arguments;
                                apply_binding(abs, node, &call->dynamic, &transform_args);
                                call->transform->func(abs, node, &transform_args);
                                transformed = true;
                                break;
                            }
                        }
                        // verify output
                        if (!transformed) {
                            all_correct = false;
                        } else {
                            graph_t* reconstructed = undo_abstraction(abs);
                            const graph_t* target = task->test_output[i_test];
                            if (target->width != reconstructed->width ||
                                target->height != reconstructed->height) {
                                all_correct = false;
                            } else {
                                for (int x = 0; x < target->width; x++) {
                                    for (int y = 0; y < target->height; y++) {
                                        const node_t* node =
                                            get_node(reconstructed, (coordinate_t){x, y});
                                        const node_t* orig =
                                            get_node(target, (coordinate_t){x, y});
                                        const subnode_t subnode = get_subnode(node, 0);
                                        const subnode_t refsub = get_subnode(orig, 0);
                                        if (subnode.color != refsub.color) {
                                            all_correct = false;
                                        }
                                    }
                                }
                            }
                            free_graph(reconstructed);
                        }
                        free_graph(abs);
                    }
                    if (all_correct) {
                        printf("FOUND SOLUTION!\n");
                    }
                    n++;
                }
            }
            free_task(task);
            free((char*)source);
        }
    }
    return 0;
}