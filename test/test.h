#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#define ASSERT(stmt, msg)                      \
    if (!(stmt)) {                             \
        printf("%s (line %d)", msg, __LINE__); \
        printf("\n");                          \
        return false;                          \
    }

#define BEGIN_TEST(name) \
    bool name() {        \
        printf("  %s... ", __func__);
#define END_TEST()    \
    printf("PASS\n"); \
    return true;      \
    }

#define RUN_TEST(name) result &= name()

#define DEFINE_SUITE(suite, body)          \
    bool suite() {                      \
        bool result = true;             \
        printf("Testing %s\n", #suite); \
        body;                           \
        if (result) {                   \
            printf("OK\n");             \
            return true;                \
        } else {                        \
            printf("FAIL\n");           \
            return false;               \
        }                               \
    }
