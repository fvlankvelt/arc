#ifndef __FILTER_H__
#define __FILTER_H__

#include <stdbool.h>

#include "graph.h"
#include "image.h"
#include "task.h"

typedef struct _filter_arguments {
    int size;
    int degree;
    bool exclude;
    color_t color;
} filter_arguments_t;

typedef struct _filter_func {
    bool (*func)(const graph_t*, const node_t*, const filter_arguments_t*);
    bool size;
    bool degree;
    bool exclude;
    bool color;
    const char* name;
} filter_func_t;

extern filter_func_t filter_funcs[];

typedef struct _filter_call {
    struct _filter_call* next;
    const filter_func_t* filter;
    struct _filter_call* next_in_multi;
    filter_arguments_t args;
} filter_call_t;

bool apply_filter(const graph_t* graph, const node_t* node, const filter_call_t* call);

bool filter_by_size(const graph_t* graph, const node_t* node, const filter_arguments_t* args);
bool filter_by_color(const graph_t* graph, const node_t* node, const filter_arguments_t* args);
bool filter_by_degree(const graph_t* graph, const node_t* node, const filter_arguments_t* args);

filter_call_t* get_candidate_filters(task_t* task, const abstraction_t* abstraction);

void init_filter(guide_t* guide);

filter_call_t* sample_filter(task_t* task, const graph_t* graph, trail_t** p_trail);

trail_t * observe_filter(trail_t* trail, filter_call_t* call);

#endif  // __FILTER_H__