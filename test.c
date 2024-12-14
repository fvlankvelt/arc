#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "graph.h"
#include "image.h"
#include "transform.h"

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
    node_t* node = get_node(graph, (coordinate_t){0, 0});
    remove_node(graph, node);
    ASSERT(graph->n_nodes == 3, "n_nodes incorrect");

    int n_coordinates = 0;
    coordinate_t coordinates[3] = {{0, 1}, {1, 0}, {1, 1}};
    bool found[3] = {false, false, false};
    for (const node_t* n = first_node(graph); n != NULL; n = next_node(n)) {
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
    node_t* top_right = get_node(graph, (coordinate_t){0, 1});
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

    node_t* node = get_node(no_abstract, (coordinate_t){0, 0});
    ASSERT(node->n_subnodes == 4, "n_subnodes incorrect");
    free_graph(graph);
    free_graph(no_abstract);
    return true;
}

bool test_subgraph_by_color() {
    color_t grid[] = {2, 2, 1, 1};
    graph_t* graph = graph_from_grid(grid, 2, 2);

    graph_t* by_color = subgraph_by_color(graph, 2);
    ASSERT(by_color->n_nodes == 2, "n_nodes incorrect");
    free_graph(by_color);

    by_color = subgraph_by_color(graph, 1);
    ASSERT(by_color->n_nodes == 2, "n_nodes incorrect");
    free_graph(by_color);

    by_color = subgraph_by_color(graph, 0);
    ASSERT(by_color->n_nodes == 0, "n_nodes incorrect");
    free_graph(by_color);

    free_graph(graph);
    return true;
}

bool test_connected_components() {
    // clang-format off
    color_t grid[] = {
      2, 2, 0,
      2, 0, 0,
      2, 0, 2,
    };
    // clang-format on
    graph_t* graph = graph_from_grid(grid, 3, 3);

    graph_t* connected = get_connected_components_graph(graph);
    ASSERT(connected->n_nodes == 3, "incorrect number of components");

    node_t* first_component = get_node(connected, (coordinate_t){2, 0});
    ASSERT(first_component->n_subnodes == 4, "first component has wrong number of subnodes");
    ASSERT(first_component->edges, "component does not have any edges");
    node_t* other = get_node(connected, (coordinate_t){2, 1});
    bool linked = false;
    edge_direction_t direction;
    for (const edge_t* edge = first_component->edges; edge; edge = edge->next) {
        if (edge->swap->node == other) {
            linked = true;
            direction = edge->direction;
            break;
        }
    }
    ASSERT(linked, "components are not linked");
    ASSERT(direction == HORIZONTAL, "orientation is incorrect");
    free_graph(connected);

    free_graph(graph);
    return true;
}

bool test_update_color() {
    // clang-format off
    color_t grid[] = {
      2, 2, 0,
      2, 0, 0,
      2, 0, 2,
    };
    // clang-format on
    graph_t* graph = graph_from_grid(grid, 3, 3);
    transform_params_t params = {
        .color = 1
    };
    node_t * node = get_node(graph, (coordinate_t) {0, 0});
    update_color(graph, node, &params);
    subnode_t subnode = get_subnode(node, 0);
    ASSERT(subnode.color == 1, "color is not updates");
    free_graph(graph);
    return true;
}

bool test_move_node() {
    // clang-format off
    color_t grid[] = {
      2, 2, 0,
      2, 0, 0,
      2, 0, 2,
    };
    // clang-format on
    graph_t* graph = graph_from_grid(grid, 3, 3);
    transform_params_t params = {
        .direction = LEFT
    };
    node_t * node = get_node(graph, (coordinate_t) {2, 2});
    move_node(graph, node, &params);
    subnode_t subnode = get_subnode(node, 0);
    ASSERT(subnode.coord.pri == 1 && subnode.coord.sec == 2, "pixel has not moved");
    free_graph(graph);
    return true;
}

bool test_extend_node() {
    // clang-format off
    color_t grid[] = {
      1, 0, 0,
      0, 0, 0,
      0, 0, 2,
    };
    // clang-format on
    graph_t* graph = graph_from_grid(grid, 3, 3);
    transform_params_t params = {
        .direction = UP_LEFT,
        .overlap = false,
    };

    // remove background
    node_t * next_node;
    for (node_t * node = graph->nodes; node; node = next_node) {
        next_node = node->next;
        if (!get_subnode(node, 0).color) {
            remove_node(graph, node);
        }
    }

    node_t * node = get_node(graph, (coordinate_t) {2, 2});
    extend_node(graph, node, &params);
    ASSERT(node->n_subnodes == 2, "node has not been extended");
    subnode_t subnode = get_subnode(node, 0);
    ASSERT(subnode.coord.pri == 2 && subnode.coord.sec == 2, "existing pixel is gone");
    subnode = get_subnode(node, 1);
    ASSERT(subnode.coord.pri == 1 && subnode.coord.sec == 1, "new pixel not added");

    node_t * cst = get_node(graph, (coordinate_t) {0, 0});
    subnode = get_subnode(cst, 0);
    ASSERT(subnode.color == 1, "top-left node has wrong color");
    free_graph(graph);
    return true;
}

int main() {
    bool result = true;

    result &= test_image();
    result &= test_mutate_graph();
    result &= test_no_abstraction();
    result &= test_subgraph_by_color();
    result &= test_connected_components();
    result &= test_update_color();
    result &= test_move_node();
    result &= test_extend_node();

    if (result) {
        printf("PASS\n");
        return 0;
    } else {
        printf("FAIL\n");
        return 1;
    }
}
