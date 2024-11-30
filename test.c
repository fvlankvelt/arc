#include <stdlib.h>
#include <stdio.h>

#include "graph.h"
#include "image.h"

#define ASSERT(stmt, msg) if (!(stmt)) { printf(msg); printf("\n"); return false; }

void initialize_mem(graph_mem_t * mem, int n_graphs, int n_nodes, int n_edges, int n_subnodes) {
  mem->n_graphs = n_graphs;
  mem->n_nodes = n_nodes;
  mem->n_edges = n_edges;
  mem->n_subnodes = n_subnodes;
  mem->graphs = malloc(n_graphs * sizeof(graph_t));
  mem->nodes = malloc(n_nodes * sizeof(node_t));
  mem->edges = malloc(n_edges * sizeof(edge_t));
  mem->subnodes = malloc(n_subnodes * sizeof(subnode_t));
}

void free_mem(graph_mem_t * mem) {
  free(mem->graphs);
  free(mem->nodes);
  free(mem->edges);
  free(mem->subnodes);
}

bool test_image() {
  graph_mem_t mem;
  initialize_mem(&mem, 10, 100, 100, 100);
  color_t grid[] = { 2, 2, 1, 1 };
  graph_t * graph = graph_from_grid(&mem, grid, 2, 2);
  ASSERT(graph->n_nodes == 4, "n_nodes incorrect");
  ASSERT(graph->n_edges == 4, "n_edges incorrect");
  free_mem(&mem);
  return true;
}

int main() {
  bool result = true;

  result &= test_image();

  if (result) {
    printf("PASS\n");
    return 0;
  } else {
    printf("FAIL\n");
    return 1;
  }
}
