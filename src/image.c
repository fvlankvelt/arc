#include "collection.h"
#include "graph.h"
#include "image.h"

graph_t* new_grid(const color_t bg_color, int n_rows, int n_cols) {
    graph_t* graph = new_graph(n_cols, n_rows);
    if (unlikely(graph == NULL)) {
        return NULL;
    }
    for (int row = 0; row < n_rows; row++) {
        for (int col = 0; col < n_cols; col++) {
            coordinate_t coord = {col, row};
            node_t* node = add_node(graph, coord, 1);
            if (unlikely(node == NULL)) {
                return NULL;
            }
            set_subnode(node, 0, (subnode_t){coord, bg_color});
            if (col > 0) {
                coordinate_t left = {col - 1, row};
                node_t* left_node = get_node(graph, left);
                assert(left_node);
                edge_t* edge = add_edge(graph, left_node, node, EDGE_HORIZONTAL);
                if (unlikely(!edge)) {
                    return NULL;
                }
            }
            if (row > 0) {
                coordinate_t top = {col, row - 1};
                node_t* top_node = get_node(graph, top);
                assert(top_node);
                edge_t* edge = add_edge(graph, top_node, node, EDGE_VERTICAL);
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
            coordinate_t coord = {col, row};
            node_t* node = get_node(graph, coord);
            subnode_t subnode = get_subnode(node, 0);
            subnode.color = grid[row * n_cols + col];
            set_subnode(node, 0, subnode);
        }
    }
    return graph;
}

graph_t* get_no_abstraction_graph(const graph_t* in) {
    graph_t* out = new_graph(in->width, in->height);
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
        const subnode_t orig = get_subnode(node, 0);
        set_subnode(out_node, idx, orig);
    }
    return out;
}

graph_t* subgraph_by_color(const graph_t* in, color_t color) {
    graph_t* out = new_graph(in->width, in->height);
    for (const node_t* node = in->nodes; node; node = node->next) {
        if (node->n_subnodes == 1) {
            subnode_t subnode = get_subnode(node, 0);
            if (subnode.color == color) {
                node_t* new_node = add_node(out, node->coord, 1);
                for (const edge_t* edge = node->edges; edge; edge = edge->next) {
                    const node_t* other = edge->peer;
                    node_t* new_other = get_node(out, other->coord);
                    if (new_other) {
                        add_edge(out, new_node, new_other, edge->direction);
                    }
                }
            }
        }
    }
    return out;
}

void _add_neighbors(node_set_t* visited, list_node_t* nodes, const node_t* node, int* count) {
    for (const edge_t* edge = node->edges; edge; edge = edge->next) {
        if (is_node_in_set(visited, edge->peer)) {
            continue;
        }
        node_t* peer = edge->peer;
        add_node_to_set(visited, peer);
        add_node_to_list(nodes, peer);
        (*count)++;
        _add_neighbors(visited, nodes, peer, count);
    }
}

graph_t* get_connected_components_graph(const graph_t* in) {
    graph_t* out = new_graph(in->width, in->height);
    for (color_t color = 0; color < 10; color++) {
        graph_t* by_color = subgraph_by_color(in, color);
        node_set_t* visited = new_node_set(by_color->n_nodes);
        int component_idx = 0;
        for (const node_t* node = by_color->nodes; node; node = node->next) {
            if (is_node_in_set(visited, node)) {
                continue;
            }
            int n_subnodes = 1;
            list_node_t* node_list = new_node_list();
            add_node_to_set(visited, node);
            add_node_to_list(node_list, node);
            _add_neighbors(visited, node_list, node, &n_subnodes);

            node_t* component = add_node(out, (coordinate_t){color, component_idx}, n_subnodes);
            list_iter_t iter;
            int subnode_idx = 0;
            for (init_list_iter(node_list, &iter); has_iter_value(&iter);
                 next_list_iter(&iter)) {
                subnode_t subnode = get_subnode(component, subnode_idx);
                subnode.coord = iter.node->coord;
                subnode.color = color;
                set_subnode(component, subnode_idx, subnode);
                subnode_idx++;
            }
            component_idx++;
            free_node_list(node_list);
        }
        free_node_set(visited);
        free_graph(by_color);
    }

    derived_props_t props = get_derived_properties(in);
    for (node_t* node1 = out->nodes; node1; node1 = node1->next) {
        for (node_t* node2 = node1->next; node2; node2 = node2->next) {
            bool edge_added = false;
            for (int sub_1 = 0; !edge_added && sub_1 < node1->n_subnodes; sub_1++) {
                for (int sub_2 = 0; !edge_added && sub_2 < node2->n_subnodes; sub_2++) {
                    subnode_t subnode_1 = get_subnode(node1, sub_1);
                    subnode_t subnode_2 = get_subnode(node2, sub_2);
                    if (subnode_1.coord.pri == subnode_2.coord.pri) {
                        bool found = false;
                        int pri = subnode_1.coord.pri;
                        int min, max;
                        if (subnode_1.coord.sec < subnode_2.coord.sec) {
                            min = subnode_1.coord.sec;
                            max = subnode_2.coord.sec;
                        } else {
                            min = subnode_2.coord.sec;
                            max = subnode_1.coord.sec;
                        }
                        for (int sec = min + 1; sec < max; sec++) {
                            const node_t* orig_node = get_node(in, (coordinate_t){pri, sec});
                            if (get_subnode(orig_node, 0).color != props.background_color) {
                                found = true;
                                break;
                            }
                        }
                        if (!found) {
                            add_edge(out, node1, node2, EDGE_VERTICAL);
                            edge_added = true;
                            break;
                        }
                    } else if (subnode_1.coord.sec == subnode_2.coord.sec) {
                        bool found = false;
                        int sec = subnode_1.coord.sec;
                        int min, max;
                        if (subnode_1.coord.pri < subnode_2.coord.pri) {
                            min = subnode_1.coord.pri;
                            max = subnode_2.coord.pri;
                        } else {
                            min = subnode_2.coord.pri;
                            max = subnode_1.coord.pri;
                        }
                        for (int pri = min + 1; pri < max; pri++) {
                            const node_t* orig_node = get_node(in, (coordinate_t){pri, sec});
                            if (get_subnode(orig_node, 0).color != props.background_color) {
                                found = true;
                                break;
                            }
                        }
                        if (!found) {
                            add_edge(out, node1, node2, EDGE_HORIZONTAL);
                            edge_added = true;
                            break;
                        }
                    }
                }
            }
        }
    }
    return out;
}

abstraction_t abstractions[] = {
    {
        .func = get_no_abstraction_graph,
    },
    {
        .func = get_connected_components_graph,
    }
};