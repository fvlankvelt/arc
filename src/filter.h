#ifndef __FILTER_H__
#define __FILTER_H__

#include <stdbool.h>
#include "graph.h"

typedef struct _filter_arguments {
    int size;
    int degree;
    bool exclude;
    color_t color;
} filter_arguments_t;

typedef struct _filter_func {
    bool(*func)(const graph_t *, const node_t *, const filter_arguments_t *);
    bool size;
    bool degree;
    bool exclude;
    bool color;
} filter_func_t;

extern filter_func_t filter_funcs[];

typedef struct _filter_call {
    struct _filter_call * next;
    const filter_func_t * filter;
    filter_arguments_t args;
} filter_call_t;

bool apply_filter(const graph_t * graph, const node_t * node, const filter_call_t * call);

bool filter_by_size(const graph_t* graph, const node_t* node, const filter_arguments_t* args);
bool filter_by_color(const graph_t* graph, const node_t* node, const filter_arguments_t* args);
bool filter_by_degree(const graph_t* graph, const node_t* node, const filter_arguments_t* args);

#endif // __FILTER_H__