#ifndef __COLLECTION_H__
#define __COLLECTION_H__

#include <stdbool.h>
#include "graph.h"

typedef struct _node_set {
    unsigned int size;
    unsigned int available;
    const node_t **nodes;
} node_set_t;

static inline unsigned int _node_set_id(const node_set_t *set, const node_t *node) {
    return (unsigned int)(137 * node->coord.pri + node->coord.sec) % set->size;
}

static inline void clear_node_set(node_set_t *set) {
    for (int idx = 0; idx < set->size; idx++) {
        set->nodes[idx] = 0;
    }
}

static inline node_set_t *new_node_set(unsigned int size) {
    node_set_t *set = malloc(sizeof(node_set_t) + size * sizeof(node_t *));
    set->available = size;
    set->size = size;
    set->nodes = (const node_t **)((void *)set + sizeof(node_set_t));
    clear_node_set(set);
    return set;
}

static inline void free_node_set(node_set_t *set) { free(set); }

static inline bool is_node_in_set(node_set_t *set, const node_t *node) {
    unsigned int idx = _node_set_id(set, node);
    for (unsigned int rel = 0; rel < set->size; rel++) {
        unsigned int full_idx = (idx + rel) % set->size;
        if (!set->nodes[full_idx]) {
            break;
        } else {
            coordinate_t coord = set->nodes[full_idx]->coord;
            if (coord.pri == node->coord.pri && coord.sec == node->coord.sec) {
                return true;
            }
        }
    }
    return false;
}

static inline void add_node_to_set(node_set_t *set, const node_t *node) {
    unsigned int idx = _node_set_id(set, node);
    assert(set->available > 0);
    for (unsigned int rel = 0; rel < set->size; rel++) {
        unsigned int full_idx = (idx + rel) % set->size;
        if (!set->nodes[full_idx]) {
            set->nodes[full_idx] = node;
            return;
        }
    }
    assert(false);
}

#define NODE_LIST_BLOCK_SIZE 14

typedef struct _list_node {
    struct _list_node *next;
    int used;
    const node_t *entries[NODE_LIST_BLOCK_SIZE];
} list_node_t;

typedef struct _list_iter {
    list_node_t *list;
    const node_t *node;
    int index;
} list_iter_t;

static inline list_node_t *new_node_list() {
    list_node_t *list = malloc(sizeof(list_node_t));
    list->next = NULL;
    list->used = 0;
    for (int i = 0; i < NODE_LIST_BLOCK_SIZE; i++) {
        list->entries[i] = 0;
    }
    return list;
}

static inline void free_node_list(list_node_t *list) {
    if (list->next) {
        free_node_list(list->next);
    }
    free(list);
}

static inline void add_node_to_list(list_node_t *list, const node_t *node) {
    list_node_t *current_list = list;
    while (current_list->next) {
        current_list = current_list->next;
    }
    if (current_list->used == NODE_LIST_BLOCK_SIZE) {
        current_list->next = new_node_list();
        current_list = current_list->next;
    }
    current_list->entries[current_list->used++] = node;
}

static void init_list_iter(list_node_t *list, list_iter_t *iter) {
    iter->list = list;
    iter->index = 0;
    iter->node = list->entries[0];
}

static inline bool has_iter_value(list_iter_t * iter) {
    return iter->index < iter->list->used || iter->list->next;
}

static inline void next_list_iter(list_iter_t *iter) {
    if (iter->index == NODE_LIST_BLOCK_SIZE - 1) {
        iter->list = iter->list->next;
        iter->index = 0;
        iter->node = iter->list->entries[0];
    } else {
        iter->index++;
        iter->node = iter->list->entries[iter->index];
    }
}

// use a bitset for a set of coordinates

static inline int bitset_size(const graph_t * graph) {
    return (graph->width * graph->height + 63) / 64;
}

static inline void add_coordinate(const graph_t * graph, long * bitset, coordinate_t coord) {
    int index = coord.pri * graph->width + coord.sec;
    bitset[index / 64] |= 1 << (index % 64);
}

static inline bool coordinate_in_set(const graph_t * graph, const long * bitset, coordinate_t coord) {
    int index = coord.pri * graph->width + coord.sec;
    return bitset[index / 64] & (1 << (index % 64));
}

#endif  // __COLLECTION_H__