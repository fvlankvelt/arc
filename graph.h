#ifndef __GRAPH__
#define __GRAPH__

#include <assert.h>
#include <stdlib.h>

#define NODE_INDEX_SIZE 1024
#define NODES_ALLOC 1024
#define EDGES_ALLOC 2048
#define SUBNODE_BLOCK_SIZE 11
#define SUBNODE_BLOCKS_ALLOC 256

#define likely(x) __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)

typedef unsigned char color_t;

typedef struct _coordinate {
    unsigned short pri;
    unsigned short sec;
} coordinate_t;

typedef struct _subnode {
    coordinate_t coord;
    color_t color;
} subnode_t;

struct _edge;

// premature optimization: fits on a cacheline (64 bytes)
typedef struct _subnode_block {
    struct _subnode_block *next;
    coordinate_t subnode[SUBNODE_BLOCK_SIZE];
    color_t color[SUBNODE_BLOCK_SIZE];
} subnode_block_t;

typedef struct _node {
    struct _node *next;
    struct _edge *edges;
    struct _subnode_block *subnodes;
    struct _coordinate coord;
    unsigned short n_subnodes;
} node_t;

typedef enum _direction { HORIZONTAL, VERTICAL, OVERLAPPING } direction_t;

// premature optimization: fits with partner on a cacheline (64 bytes)
typedef struct _edge {
    struct _edge *next;
    struct _edge *swap;
    struct _node *node;
    direction_t direction;
} edge_t;

typedef struct _graph {
    // nodes, in (reverse) order of allocation
    // only exception is that nodes with the same index are grouped together
    // - for fast (index) lookups
    node_t *nodes;

    // memory management for mutating the graph
    unsigned short n_nodes;
    unsigned short nodes_available;
    unsigned short edges_available;
    unsigned short blocks_available;
    node_t *free_nodes;
    edge_t *free_edges;
    subnode_block_t *free_blocks;

    // nodes indexed by their node_id - this relies on sibling nodes being adjacent
    node_t *index[NODE_INDEX_SIZE];

    // for freeing / cleanup
    node_t *all_nodes;
    edge_t *all_edges;
    subnode_block_t *all_blocks;
} graph_t;

static inline unsigned int node_id(coordinate_t coord) { return 32 * coord.pri + coord.sec; }

struct _list_entry {
    struct _list_entry *next;
};

#define _init_list(pphead) \
    { *pphead = NULL; }
#define _insert_entry(pphead, pentry) \
    {                                 \
        (pentry)->next = *pphead;     \
        *pphead = pentry;             \
    }
#define _remove_entry(pphead, pentry)                           \
    {                                                           \
        struct _list_entry **p = (struct _list_entry **)pphead; \
        struct _list_entry *e = (struct _list_entry *)pentry;   \
        if ((*p) == e) {                                        \
            *p = e->next;                                       \
        } else {                                                \
            while ((*p)->next != e) {                           \
                p = &((*p)->next);                              \
            }                                                   \
            (*p)->next = e->next;                               \
        }                                                       \
    }

static inline graph_t *new_graph() {
    graph_t *graph = malloc(sizeof(graph_t));

    graph->n_nodes = 0;
    _init_list(&graph->nodes);

    _init_list(&graph->free_nodes);
    graph->nodes_available = NODES_ALLOC;
    graph->all_nodes = malloc(NODES_ALLOC * sizeof(node_t));
    for (int i = 0; i < NODES_ALLOC; i++) {
        _insert_entry(&graph->free_nodes, &graph->all_nodes[i]);
    }

    _init_list(&graph->free_edges);
    graph->edges_available = EDGES_ALLOC;
    graph->all_edges = malloc(EDGES_ALLOC * sizeof(edge_t));
    for (int i = 0; i < EDGES_ALLOC; i++) {
        _insert_entry(&graph->free_edges, &graph->all_edges[i]);
    }

    _init_list(&graph->free_blocks);
    graph->blocks_available = SUBNODE_BLOCKS_ALLOC;
    graph->all_blocks = malloc(SUBNODE_BLOCKS_ALLOC * sizeof(subnode_block_t));
    for (int i = 0; i < SUBNODE_BLOCKS_ALLOC; i++) {
        _insert_entry(&graph->free_blocks, &graph->all_blocks[i]);
    }

    for (int idx = 0; idx < NODE_INDEX_SIZE; idx++) {
        graph->index[idx] = NULL;
    }
    return graph;
}

static inline void free_graph(graph_t *graph) {
    free(graph->all_blocks);
    free(graph->all_edges);
    free(graph->all_nodes);
    free(graph);
}

// iterate over all nodes

static inline const node_t *first_node(const graph_t *graph) { return graph->nodes; }

static inline const node_t *next_node(const node_t *node) { return node->next; }

static inline subnode_t get_subnode(const node_t *node, int idx) {
    assert(idx < node->n_subnodes);

    subnode_block_t *block;
    for (block = node->subnodes; idx >= SUBNODE_BLOCK_SIZE; block = block->next) {
        idx = idx - SUBNODE_BLOCK_SIZE;
    }
    subnode_t subnode;
    subnode.coord = block->subnode[idx];
    subnode.color = block->color[idx];
    return subnode;
}

static inline void set_subnode(const node_t *node, int idx, subnode_t subnode) {
    assert(idx < node->n_subnodes);

    subnode_block_t *block;
    for (block = node->subnodes; idx >= SUBNODE_BLOCK_SIZE; block = block->next) {
        idx = idx - SUBNODE_BLOCK_SIZE;
    }
    block->subnode[idx] = subnode.coord;
    block->color[idx] = subnode.color;
}

// lookup

static inline node_t *get_node(graph_t *graph, coordinate_t coord) {
    unsigned int idx = node_id(coord) % NODE_INDEX_SIZE;
    node_t *node = graph->index[idx];
    while (node && idx == node_id(node->coord) % NODE_INDEX_SIZE) {
        if (node->coord.pri == coord.pri && node->coord.sec == coord.sec) {
            return node;
        }
        node = node->next;
    }
    return NULL;
}

// mutate

static inline node_t *add_node(graph_t *graph, coordinate_t coord, int n_subnodes) {
    int n_blocks = (n_subnodes + SUBNODE_BLOCK_SIZE - 1) / SUBNODE_BLOCK_SIZE;
    if (unlikely(graph->nodes_available == 0 || graph->blocks_available < n_blocks)) {
        return NULL;
    }

    node_t *node = graph->free_nodes;
    graph->nodes_available--;
    graph->n_nodes++;

    _remove_entry(&graph->free_nodes, node);

    unsigned int idx = node_id(coord) % NODE_INDEX_SIZE;
    node_t *sibling = graph->index[idx];
    if (sibling) {
        node->next = sibling->next;
        sibling->next = node;
    } else {
        graph->index[idx] = node;
        _insert_entry(&graph->nodes, node);
    }

    graph->blocks_available -= n_blocks;
    node->subnodes = graph->free_blocks;
    while (n_blocks-- > 0) {
        graph->free_blocks = graph->free_blocks->next;
    }
    node->n_subnodes = n_subnodes;

    node->coord = coord;
    _init_list(&node->edges);
    return node;
}

static inline void remove_edge(graph_t *graph, edge_t *edge) {
    edge_t *other = edge->swap;
    _remove_entry(&edge->node->edges, edge);
    _remove_entry(&other->node->edges, other);

    graph->edges_available += 2;
    other->next = edge;
    edge->next = graph->free_edges;
    graph->free_edges = other;
}

static inline void remove_node(graph_t *graph, node_t *node) {
    unsigned int idx = node_id(node->coord) % NODE_INDEX_SIZE;
    node_t *sibling = graph->index[idx];
    if (sibling == node) {
        node_t *next = node->next;
        if (next && idx == node_id(next->coord) % NODE_INDEX_SIZE) {
            graph->index[idx] = next;
        } else {
            graph->index[idx] = NULL;
        }
    }

    while (node->edges) {
        remove_edge(graph, node->edges);
    }

    _remove_entry(&graph->nodes, node);
    _insert_entry(&graph->free_nodes, node);
    graph->nodes_available++;
    graph->n_nodes--;

    int n_blocks = (node->n_subnodes + SUBNODE_BLOCK_SIZE - 1) / SUBNODE_BLOCK_SIZE;
    if (unlikely(n_blocks == 0)) {
        return;
    }
    graph->blocks_available += n_blocks;
    subnode_block_t *block = node->subnodes;
    while (n_blocks-- > 1) {
        block = block->next;
    }
    block->next = graph->free_blocks;
    graph->free_blocks = node->subnodes;
}

static inline edge_t *add_edge(graph_t *graph, node_t *from, node_t *to,
                               direction_t direction) {
    if (unlikely(graph->edges_available < 2)) {
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

#endif  // __GRAPH__
