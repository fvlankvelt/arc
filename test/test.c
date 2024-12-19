#include <stdbool.h>
#include <stdio.h>

extern bool test_graph();
extern bool test_filter();
extern bool test_transform();

int main() {
    bool result = true;
    printf("Running tests\n");
    result &= test_graph();
    result &= test_filter();
    result &= test_transform();
    if (result) {
        return 0;
    } else {
        return 1;
    }
}
