#include "graph.h"

graph_t * graph_from_grid(graph_mem_t * mem, const color_t * grid, int n_rows, int n_cols) {
  graph_t * graph = new_graph(mem);
  if (unlikely(graph == NULL)) {
    return NULL;
  }
  for (int row = 0; row < n_rows; row++) {
    for (int col = 0; col < n_cols; col++) {
      color_t color = grid[row * n_cols + col];
      coordinate_t coord = { row, col };
      node_t * node = add_node(mem, graph, coord);
      if (unlikely(node == NULL)) {
        return NULL;
      }
      bool added_subnode = add_subnode(mem, node, coord, color);
      if (unlikely(!added_subnode)) {
        return NULL;
      }
      if (col > 0) {
        coordinate_t left = { row, col - 1 };
        bool added_edge = add_edge(mem, graph, left, coord, HORIZONTAL);
        if (unlikely(!added_edge)) {
          return NULL;
        }
      }
      if (row > 0) {
        coordinate_t top = { row - 1, col };
        bool added_edge = add_edge(mem, graph, top, coord, VERTICAL);
        if (unlikely(!added_edge)) {
          return NULL;
        }
      }
    }
  }
  return graph;
}

graph_t * get_no_abstraction_graph(graph_mem_t * mem, const graph_t * in) {
  graph_t * out = new_graph(mem);
  if (unlikely(out == NULL)) {
    return NULL;
  }
  coordinate_t coord = { 0, 0 };
  node_t * out_node = add_node(mem, out, coord);
  if (unlikely(out_node == NULL)) {
    return NULL;
  }
  for (int idx = 0; idx < in->n_nodes; idx++) {
    const node_t * node = get_node(mem, in, idx);
    const subnode_t * subnode = get_subnode(mem, node, 0);
    bool added_subnode = add_subnode(mem, out_node, node->coord, subnode->color);
    if (unlikely(!added_subnode)) {
      return NULL;
    }
  }
  return out;
}
