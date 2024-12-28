#ifndef __GRAPH__
#define __GRAPH__

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>

#define NODE_INDEX_SIZE 1024
#define NODES_ALLOC 1024
#define EDGES_ALLOC 4096
#define SUBNODE_BLOCK_SIZE 11
#define SUBNODE_BLOCKS_ALLOC 1024

#define likely(x) __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)

typedef char color_t;
#define BACKGROUND_COLOR ((color_t) - 1)
#define MOST_COMMON_COLOR ((color_t) - 2)
#define LEAST_COMMON_COLOR ((color_t) - 3)

#define MAX_SIZE -1
#define MIN_SIZE -2
#define ODD_SIZE -3

typedef struct _derived_props {
    color_t background_color;
    color_t most_common_color;
    color_t least_common_color;
    unsigned short min_size;
    unsigned short max_size;
} derived_props_t;

typedef struct _coordinate {
    short pri;
    short sec;
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
    unsigned short n_edges;
} node_t;

typedef enum _edge_direction {
    EDGE_HORIZONTAL,
    EDGE_VERTICAL,
    EDGE_OVERLAPPING
} edge_direction_t;

// premature optimization: fits with partner on a cacheline (64 bytes)
typedef struct _edge {
    struct _edge *next;
    struct _edge *swap;
    struct _node *peer;
    edge_direction_t direction;
} edge_t;

typedef struct _graph {
    // nodes, in (reverse) order of allocation
    // only exception is that nodes with the same index are grouped together
    // - for fast (index) lookups
    node_t *nodes;

    // subnodes of a node can have different colors
    bool is_multicolor;

    // derived properties
    bool _has_changed;
    derived_props_t _derived;

    // dimensions
    unsigned short width;
    unsigned short height;

    // memory management for mutating the graph
    unsigned short n_nodes;
    unsigned short _nodes_available;
    unsigned short _edges_available;
    unsigned short _blocks_available;
    node_t *_free_nodes;
    edge_t *_free_edges;
    subnode_block_t *_free_blocks;

    // nodes indexed by their node_id - this relies on sibling nodes being adjacent
    node_t *_index[NODE_INDEX_SIZE];

    // for freeing / cleanup
    node_t *_all_nodes;
    edge_t *_all_edges;
    subnode_block_t *_all_blocks;
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

static inline graph_t *new_graph(unsigned short width, unsigned short height) {
    graph_t *graph = malloc(sizeof(graph_t));

    graph->width = width;
    graph->height = height;

    graph->n_nodes = 0;
    _init_list(&graph->nodes);

    _init_list(&graph->_free_nodes);
    graph->_nodes_available = NODES_ALLOC;
    graph->_all_nodes = malloc(NODES_ALLOC * sizeof(node_t));
    for (int i = 0; i < NODES_ALLOC; i++) {
        _insert_entry(&graph->_free_nodes, &graph->_all_nodes[i]);
    }

    _init_list(&graph->_free_edges);
    graph->_edges_available = EDGES_ALLOC;
    graph->_all_edges = malloc(EDGES_ALLOC * sizeof(edge_t));
    for (int i = 0; i < EDGES_ALLOC; i++) {
        _insert_entry(&graph->_free_edges, &graph->_all_edges[i]);
    }

    _init_list(&graph->_free_blocks);
    graph->_blocks_available = SUBNODE_BLOCKS_ALLOC;
    graph->_all_blocks = malloc(SUBNODE_BLOCKS_ALLOC * sizeof(subnode_block_t));
    for (int i = 0; i < SUBNODE_BLOCKS_ALLOC; i++) {
        _insert_entry(&graph->_free_blocks, &graph->_all_blocks[i]);
    }

    for (int idx = 0; idx < NODE_INDEX_SIZE; idx++) {
        graph->_index[idx] = NULL;
    }
    return graph;
}

static inline void free_graph(graph_t *graph) {
    free(graph->_all_blocks);
    free(graph->_all_edges);
    free(graph->_all_nodes);
    free(graph);
}

static inline subnode_block_t *new_subnode_block(graph_t *graph) {
    if (graph->_blocks_available == 0) {
        return NULL;
    }
    subnode_block_t *block = graph->_free_blocks;
    graph->_free_blocks = block->next;
    block->next = NULL;
    graph->_blocks_available--;
    return block;
}

static inline bool add_subnode_to_block(graph_t *graph, subnode_block_t *block,
                                        subnode_t subnode, int *n_subnodes) {
    subnode_block_t *last_block = block;
    while (last_block->next) {
        last_block = last_block->next;
    }
    if (*n_subnodes > 0 && (*n_subnodes % SUBNODE_BLOCK_SIZE) == 0) {
        last_block->next = new_subnode_block(graph);
        if (unlikely(!last_block->next)) {
            return false;
        }
        last_block = last_block->next;
    }
    int idx = (*n_subnodes) % SUBNODE_BLOCK_SIZE;
    last_block->subnode[idx] = subnode.coord;
    last_block->color[idx] = subnode.color;
    (*n_subnodes)++;
    return true;
}

static inline void free_subnode_block(graph_t *graph, subnode_block_t *block) {
    subnode_block_t *last_block = block;
    int n_blocks = 1;
    while (last_block->next) {
        last_block = last_block->next;
        n_blocks++;
    }
    last_block->next = graph->_free_blocks;
    graph->_free_blocks = block;
    graph->_blocks_available += n_blocks;
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

static inline void set_subnodes(graph_t *graph, node_t *node, subnode_block_t *block,
                                int n_subnodes) {
    free_subnode_block(graph, node->subnodes);
    node->subnodes = block;
    node->n_subnodes = n_subnodes;
}

// lookup

static inline node_t *get_node(const graph_t *graph, coordinate_t coord) {
    unsigned int idx = node_id(coord) % NODE_INDEX_SIZE;
    node_t *node = graph->_index[idx];
    while (node && idx == node_id(node->coord) % NODE_INDEX_SIZE) {
        if (node->coord.pri == coord.pri && node->coord.sec == coord.sec) {
            return node;
        }
        node = node->next;
    }
    return NULL;
}

static inline derived_props_t get_derived_properties(const graph_t *graph) {
    derived_props_t *props = (derived_props_t *)&graph->_derived;
    if (graph->_has_changed) {
        int counts[10] = {0};
        int min_size = -1, max_size = -1;
        for (const node_t *node = graph->nodes; node; node = node->next) {
            if (min_size < 0 || min_size > node->n_subnodes) {
                min_size = node->n_subnodes;
            }
            if (max_size < 0 || max_size < node->n_subnodes) {
                max_size = node->n_subnodes;
            }
            for (int i = 0; i < node->n_subnodes; i++) {
                subnode_t subnode = get_subnode(node, i);
                counts[subnode.color]++;
            }
        }
        props->max_size = max_size;
        props->min_size = min_size;

        int max = 0, min = 0;
        int n_max = counts[0], n_min = counts[0];
        for (int i = 1; i < 10; i++) {
            if (counts[i] > 0) {
                if (n_max <= counts[i]) {
                    max = i;
                    n_max = counts[i];
                }
                if (n_min >= counts[i]) {
                    min = i;
                    n_min = counts[i];
                }
            }
        }
        if (counts[0] > 0) {
            props->background_color = 0;
        } else {
            props->background_color = max;
        }
        props->most_common_color = max;
        props->least_common_color = min;
        ((graph_t *)graph)->_has_changed = false;
    }
    return graph->_derived;
}

// mutate

static inline node_t *add_node(graph_t *graph, coordinate_t coord, int n_subnodes) {
    int n_blocks = (n_subnodes + SUBNODE_BLOCK_SIZE - 1) / SUBNODE_BLOCK_SIZE;
    if (unlikely(graph->_nodes_available == 0 || graph->_blocks_available < n_blocks)) {
        return NULL;
    }

    node_t *node = graph->_free_nodes;
    graph->_nodes_available--;
    graph->n_nodes++;
    graph->_has_changed = true;

    _remove_entry(&graph->_free_nodes, node);

    unsigned int idx = node_id(coord) % NODE_INDEX_SIZE;
    node_t *sibling = graph->_index[idx];
    if (sibling) {
        node->next = sibling->next;
        sibling->next = node;
    } else {
        graph->_index[idx] = node;
        _insert_entry(&graph->nodes, node);
    }

    graph->_blocks_available -= n_blocks;
    node->subnodes = graph->_free_blocks;
    while (n_blocks-- > 0) {
        graph->_free_blocks = graph->_free_blocks->next;
    }
    node->n_subnodes = n_subnodes;
    node->n_edges = 0;

    node->coord = coord;
    _init_list(&node->edges);
    return node;
}

static inline void remove_edge(graph_t *graph, edge_t *edge) {
    edge_t *other = edge->swap;
    _remove_entry(&edge->peer->edges, other);
    _remove_entry(&other->peer->edges, edge);
    edge->peer->n_edges--;
    other->peer->n_edges--;

    graph->_edges_available += 2;
    other->next = edge;
    edge->next = graph->_free_edges;
    graph->_free_edges = other;
}

static inline void remove_node(graph_t *graph, node_t *node) {
    unsigned int idx = node_id(node->coord) % NODE_INDEX_SIZE;
    node_t *sibling = graph->_index[idx];
    if (sibling == node) {
        node_t *next = node->next;
        if (next && idx == node_id(next->coord) % NODE_INDEX_SIZE) {
            graph->_index[idx] = next;
        } else {
            graph->_index[idx] = NULL;
        }
    }

    while (node->edges) {
        remove_edge(graph, node->edges);
    }

    _remove_entry(&graph->nodes, node);
    _insert_entry(&graph->_free_nodes, node);
    graph->_nodes_available++;
    graph->n_nodes--;
    graph->_has_changed = true;

    int n_blocks = (node->n_subnodes + SUBNODE_BLOCK_SIZE - 1) / SUBNODE_BLOCK_SIZE;
    if (unlikely(n_blocks == 0)) {
        return;
    }
    graph->_blocks_available += n_blocks;
    subnode_block_t *block = node->subnodes;
    while (n_blocks-- > 1) {
        block = block->next;
    }
    block->next = graph->_free_blocks;
    graph->_free_blocks = node->subnodes;
}

static inline edge_t *add_edge(graph_t *graph, node_t *from, node_t *to,
                               edge_direction_t direction) {
    if (unlikely(graph->_edges_available < 2)) {
        return NULL;
    }

    graph->_edges_available -= 2;
    edge_t *from_to = graph->_free_edges;
    edge_t *to_from = from_to->next;
    graph->_free_edges = to_from->next;

    from_to->next = from->edges;
    from_to->swap = to_from;
    from_to->peer = to;
    from_to->direction = direction;
    from->edges = from_to;
    from->n_edges++;

    to_from->next = to->edges;
    to_from->swap = from_to;
    to_from->peer = from;
    to_from->direction = direction;
    to->edges = to_from;
    to->n_edges++;

    return from_to;
}

static inline edge_t *has_edge(node_t *from, node_t *to) {
    for (edge_t *edge = from->edges; edge; edge = edge->next) {
        if (edge->peer == to) {
            return edge;
        }
    }
    return NULL;
}

#endif  // __GRAPH__
