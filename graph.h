#ifndef __GRAPH__
#define __GRAPH__

#include <stdlib.h>
#include <assert.h>

#define NODE_INDEX_SIZE 1024
#define NODES_ALLOC 1024
#define EDGES_ALLOC 2048
#define SUBNODE_BLOCK_SIZE 16
#define SUBNODE_BLOCKS_ALLOC 256

#define likely(x) __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)

typedef unsigned char color_t;

typedef struct _coordinate
{
  unsigned short pri;
  unsigned short sec;
} coordinate_t;

typedef struct _subnode
{
  struct _coordinate coord;
  color_t color;
} subnode_t;

struct _edge;

typedef struct _list_entry
{
  struct _list_entry *next;
} list_entry_t;

typedef struct _subnode_block
{
  struct _subnode_block *next;
  subnode_t subnode[SUBNODE_BLOCK_SIZE];
} subnode_block_t;

typedef struct _node
{
  struct _node *next;
  struct _edge *edges;
  struct _subnode_block *subnodes;
  struct _coordinate coord;
  unsigned short n_subnodes;
} node_t;

typedef enum _direction
{
  HORIZONTAL,
  VERTICAL,
  OVERLAPPING
} direction_t;

typedef struct _edge
{
  struct _edge *next;
  struct _edge *swap;
  struct _node *node;
  direction_t direction;
} edge_t;

typedef struct _graph
{
  node_t *nodes;

  // memory management
  unsigned short n_nodes;
  unsigned short nodes_available;
  unsigned short edges_available;
  unsigned short blocks_available;
  node_t *free_nodes;
  edge_t *free_edges;
  subnode_block_t *free_blocks;

  node_t *index[NODE_INDEX_SIZE];
} graph_t;

static inline unsigned int node_id(coordinate_t coord)
{
  return 32 * coord.pri + coord.sec;
}

static inline void _init_node_list(node_t **head)
{
  *head = NULL;
}

static inline void _insert_node(node_t **head, node_t *entry)
{
  (entry)->next = *head;
  *head = entry;
}

static inline void _remove_node(node_t **head, node_t *entry)
{
  node_t **p = head;
  if (*p == entry) {
    *p = entry->next;
    return;
  }
  while ((*p)->next != entry)
  {
    p = &((*p)->next);
  }
  (*p)->next = entry->next;
}

static inline void _init_block_list(subnode_block_t **head)
{
  *head = NULL;
}

static inline void _insert_block(subnode_block_t **head, subnode_block_t *entry)
{
  (entry)->next = *head;
  *head = entry;
}

static inline void _remove_block(subnode_block_t **head, subnode_block_t *entry)
{
  subnode_block_t **p = head;
  while ((*p)->next != entry)
  {
    p = &((*p)->next);
  }
  (*p)->next = entry->next;
}

static inline void _init_edge_list(edge_t **head)
{
  *head = NULL;
}

static inline void _insert_edge(edge_t **head, edge_t *entry)
{
  (entry)->next = *head;
  *head = entry;
}

static inline void _remove_edge(edge_t **head, edge_t *entry)
{
  edge_t **p = head;
  while ((*p)->next != entry)
  {
    p = &((*p)->next);
  }
  (*p)->next = entry->next;
}

static inline graph_t *new_graph()
{
  graph_t *graph = malloc(sizeof(graph_t));

  graph->n_nodes = 0;
  _init_node_list(&graph->nodes);

  _init_node_list(&graph->free_nodes);
  graph->nodes_available = NODES_ALLOC;
  node_t *nodes = malloc(NODES_ALLOC * sizeof(node_t));
  for (int i = 0; i < NODES_ALLOC; i++)
  {
    _insert_node(&graph->free_nodes, &nodes[i]);
  }

  _init_edge_list(&graph->free_edges);
  graph->edges_available = EDGES_ALLOC;
  edge_t *edges = malloc(EDGES_ALLOC * sizeof(edge_t));
  for (int i = 0; i < EDGES_ALLOC; i++)
  {
    _insert_edge(&graph->free_edges, &edges[i]);
  }

  _init_block_list(&graph->free_blocks);
  graph->blocks_available = SUBNODE_BLOCKS_ALLOC;
  subnode_block_t *blocks = malloc(SUBNODE_BLOCKS_ALLOC * sizeof(subnode_block_t));
  for (int i = 0; i < SUBNODE_BLOCKS_ALLOC; i++)
  {
    _insert_block(&graph->free_blocks, &blocks[i]);
  }

  for (int idx = 0; idx < NODE_INDEX_SIZE; idx++)
  {
    graph->index[idx] = NULL;
  }
  return graph;
}

// iterate over all nodes

static inline const node_t *first_node(const graph_t *graph)
{
  return graph->nodes;
}

static inline const node_t *next_node(const node_t *node)
{
  return node->next;
}

static inline subnode_t *get_subnode(const node_t *node, int idx)
{
  assert(idx < node->n_subnodes);

  subnode_block_t *block;
  for (block = node->subnodes; idx >= SUBNODE_BLOCK_SIZE; block = block->next)
  {
    idx = idx - SUBNODE_BLOCK_SIZE;
  }
  return &block->subnode[idx];
}

// lookup

static inline node_t *get_node(graph_t *graph, coordinate_t coord)
{
  unsigned int idx = node_id(coord) % NODE_INDEX_SIZE;
  return graph->index[idx];
}

// mutate

static inline node_t *add_node(graph_t *graph, coordinate_t coord, int n_subnodes)
{
  int n_blocks = (n_subnodes + SUBNODE_BLOCK_SIZE - 1) / SUBNODE_BLOCK_SIZE;
  if (unlikely(graph->nodes_available == 0 || graph->blocks_available < n_blocks))
  {
    return NULL;
  }

  node_t *node = graph->free_nodes;
  graph->nodes_available--;
  graph->n_nodes++;

  _remove_node(&graph->free_nodes, node);

  unsigned int idx = node_id(coord) % NODE_INDEX_SIZE;
  node_t * sibling = graph->index[idx];
  if (sibling) {
    node->next = sibling->next;
    sibling->next = node;
  } else {
    graph->index[idx] = node;
    _insert_node(&graph->nodes, node);
  }

  graph->blocks_available -= n_blocks;
  node->subnodes = graph->free_blocks;
  while (n_blocks-- > 0)
  {
    graph->free_blocks = graph->free_blocks->next;
  }
  node->n_subnodes = n_subnodes;

  _init_edge_list(&node->edges);
  return node;
}

static inline void remove_node(graph_t *graph, node_t *node)
{
  unsigned int idx = node_id(node->coord) % NODE_INDEX_SIZE;
  node_t * sibling = graph->index[idx];
  if (sibling == node) {
    node_t * next = node->next;
    if (next && idx == node_id(next->coord) % NODE_INDEX_SIZE) {
      graph->index[idx] = next;
    } else {
      graph->index[idx] = NULL;
    }
  }

  _remove_node(&graph->nodes, node);
  _insert_node(&graph->free_nodes, node);
  graph->nodes_available++;
  graph->n_nodes--;

  int n_blocks = (node->n_subnodes + SUBNODE_BLOCK_SIZE - 1) / SUBNODE_BLOCK_SIZE;
  if (unlikely(n_blocks == 0))
  {
    return;
  }
  graph->blocks_available += n_blocks;
  subnode_block_t *block = node->subnodes;
  while (n_blocks-- > 1)
  {
    block = block->next;
  }
  block->next = graph->free_blocks;
  graph->free_blocks = node->subnodes;
}

static inline edge_t *add_edge(graph_t *graph, node_t *from, node_t *to, direction_t direction)
{
  if (unlikely(graph->edges_available < 2))
  {
    return NULL;
  }

  graph->edges_available -= 2;
  edge_t *from_to = graph->free_edges;
  edge_t *to_from = from_to->next;
  graph->free_edges = to_from->next;

  from_to->next = from->edges;
  from_to->swap = to_from;
  from_to->node = from;
  from_to->direction = direction;
  from->edges = from_to;

  to_from->next = to->edges;
  to_from->swap = from_to;
  to_from->node = to;
  to_from->direction = direction;
  to->edges = to_from;

  return from_to;
}

static inline void remove_edge(graph_t *graph, edge_t *edge)
{
  edge_t *other = edge->swap;
  _remove_edge(&edge->node->edges, edge);
  _remove_edge(&other->node->edges, other);

  graph->edges_available += 2;
  other->next = edge;
  edge->next = graph->free_edges;
  graph->free_edges = other;
}

#endif // __GRAPH__
