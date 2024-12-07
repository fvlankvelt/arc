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
    free_graph(graph);
    return true;
}

bool test_mutate_graph() {
    color_t grid[] = {2, 2, 1, 1};
    graph_t* graph = graph_from_grid(grid, 2, 2);
    node_t * node = get_node(graph, (coordinate_t) {0, 0});
    remove_node(graph, node);
    ASSERT(graph->n_nodes == 3, "n_nodes incorrect");

    int n_coordinates = 0;
    coordinate_t coordinates[3] = {{0, 1}, {1, 0}, {1, 1}};
    bool found[3] = {false, false, false};
    for (const node_t * n = first_node(graph); n != NULL; n = next_node(n)) {
      for (int i = 0; i < 3; i++) {
        coordinate_t coord = coordinates[i];
        if (coord.pri == n->coord.pri && coord.sec == n->coord.sec) {
          found[i] = true;
        }
      }
    }
    for (int i = 0; i < 3; i++) {
      ASSERT(found[i], "Coordinate not found");
    }

    // check edges
    node_t * top_right = get_node(graph, (coordinate_t) {0, 1});
    ASSERT(top_right->edges, "No edge found");
    ASSERT(!top_right->edges->next, "More than 1 edge found");

    free_graph(graph);
    return true;
}

bool test_no_abstraction() {
    color_t grid[] = {2, 2, 1, 1};
    graph_t* graph = graph_from_grid(grid, 2, 2);
    graph_t* no_abstract = get_no_abstraction_graph(graph);
    ASSERT(no_abstract->n_nodes == 1, "n_nodes incorrect");

    node_t * node = get_node(no_abstract, (coordinate_t) {0, 0});
    ASSERT(node->n_subnodes == 4, "n_subnodes incorrect");
    free_graph(graph);
    free_graph(no_abstract);
    return true;
}

int main() {
    bool result = true;

    result &= test_image();
    result &= test_mutate_graph();
    result &= test_no_abstraction();

    if (result) {
        printf("PASS\n");
        return 0;
    } else {
        printf("FAIL\n");
        return 1;
    }
}
