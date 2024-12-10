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

typedef struct _filter {
    bool(*func)(const graph_t *, const node_t *, const filter_arguments_t *);
    bool size;
    bool degree;
    bool exclude;
    bool color;
} filter_t;

filter_t filters[];

#endif // __FILTER_H__