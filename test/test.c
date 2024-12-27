#include <stdbool.h>
#include <stdio.h>

extern bool test_graph();
extern bool test_filter();
extern bool test_transform();
extern bool test_mem();
extern bool test_task();

int main() {
    bool result = true;
    printf("Running tests\n");
    // for (;;) {
        result &= test_graph();
        result &= test_filter();
        result &= test_transform();
        result &= test_mem();
        result &= test_task();
    // }
    if (result) {
        return 0;
    } else {
        return 1;
    }
}
