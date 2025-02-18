#include "filter.h"
#include "graph.h"
#include "image.h"
#include "test.h"
#include "transform.h"

// for testing
extern void update_color(graph_t* graph, node_t* node, transform_arguments_t* params);
extern void move_node(graph_t* graph, node_t* node, transform_arguments_t* params);
extern void extend_node(graph_t* graph, node_t* node, transform_arguments_t* params);
extern void move_node_max(graph_t* graph, node_t* node, transform_arguments_t* params);

BEGIN_TEST(test_update_color) {
    // clang-format off
    color_t grid[] = {
      2, 2, 0,
      2, 0, 0,
      2, 0, 2,
    };
    // clang-format on
    graph_t* graph = graph_from_grid(grid, 3, 3);
    transform_arguments_t params = {.color = 1};
    node_t* node = get_node(graph, (coordinate_t){0, 0});
    update_color(graph, node, &params);
    subnode_t subnode = get_subnode(node, 0);
    ASSERT(subnode.color == 1, "color is not updates");
    free_graph(graph);
}
END_TEST()

BEGIN_TEST(test_move_node) {
    // clang-format off
    color_t grid[] = {
      2, 2, 0,
      2, 0, 0,
      2, 0, 2,
    };
    // clang-format on
    graph_t* graph = graph_from_grid(grid, 3, 3);
    transform_arguments_t params = {.direction = LEFT};
    node_t* node = get_node(graph, (coordinate_t){2, 2});
    move_node(graph, node, &params);
    subnode_t subnode = get_subnode(node, 0);
    ASSERT(subnode.coord.pri == 1 && subnode.coord.sec == 2, "pixel has not moved");
    free_graph(graph);
}
END_TEST()

BEGIN_TEST(test_extend_node) {
    // clang-format off
    color_t grid[] = {
      1, 0, 0,
      0, 0, 0,
      0, 0, 2,
    };
    // clang-format on
    graph_t* graph = graph_from_grid(grid, 3, 3);
    transform_arguments_t params = {
        .direction = UP_LEFT,
        .overlap = false,
    };

    // remove background
    node_t* next_node;
    for (node_t* node = graph->nodes; node; node = next_node) {
        next_node = node->next;
        if (!get_subnode(node, 0).color) {
            remove_node(graph, node);
        }
    }

    node_t* node = get_node(graph, (coordinate_t){2, 2});
    extend_node(graph, node, &params);
    ASSERT(node->n_subnodes == 2, "node has not been extended");
    subnode_t subnode = get_subnode(node, 0);
    ASSERT(subnode.coord.pri == 2 && subnode.coord.sec == 2, "existing pixel is gone");
    subnode = get_subnode(node, 1);
    ASSERT(subnode.coord.pri == 1 && subnode.coord.sec == 1, "new pixel not added");

    node_t* cst = get_node(graph, (coordinate_t){0, 0});
    subnode = get_subnode(cst, 0);
    ASSERT(subnode.color == 1, "top-left node has wrong color");
    free_graph(graph);
}
END_TEST()

BEGIN_TEST(test_move_node_max) {
    // clang-format off
    color_t grid[] = {
      2, 2, 0,
      2, 0, 0,
      2, 0, 2,
    };
    // clang-format on
    graph_t* graph = graph_from_grid(grid, 3, 3);
    remove_node(graph, get_node(graph, (coordinate_t){1, 2}));

    transform_arguments_t params = {.direction = LEFT};
    node_t* node = get_node(graph, (coordinate_t){2, 2});
    move_node_max(graph, node, &params);
    subnode_t subnode = get_subnode(node, 0);
    ASSERT(subnode.coord.pri == 1 && subnode.coord.sec == 2, "pixel has not moved");
    free_graph(graph);
}
END_TEST()

DEFINE_SUITE(test_transform, ({
              RUN_TEST(test_update_color);
              RUN_TEST(test_move_node);
              RUN_TEST(test_extend_node);
              RUN_TEST(test_move_node_max);
          }))