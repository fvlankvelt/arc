#ifndef __BINDING_H__
#define __BINDING_H__

#include "graph.h"
#include "transform.h"

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

typedef struct _transform_binding {
    transform_argument_flags_t constant_flags;
    transform_arguments_t constant_values;
    binding_call_t color_call;
    binding_call_t direction_call;
    binding_call_t point_call;
    binding_call_t mirror_axis;
    binding_call_t mirror_direction;
} transform_binding_t;

void apply_binding(const graph_t * graph, const node_t * node, const transform_binding_t * binding, transform_arguments_t *args);

#endif // __BINDING_H__