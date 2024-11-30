#ifndef __GRAPH__
#define __GRAPH__

#include <stddef.h>
#include <stdbool.h>

typedef unsigned char color_t;

typedef struct _coordinate {
  unsigned short primary;
  unsigned short secondary;
} coordinate_t;

typedef struct _subnode {
  coordinate_t coord;
  color_t color;
} subnode_t;

typedef struct _node {
  coordinate_t coord;
  unsigned char size;
  unsigned short subnode_last;
} node_t;

typedef enum _direction {
  HORIZONTAL,
  VERTICAL,
  OVERLAPPING
} direction_t;

typedef struct _edge {
  coordinate_t from;
  coordinate_t to;
  direction_t direction;
} edge_t;

typedef struct _graph {
  unsigned short n_nodes;
  unsigned short n_edges;
  unsigned short node_last;
  unsigned short edge_last;
} graph_t;

typedef struct _graph_mem {
  unsigned short n_graphs;
  unsigned short n_nodes;
  unsigned short n_edges;
  unsigned short n_subnodes;
  graph_t * graphs;
  node_t * nodes;
  edge_t * edges;
  subnode_t * subnodes;
} graph_mem_t;

#define likely(x)   __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)

static inline graph_t * new_graph(graph_mem_t * mem) {
  if (unlikely(mem->n_graphs == 0)) {
    return NULL;
  }
  mem->n_graphs--;
  graph_t * graph = &mem->graphs[mem->n_graphs];
  graph->n_nodes = 0;
  graph->n_edges = 0;
  graph->node_last = mem->n_nodes;
  graph->edge_last = mem->n_edges;
  return graph;
}

static inline node_t * add_node(graph_mem_t * mem, graph_t * graph, coordinate_t coord) {
  if (unlikely(mem->n_nodes == 0)) {
    return NULL;
  }
  mem->n_nodes--;
  graph->n_nodes++;
  node_t * node = &mem->nodes[mem->n_nodes];
  node->coord = coord;
  node->size = 0;
  node->subnode_last = mem->n_subnodes;
  return node;
}

static inline const node_t * get_node(const graph_mem_t * mem, const graph_t * graph, int idx) {
  return &mem->nodes[graph->node_last - idx - 1];
}

static inline const subnode_t * get_subnode(const graph_mem_t * mem, const node_t * node, int idx) {
  return &mem->subnodes[node->subnode_last - idx - 1];
}

static inline bool add_subnode(graph_mem_t * mem, node_t * node, coordinate_t coord, color_t color) {
  if (unlikely(mem->n_subnodes == 0)) {
    return false;
  }
  mem->n_subnodes--;
  node->size++;
  subnode_t * subnode = &mem->subnodes[mem->n_subnodes];
  subnode->coord = coord;
  subnode->color = color;
  return true;
}

static inline bool add_edge(graph_mem_t * mem, graph_t * graph, coordinate_t from, coordinate_t to, direction_t dir) {
  if (unlikely(mem->n_edges == 0)) {
    return false;
  }
  mem->n_edges--;
  graph->n_edges++;
  edge_t * edge = &mem->edges[mem->n_edges];
  edge->from = from;
  edge->to = to;
  edge->direction = dir;
  return true;
}

#endif
