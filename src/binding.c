#include "graph.h"
#include "filter.h"
#include "binding.h"

node_t * bind_node_by_size(const graph_t * graph, const binding_arguments_t * params) {
    filter_arguments_t args;
    args.size = params->size;
    args.exclude = params->exclude;
    for  (node_t * node = graph->nodes; node; node = node->next) {
        if (filter_by_size(graph, node, &args)) {
            return node;
        }
    }
    return NULL;
}

node_t * bind_neighbor_by_size(const graph_t * graph, const binding_arguments_t * params) {
    filter_arguments_t args;
    args.size = params->size;
    args.exclude = params->exclude;
    for (const edge_t * edge = params->node->edges; edge; edge = edge->next) {
        if (filter_by_size(graph, edge->swap->node, &args)) {
            return edge->swap->node;
        }
    }
    return NULL;
}

node_t * bind_neighbor_by_color(const graph_t * graph, const binding_arguments_t * params) {
    filter_arguments_t args;
    args.color = params->color;
    args.exclude = params->exclude;
    for (const edge_t * edge = params->node->edges; edge; edge = edge->next) {
        node_t * peer = edge->swap->node;
        if (filter_by_color(graph, peer, &args)) {
            return peer;
        }
    }
    return NULL;
}

node_t * bind_neighbor_by_degree(const graph_t * graph, const binding_arguments_t * params) {
    filter_arguments_t args;
    args.degree = params->degree;
    args.exclude = params->exclude;
    for (const edge_t * edge = params->node->edges; edge; edge = edge->next) {
        node_t * peer = edge->swap->node;
        if (filter_by_degree(graph, peer, &args)) {
            return peer;
        }
    }
    return NULL;
}

binding_func_t binding_funcs[] = {
    {
        .func = bind_node_by_size,
        .size = true,
        .exclude = true,
    },
    {
        .func = bind_neighbor_by_size,
        .node = true,
        .size = true,
        .exclude = true,
    },
    {
        .func = bind_neighbor_by_color,
        .node = true,
        .exclude = true,
        .color = true,
    },
    {
        .func = bind_neighbor_by_degree,
        .node = true,
        .exclude = true,
        .degree = true,
    },
};