#include "graph.h"

graph_t* new_grid(const color_t bg_color, int n_rows, int n_cols) {
    graph_t* graph = new_graph();
    if (unlikely(graph == NULL)) {
        return NULL;
    }
    for (int row = 0; row < n_rows; row++) {
        for (int col = 0; col < n_cols; col++) {
            coordinate_t coord = {row, col};
            node_t* node = add_node(graph, coord, 1);
            if (unlikely(node == NULL)) {
                return NULL;
            }
            subnode_t* subnode = get_subnode(node, 0);
            subnode->coord = coord;
            subnode->color = bg_color;
            if (col > 0) {
                coordinate_t left = {row, col - 1};
                node_t* left_node = get_node(graph, left);
                assert(left_node);
                edge_t* edge = add_edge(graph, left_node, node, HORIZONTAL);
                if (unlikely(!edge)) {
                    return NULL;
                }
            }
            if (row > 0) {
                coordinate_t top = {row - 1, col};
                node_t* top_node = get_node(graph, top);
                assert(top_node);
                edge_t* edge = add_edge(graph, top_node, node, VERTICAL);
                if (unlikely(!edge)) {
                    return NULL;
                }
            }
        }
    }
    return graph;
}

graph_t* graph_from_grid(const color_t* grid, int n_rows, int n_cols) {
    graph_t* graph = new_grid(0, n_rows, n_cols);
    for (int row = 0; row < n_rows; row++) {
        for (int col = 0; col < n_cols; col++) {
            coordinate_t coord = {row, col};
            node_t* node = get_node(graph, coord);
            subnode_t* subnode = get_subnode(node, 0);
            subnode->color = grid[row * n_cols + col];
        }
    }
    return graph;
}

graph_t* get_no_abstraction_graph(const graph_t* in) {
    graph_t* out = new_graph();
    if (unlikely(out == NULL)) {
        return NULL;
    }
    coordinate_t coord = {0, 0};
    node_t* out_node = add_node(out, coord, in->n_nodes);
    if (unlikely(out_node == NULL)) {
        return NULL;
    }
    int idx = 0;
    for (const node_t* node = first_node(in); node != NULL; node = next_node(node)) {
        const subnode_t* orig = get_subnode(node, 0);
        subnode_t* out = get_subnode(out_node, idx++);
        out->coord = orig->coord;
        out->color = orig->color;
    }
    return out;
}
