#include "filter.h"
#include "graph.h"
#include "image.h"
#include "test.h"
#include "transform.h"

// for testing
extern bool update_color(graph_t* graph, node_t* node, transform_arguments_t* params);
extern bool move_node(graph_t* graph, node_t* node, transform_arguments_t* params);
extern bool extend_node(graph_t* graph, node_t* node, transform_arguments_t* params);
extern bool move_node_max(graph_t* graph, node_t* node, transform_arguments_t* params);
extern bool rotate_node(graph_t* graph, node_t* node, transform_arguments_t* params);
extern bool add_border(graph_t* graph, node_t* node, transform_arguments_t* params);
extern bool fill_rectangle(graph_t* graph, node_t* node, transform_arguments_t* params);

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

BEGIN_TEST(test_rotate_node) {
    // clang-format off
    color_t grid[] = {
      0, 1, 0,
      0, 1, 1,
      0, 0, 0,
    };
    // clang-format on
    graph_t* graph = graph_from_grid(grid, 3, 3);
    graph_t* abstraction = get_connected_components_graph_background_removed(graph);

    transform_arguments_t params = {.rotation_dir = CLOCK_WISE};
    node_t* node = get_node(abstraction, (coordinate_t){1, 0});
    ASSERT(node->n_subnodes == 3, "node not found");

    subnode_t subnode = get_subnode(node, 0);
    ASSERT(subnode.coord.pri == 1 && subnode.coord.sec == 0, "subnode order changed");

    bool rotated = rotate_node(abstraction, node, &params);
    ASSERT(rotated, "rotation failed");

    subnode = get_subnode(node, 0);
    ASSERT(subnode.coord.pri == 2 && subnode.coord.sec == 1, "pixel has not moved");

    free_graph(abstraction);
    free_graph(graph);
}
END_TEST()

BEGIN_TEST(test_add_border) {
    // clang-format off
    color_t grid[] = {
      0, 1, 0,
      0, 1, 1,
      0, 0, 0,
    };
    // clang-format on
    graph_t* graph = graph_from_grid(grid, 3, 3);
    graph_t* abstraction = get_connected_components_graph_background_removed(graph);

    transform_arguments_t params = {.color = 4};
    node_t* node = get_node(abstraction, (coordinate_t){1, 0});
    ASSERT(node->n_subnodes == 3, "node not found");

    bool added = add_border(abstraction, node, &params);
    ASSERT(added, "adding border failed");

    node_t * border = get_node(abstraction, (coordinate_t){2, 0});
    ASSERT(border && border->n_subnodes == 6, "border not fully drawn");

    free_graph(abstraction);
    free_graph(graph);
}
END_TEST()

BEGIN_TEST(test_fill_rectangle) {
    // clang-format off
    color_t grid[] = {
      0, 1, 0,
      0, 1, 1,
      0, 1, 2,
    };
    // clang-format on
    graph_t* graph = graph_from_grid(grid, 3, 3);
    graph_t* abstraction = get_connected_components_graph_background_removed(graph);

    transform_arguments_t params = {.color = 4, .overlap = true};
    node_t* node = get_node(abstraction, (coordinate_t){1, 0});
    ASSERT(node->n_subnodes == 4, "node not found");

    bool added = fill_rectangle(abstraction, node, &params);
    ASSERT(added, "filling rectangle failed");

    node_t * rect = get_node(abstraction, (coordinate_t){3, 0});
    ASSERT(rect && rect->n_subnodes == 2, "rectangle not fully filled");

    free_graph(abstraction);
    free_graph(graph);
}
END_TEST()

DEFINE_SUITE(test_transform, ({
              RUN_TEST(test_update_color);
              RUN_TEST(test_move_node);
              RUN_TEST(test_extend_node);
              RUN_TEST(test_move_node_max);
              RUN_TEST(test_rotate_node);
              RUN_TEST(test_add_border);
              RUN_TEST(test_fill_rectangle);
          }))