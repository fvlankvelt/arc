#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "graph.h"
#include "image.h"

#define ASSERT(stmt, msg) \
    if (!(stmt)) {        \
        printf(msg);      \
        printf("\n");     \
        return false;     \
    }

bool test_image() {
    color_t grid[] = {2, 2, 1, 1};
    graph_t* graph = graph_from_grid(grid, 2, 2);
    ASSERT(graph->n_nodes == 4, "n_nodes incorrect");
    return true;
}

bool test_no_abstraction() {
    color_t grid[] = {2, 2, 1, 1};
    graph_t* graph = graph_from_grid(grid, 2, 2);
    graph_t* no_abstract = get_no_abstraction_graph(graph);
    ASSERT(no_abstract->n_nodes == 1, "n_nodes incorrect");

    node_t * node = get_node(no_abstract, (coordinate_t) {0, 0});
    ASSERT(node->n_subnodes == 4, "n_subnodes incorrect");
    return true;
}

int main() {
    bool result = true;

    result &= test_image();
    result &= test_no_abstraction();

    if (result) {
        printf("PASS\n");
        return 0;
    } else {
        printf("FAIL\n");
        return 1;
    }
}
