#ifndef __BINDING_H__
#define __BINDING_H__

#include "graph.h"
#include "guide.h"
#include "filter.h"
#include "task.h"

typedef struct _binding_arguments {
    int size;
    int degree;
    bool exclude;
    color_t color;
} binding_arguments_t;

typedef struct _binding_func {
    node_t* (*func)(const graph_t*, const node_t*, const binding_arguments_t*);
    bool size;
    bool degree;
    bool exclude;
    bool color;
} binding_func_t;

extern binding_func_t binding_funcs[];

typedef struct _binding_call {
    struct _binding_call* next;
    const binding_func_t* binding;
    binding_arguments_t args;
} binding_call_t;

node_t* get_binding_node(const graph_t* graph, const node_t* node, const binding_call_t* call);

void init_binding(guide_builder_t* guide);

void add_binding(guide_builder_t* guide, const char* prefix);

// sample a binding, or NULL when sample didn't match the graph/filter
binding_call_t* sample_binding(
    task_t* task, const graph_t* graph, const filter_call_t* filter, trail_t** p_trail);

// only to be used in pure training (i.e. not sampling)
// when binding is skipped, use NULL for the call
trail_t* observe_binding(trail_t* trail, const binding_call_t* call);

#endif  // __BINDING_H__