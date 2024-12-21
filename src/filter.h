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

typedef struct _binding_arguments {
    node_t * node;
    int size;
    int degree;
    bool exclude;
    color_t color;
} binding_arguments_t;

typedef struct _binding_func {
    node_t * (*func)(const graph_t *, const binding_arguments_t *);
    bool node;
    bool size;
    bool degree;
    bool exclude;
    bool color;
} binding_func_t;

extern binding_func_t binding_funcs[];

typedef struct _binding_call {
    binding_func_t * binding;
    binding_arguments_t args;
} binding_call_t;

#endif // __FILTER_H__