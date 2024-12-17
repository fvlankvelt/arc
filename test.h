#include <stdio.h>
#include <stdlib.h>

#define ASSERT(stmt, msg) \
    if (!(stmt)) {        \
        printf(msg);      \
        printf("\n");     \
        return false;     \
    }

#define DEFINE_TEST(name, body)       \
    bool name() {                     \
        printf("  %s... ", __func__); \
        body;                         \
        printf("PASS\n");             \
        return true;                  \
    }

#define RUN_TEST(name) result &= name()

#define RUN_SUITE(suite, body)          \
    int main() {                        \
        bool result = true;             \
        printf("Testing %s\n", #suite); \
        body;                           \
        if (result) {                   \
            printf("OK\n");             \
            return 0;                   \
        } else {                        \
            printf("FAIL\n");           \
            return 1;                   \
        }                               \
    }
