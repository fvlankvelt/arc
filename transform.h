#ifndef __TRANSFORM_H__
#define __TRANSFORM_H__

#include "graph.h"

typedef enum _direction {
    UP,
    DOWN,
    LEFT,
    RIGHT,
    UP_LEFT,
    DOWN_LEFT,
    UP_RIGHT,
    DOWN_RIGHT
} direction_t;

typedef struct _transform_params {
    color_t color;
    direction_t direction;
    bool overlap;
} transform_params_t;

typedef struct _transformation {
    void (*func)(graph_t*, node_t*, transform_params_t*);
    bool color;
    bool direction;
    bool overlap;
} transformation_t;

// for testing
void update_color(graph_t * graph, node_t * node, transform_params_t * params);
void move_node(graph_t * graph, node_t * node, transform_params_t * params);
void extend_node(graph_t * graph, node_t * node, transform_params_t * params);

static transformation_t transformations[] = {
    {
        .func = update_color,
        .color = true,
    },
    {
        .func = move_node,
        .direction = true,
    },
    {
        .func = extend_node,
        .direction = true,
        .overlap = true,
    }
};

#endif // __TRANSFORM_H__