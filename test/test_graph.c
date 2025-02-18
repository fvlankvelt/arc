#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "binding.h"
#include "graph.h"
#include "image.h"
#include "test.h"
#include "transform.h"

extern node_t* bind_node_by_size(const graph_t* graph, const binding_arguments_t* params);
extern node_t* bind_neighbor_by_size(const graph_t* graph, const binding_arguments_t* params);
extern node_t* bind_neighbor_by_color(const graph_t* graph, const binding_arguments_t* params);
extern node_t* bind_neighbor_by_degree(const graph_t* graph, const binding_arguments_t* params);

BEGIN_TEST(test_image) {
    color_t grid[] = {2, 2, 1, 1};
    graph_t* graph = graph_from_grid(grid, 2, 2);
    ASSERT(graph->n_nodes == 4, "n_nodes incorrect");
    free_graph(graph);
}
END_TEST()

BEGIN_TEST(test_mutate_graph) {
    color_t grid[] = {2, 2, 1, 1};
    graph_t* graph = graph_from_grid(grid, 2, 2);
    node_t* node = get_node(graph, (coordinate_t){0, 0});
    remove_node(graph, node);
    ASSERT(graph->n_nodes == 3, "n_nodes incorrect");

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
}
END_TEST()

BEGIN_TEST(test_no_abstraction) {
    color_t grid[] = {2, 2, 1, 1};
    graph_t* graph = graph_from_grid(grid, 2, 2);
    graph_t* no_abstract = get_no_abstraction_graph(graph);
    ASSERT(no_abstract->n_nodes == 1, "n_nodes incorrect");

    node_t* node = get_node(no_abstract, (coordinate_t){0, 0});
    ASSERT(node->n_subnodes == 4, "n_subnodes incorrect");
    free_graph(graph);
    free_graph(no_abstract);
}
END_TEST()

BEGIN_TEST(test_subgraph_by_color) {
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
}
END_TEST()

BEGIN_TEST(test_connected_components) {
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
        if (edge->peer == other) {
            linked = true;
            direction = edge->direction;
            break;
        }
    }
    ASSERT(linked, "components are not linked");
    ASSERT(direction == EDGE_HORIZONTAL, "orientation is incorrect");
    free_graph(connected);

    free_graph(graph);
}
END_TEST()

BEGIN_TEST(test_undo_abstraction) {
    // clang-format off
    color_t grid[] = {
      2, 2, 0,
      2, 0, 0,
      2, 0, 2,
    };
    // clang-format on
    graph_t* graph = graph_from_grid(grid, 3, 3);

    for (int i_abstraction = 0; abstractions[i_abstraction].func; i_abstraction++) {
        graph_t* out = abstractions[i_abstraction].func(graph);
        graph_t* reconstructed = undo_abstraction(out);
        for (int x = 0; x < graph->width; x++) {
            for (int y = 0; y < graph->height; y++) {
                const node_t* node = get_node(reconstructed, (coordinate_t){x, y});
                const node_t* orig = get_node(graph, (coordinate_t){x, y});
                ASSERT(node->n_subnodes == 1, "Reconstructed graph is not flat");
                subnode_t subnode = get_subnode(node, 0);
                subnode_t orig_sub = get_subnode(orig, 0);
                ASSERT(
                    subnode.coord.pri == orig_sub.coord.pri &&
                        subnode.coord.sec == orig_sub.coord.sec,
                    "Coordinates are incorrect");
                ASSERT(subnode.color == orig_sub.color, "Color is incorrect");
            }
        }
        free_graph(reconstructed);
        free_graph(out);
    }
    free_graph(graph);
}
END_TEST()

DEFINE_SUITE(test_graph, {
    RUN_TEST(test_image);
    RUN_TEST(test_mutate_graph);
    RUN_TEST(test_no_abstraction);
    RUN_TEST(test_subgraph_by_color);
    RUN_TEST(test_connected_components);
    RUN_TEST(test_undo_abstraction);
})
