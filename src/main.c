#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "filter.h"
#include "guide.h"
#include "image.h"
#include "io.h"
#include "mtwister.h"
#include "transform.h"

int main(int argc, char* argv[]) {
    FILE* out = stdout;
    if (argc >= 2) {
        char* out_filename = argv[1];
        out = fopen(out_filename, "a");
    }

    task_def_t* tasks = list_tasks();
    int n_tasks = 0;
    for (task_def_t* task_def = tasks; task_def; task_def = task_def->next) {
        const char* source = read_task(task_def->name);
        if (source) {
            task_def->task = parse_task(source);
            n_tasks++;
        }
    }
    task_def_t* task_array[n_tasks];
    n_tasks = 0;
    for (task_def_t* task_def = tasks; task_def; task_def = task_def->next) {
        if (task_def->task) {
            task_array[n_tasks++] = task_def;
        }
    }

    guide_builder_t builder;
    init_guide(&builder);

    init_image(&builder);
    init_filter(&builder);
    init_binding(&builder);
    init_transform(&builder);
    guide_t* guide = build_guide(&builder);

    MTRand rnd = seedRand(1234l);

    fprintf(out, "task,example,loss,reconstructed,abstraction,filter,transform\n");
    while (true) {
        int i_task = genRandLong(&rnd) % n_tasks;
        task_def_t* task_def = task_array[i_task];
        task_t* task = task_def->task;

        int i_train = genRandLong(&rnd) % task->n_train;
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
        if (output->width == reconstructed->width && output->height == reconstructed->height) {
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

        if (transformed) {
            trail_t* train_trail = new_trail(input, reconstructed, guide);
            train_trail = observe_abstraction(train_trail, abstraction);
            train_trail = observe_filter(train_trail, filter);
            train_trail = observe_transform(train_trail, call);
            float loss = free_trail(guide, train_trail, true);
            fprintf(
                out,
                "%s, %d, %.12e, %d, %s, %s, %s\n",
                task_def->name,
                i_train,
                loss,
                is_correct,
                abstraction->name,
                filter->filter->name,
                call->transform->name);
            fflush(out);
        }

        free_graph(reconstructed);

    no_reconstruction:
        free_transform(task, call);

    no_transform:
        free_item(task->_mem_filter_calls, filter);

    no_filter:
        free_graph(graph);

        free_trail(guide, trail, false);
    }

    return 0;
}