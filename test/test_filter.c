#include "test.h"

#include "graph.h"
#include "image.h"
#include "filter.h"
#include "transform.h"

extern bool filter_by_color(const graph_t* graph, const node_t* node, const filter_arguments_t* args);
extern bool filter_by_size(const graph_t* graph, const node_t* node, const filter_arguments_t* args);
extern bool filter_by_degree(const graph_t* graph, const node_t* node, const filter_arguments_t* args);

DEFINE_TEST(test_filter_by_color,  ({
    color_t grid[] = {2, 2, 1, 1};
    graph_t* graph = graph_from_grid(grid, 2, 2);

    filter_arguments_t args = {
        .color = 2,
        .exclude = false,
    };
    node_t * node = get_node(graph, (coordinate_t){0, 0});
    bool matches = filter_by_color(graph, node, &args);
    ASSERT(matches, "node does not match");

    args.exclude = true;
    matches = filter_by_color(graph, node, &args);
    ASSERT(!matches, "node matches but shouldn't");

    args.color = 1;
    matches = filter_by_color(graph, node, &args);
    ASSERT(matches, "node does not match");

    args.exclude = false;
    matches = filter_by_color(graph, node, &args);
    ASSERT(!matches, "node matches but shouldn't");

    free_graph(graph);
}))

DEFINE_TEST(test_filter_by_derived_colors,  ({
    color_t grid[] = {2, 2, 0, 1};
    graph_t* graph = graph_from_grid(grid, 2, 2);

    filter_arguments_t args = {
        .color = MOST_COMMON_COLOR,
        .exclude = false,
    };
    node_t * node = get_node(graph, (coordinate_t){0, 0});
    bool matches = filter_by_color(graph, node, &args);
    ASSERT(matches, "node does not match");

    node = get_node(graph, (coordinate_t){1, 1});
    args = (filter_arguments_t) {.color = LEAST_COMMON_COLOR, .exclude = false};
    matches = filter_by_color(graph, node, &args);
    ASSERT(matches, "node does not match");

    node = get_node(graph, (coordinate_t){0, 1});
    args = (filter_arguments_t) {.color = BACKGROUND_COLOR, .exclude = false};
    matches = filter_by_color(graph, node, &args);
    ASSERT(matches, "node does not match");
    free_graph(graph);
}))

DEFINE_TEST(test_filter_by_size, ({
    color_t grid[] = {2, 2, 0, 1};
    graph_t* graph = graph_from_grid(grid, 2, 2);

    graph_t* connected = get_connected_components_graph(graph);
    ASSERT(connected->n_nodes == 3, "incorrect number of components");

    filter_arguments_t args = {
        .size = 2,
        .exclude = false,
    };
    node_t * node = get_node(connected, (coordinate_t){2, 0});
    bool matches = filter_by_size(connected, node, &args);
    ASSERT(matches, "node does not match");

    args = (filter_arguments_t){.size = ODD_SIZE, .exclude = true};
    matches = filter_by_size(connected, node, &args);
    ASSERT(matches, "node does not match");

    args = (filter_arguments_t){.size = MAX_SIZE, .exclude = false};
    matches = filter_by_size(connected, node, &args);
    ASSERT(matches, "node does not match");

    node = get_node(connected, (coordinate_t){1, 0});
    args = (filter_arguments_t){.size = MIN_SIZE, .exclude = false};
    matches = filter_by_size(connected, node, &args);
    ASSERT(matches, "node does not match");

    free_graph(connected);
    free_graph(graph);
}))

DEFINE_TEST(test_filter_by_degree, ({
    color_t grid[] = {2, 2, 0, 1};
    graph_t* graph = graph_from_grid(grid, 2, 2);

    graph_t* connected = get_connected_components_graph(graph);
    ASSERT(connected->n_nodes == 3, "incorrect number of components");

    filter_arguments_t args = {
        .degree = 2,
        .exclude = false,
    };
    node_t * node = get_node(connected, (coordinate_t){2, 0});
    bool matches = filter_by_degree(connected, node, &args);
    ASSERT(matches, "node does not match");

    free_graph(connected);
    free_graph(graph);
}))

RUN_SUITE(test_filter, ({
    RUN_TEST(test_filter_by_color);
    RUN_TEST(test_filter_by_derived_colors);
    RUN_TEST(test_filter_by_size);
    RUN_TEST(test_filter_by_degree);
}))