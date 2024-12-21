#ifndef __MEM_H__
#define __MEM_H__

#include <stdlib.h>
#include <assert.h>

typedef struct _mem_item {
    struct _mem_item * next;
} mem_item_t;

typedef struct _mem_block {
    struct _mem_block * next;
    unsigned int _size;
    unsigned int _member_size;
    mem_item_t * _available;
} mem_block_t;

static inline mem_block_t * new_block(unsigned int n, unsigned int member_size) {
    if (member_size < sizeof(mem_item_t)) {
        member_size = sizeof(mem_item_t);
    }
    int mem_block_size = sizeof(mem_block_t);
    mem_block_t * block = malloc(sizeof(mem_block_t) + n * member_size);
    block->next = NULL;
    block->_size = n;
    block->_member_size = member_size;
    mem_item_t * cursor = ((void *) block) + sizeof(mem_block_t);
    block->_available = cursor;
    for (int i = 0; i < n - 1; i++) {
        mem_item_t * next = (mem_item_t *) (((void *) cursor) + member_size);
        cursor->next = next;
        cursor = next;
    }
    cursor->next = NULL;
    return block;
}

static inline void * new_item(mem_block_t * block) {
    if (!block->_available) {
        mem_block_t * next_block = new_block(block->_size, block->_member_size);
        assert(next_block);
        block->_available = next_block->_available;
        next_block->next = block->next;
        block->next = next_block;
    }
    mem_item_t * entry = block->_available;
    block->_available = entry->next;
    return (void *) entry;
}

static inline void free_item(mem_block_t * block, void * p) {
    mem_item_t * entry = (mem_item_t *) p;
    entry->next = block->_available;
    block->_available = entry;
}

static inline void free_block(mem_block_t * block) {
    mem_block_t * next = block->next;
    if (next) {
        free_block(next);
    }
    free(block);
}

#endif // __MEM_H__