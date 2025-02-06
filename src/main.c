#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "filter.h"
#include "guide.h"
#include "image.h"
#include "io.h"
#include "transform.h"

int main(int argc, char* argv[]) {
    task_def_t* tasks = list_tasks();
    int n_tasks = 0;
    for (task_def_t* task_def = tasks; task_def; task_def = task_def->next) {
        const char* source = read_task(task_def->name);
        if (source) {
            task_def->task = parse_task(source);
        }
        n_tasks++;
    }

    guide_builder_t builder;
    init_guide(&builder);

    init_image(&builder);
    init_filter(&builder);
    init_binding(&builder);
    init_transform(&builder);
    guide_t* guide = build_guide(&builder);

    printf("task,example,loss,reconstructed,transformed\n");
    while (true) {
        for (task_def_t* task_def = tasks; task_def; task_def = task_def->next) {
            task_t* task = task_def->task;
            if (!task) {
                continue;
            }

            // printf("%s\n", task_def->name);
            for (int i_train = 0; i_train < task->n_train; i_train++) {
                const graph_t* input = task->train_input[i_train];
                const graph_t* output = task->train_output[i_train];
                trail_t* trail = new_trail(input, output, guide);

                abstraction_t* abstraction = sample_abstraction(&trail);
                graph_t* graph = abstraction->func(input);
                filter_call_t* filter = sample_filter(task, graph, &trail);
                if (!filter) {
                    goto no_filter;
                }

                transform_call_t* call = sample_transform(task, graph, filter, &trail);
                if (!call) {
                    goto no_transform;
                }

                // printf("Found training example for %s\n", task_def->name);
                bool transformed = false;
                for (node_t* node = graph->nodes; node; node = node->next) {
                    if (filter->filter->func(graph, node, &filter->args)) {
                        transform_arguments_t transform_args = call->arguments;
                        if (apply_binding(graph, node, &call->dynamic, &transform_args)) {
                            call->transform->func(graph, node, &transform_args);
                            transformed = true;
                        }
                    }
                }

                graph_t* reconstructed = undo_abstraction(graph);
                if (!reconstructed) {
                    goto no_reconstruction;
                }

                bool is_correct = true;
                if (output->width == reconstructed->width &&
                    output->height == reconstructed->height) {
                    for (int x = 0; x < output->width; x++) {
                        for (int y = 0; y < output->height; y++) {
                            const node_t* node = get_node(reconstructed, (coordinate_t){x, y});
                            const node_t* orig = get_node(output, (coordinate_t){x, y});
                            const subnode_t subnode = get_subnode(node, 0);
                            const subnode_t refsub = get_subnode(orig, 0);
                            if (subnode.color != refsub.color) {
                                is_correct = false;
                            }
                        }
                    }
                    if (is_correct) {
                        fprintf(stderr, "  %s: Correct transformation\n", task_def->name);
                    }
                } else {
                    is_correct = false;
                }

                trail_t* train_trail = new_trail(input, reconstructed, guide);
                train_trail = observe_abstraction(train_trail, abstraction);
                train_trail = observe_filter(train_trail, filter);
                train_trail = observe_transform(train_trail, call);
                float loss = free_trail(guide, train_trail, true);
                printf("%s, %d, %f, %d, %d\n", task_def->name, i_train, loss, is_correct, transformed);

                free_graph(reconstructed);

            no_reconstruction:
                free_item(task->_mem_transform_calls, call);

            no_transform:
                free_item(task->_mem_filter_calls, filter);

            no_filter:
                free_graph(graph);

                free_trail(guide, trail, false);
            }
        }
    }

    return 0;
}