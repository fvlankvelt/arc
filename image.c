#include "graph.h"
#include "collection.h"

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
            set_subnode(node, 0, (subnode_t){coord, bg_color});
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
            subnode_t subnode = get_subnode(node, 0);
            subnode.color = grid[row * n_cols + col];
            set_subnode(node, 0, subnode);
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
        const subnode_t orig = get_subnode(node, 0);
        set_subnode(out_node, idx, orig);
    }
    return out;
}

graph_t* subgraph_by_color(const graph_t* in, color_t color) {
    graph_t* out = new_graph();
    for (const node_t* node = in->nodes; node; node = node->next) {
        if (node->n_subnodes == 1) {
            subnode_t subnode = get_subnode(node, 0);
            if (subnode.color == color) {
                node_t* new_node = add_node(out, node->coord, 1);
                for (const edge_t* edge = node->edges; edge; edge = edge->next) {
                    const node_t* other = edge->swap->node;
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

void _add_neighbors(node_set_t * visited, list_node_t * nodes, const node_t * node, int *  count) {
  for (const edge_t * edge = node->edges; edge; edge = edge->next) {
    if (is_node_in_set(visited, edge->swap->node)) {
      continue;
    }
    node_t * peer = edge->swap->node;
    add_node_to_set(visited, peer);
    add_node_to_list(nodes, peer);
    (*count)++;
    _add_neighbors(visited, nodes, peer, count);
  } 
} 

graph_t* get_connected_components_graph(const graph_t* in) { 
    graph_t* out = new_graph(); 
    for (color_t color = 0; color < 10; color++) { 
        graph_t* by_color = subgraph_by_color(in, color); 
        node_set_t* visited = new_node_set(by_color->n_nodes);
        int component_idx = 0;
        for (const node_t* node = by_color->nodes; node; node = node->next) {
            if (is_node_in_set(visited, node)) {
                continue;
            }
            int n_subnodes = 1;
            list_node_t * node_list = new_node_list();
            add_node_to_set(visited, node);
            add_node_to_list(node_list, node);
            _add_neighbors(visited, node_list, node, &n_subnodes);

            node_t * component = add_node(out, (coordinate_t) {color, component_idx}, n_subnodes);
            list_iter_t iter;
            int subnode_idx = 0;
            for (init_list_iter(node_list, &iter); has_iter_value(&iter); next_list_iter(&iter)) {
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
    return out;
}