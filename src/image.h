#ifndef __IMAGE__
#define __IMAGE__

#include "graph.h"
#include "guide.h"

graph_t* new_grid(const color_t bg_color, int n_rows, int n_cols);
graph_t* graph_from_grid(const color_t* grid, int n_rows, int n_cols);
graph_t* subgraph_by_color(const graph_t* in, color_t color);
void print_graph(const graph_t* graph);

graph_t* get_no_abstraction_graph(const graph_t* in);
graph_t* get_connected_components_graph(const graph_t* in);

graph_t* undo_abstraction(const graph_t* in);

typedef struct _abstraction {
    graph_t* (*func)(const graph_t* in);
    char* name;
} abstraction_t;

extern abstraction_t abstractions[];

void init_image(guide_t* guide);

abstraction_t* sample_abstraction(trail_t** trail);

trail_t* observe_abstraction(trail_t* trail, abstraction_t* abstraction);

#endif
