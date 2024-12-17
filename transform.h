#ifndef __TRANSFORM_H__
#define __TRANSFORM_H__

#include "graph.h"
#include "filter.h"

typedef enum _direction {
    NO_DIRECTION = -1,
    UP = 0,
    DOWN,
    LEFT,
    RIGHT,
    UP_LEFT,
    DOWN_LEFT,
    UP_RIGHT,
    DOWN_RIGHT,
} direction_t;

typedef struct _transform_arguments {
    color_t color;
    direction_t direction;
    bool overlap;
} transform_arguments_t;

typedef struct _transform_func {
    void (*func)(graph_t*, node_t*, transform_arguments_t*);
    bool color;
    bool direction;
    bool overlap;
} transform_func_t;

extern transform_func_t transformations[];

typedef enum _argument {
    COLOR,
    DIRECTION,
    OVERLAP
} argument_t;

typedef struct _transform_arg_bindings {
    bool constant_color;
    bool constant_direction;
    bool constant_overlap;
    transform_arguments_t constant;
    binding_call_t color_call;
    binding_call_t direction_call;
    binding_call_t overlap_call;
} transform_arg_bindings_t;

void apply_binding(const graph_t * graph, const node_t * node, const transform_arg_bindings_t * binding, transform_arguments_t *args);

#endif // __TRANSFORM_H__