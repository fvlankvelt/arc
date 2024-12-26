#ifndef __BINDING_H__
#define __BINDING_H__

#include "graph.h"

typedef struct _binding_arguments {
    int size;
    int degree;
    bool exclude;
    color_t color;
} binding_arguments_t;

typedef struct _binding_func {
    node_t * (*func)(const graph_t *, const node_t *, const binding_arguments_t *);
    bool size;
    bool degree;
    bool exclude;
    bool color;
} binding_func_t;

extern binding_func_t binding_funcs[];

typedef struct _binding_call {
    struct _binding_call * next;
    const binding_func_t * binding;
    binding_arguments_t args;
} binding_call_t;

node_t * get_binding_node(const graph_t * graph, const node_t * node, const binding_call_t * call);

#endif // __BINDING_H__