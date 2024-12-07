#ifndef __IMAGE__
#define __IMAGE__

#include "graph.h"

graph_t * new_grid(const color_t bg_color, int n_rows, int n_cols);
graph_t * graph_from_grid(const color_t * grid, int n_rows, int n_cols);
graph_t * get_no_abstraction_graph(const graph_t * in);

#endif
