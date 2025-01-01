#include "mem.h"
#include "test.h"

typedef struct _test_item {
    struct _test_item* next;
    int item;
} test_item_t;

BEGIN_TEST(test_alloc_many) {
    mem_block_t* block = new_block(128, sizeof(test_item_t));

    test_item_t* cursor = new_item(block);
    cursor->item = 0;
    for (int i = 1; i < 1000; i++) {
        test_item_t* item = new_item(block);
        item->next = cursor;
        item->item = i;
        cursor = item;
    }

    for (int i = 0; i < 1000; i++) {
        test_item_t* next = cursor->next;
        free_item(block, cursor);
        cursor = next;
    }

    free_block(block);
}
END_TEST()

DEFINE_SUITE(test_mem, { RUN_TEST(test_alloc_many); })