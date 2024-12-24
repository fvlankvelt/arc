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

typedef enum _rotation {
    CLOCK_WISE = 0,
    COUNTER_CLOCK_WISE,
    DOUBLE_CLOCK_WISE
} rotation_t;

typedef enum _mirror {
    MIRROR_VERTICAL,
    MIRROR_HORIZONTAL,
    MIRROR_DIAGONAL_LEFT,
    MIRROR_DIAGONAL_RIGHT
} mirror_t;

typedef enum _image_points {
    IMAGE_TOP,
    IMAGE_BOTTOM,
    IMAGE_LEFT,
    IMAGE_RIGHT,
    IMAGE_TOP_LEFT,
    IMAGE_TOP_RIGHT,
    IMAGE_BOTTOM_LEFT,
    IMAGE_BOTTOM_RIGHT,
} image_points_t;

typedef enum _relative_position {
    SOURCE,
    TARGET,
    MIDDLE
} relative_position_t;

typedef enum _object_property {
    SYMMETRICAL,
    HOLLOW
} object_property_t;

typedef struct _mirror_axis {
    bool orientation;
    short distance; 
} mirror_axis_t;

typedef struct _transform_argument_flags {
    bool color;
    bool direction;
    bool overlap;
    bool rotation_dir;
    bool mirror_axis;
    bool mirror_direction;
    bool object_id;
    bool point;
    bool relative_pos;
} transform_argument_flags_t;

typedef struct _transform_arguments {
    color_t color;
    direction_t direction;
    bool overlap;
    rotation_t rotation_dir;
    mirror_axis_t mirror_axis;
    mirror_t mirror_direction;
    short object_id;
    coordinate_t point;
    relative_position_t relative_pos;
} transform_arguments_t;

typedef struct _transform_func {
    void (*func)(graph_t*, node_t*, transform_arguments_t*);
    transform_argument_flags_t flags;
} transform_func_t;

extern transform_func_t transformations[];

typedef struct _transform_call {
    struct _transform_call * next;
    const transform_func_t * filter;
    transform_arguments_t args;
} transform_call_t;

void apply_transformation(const graph_t * graph, const node_t * node, const transform_call_t * call);

#endif // __TRANSFORM_H__