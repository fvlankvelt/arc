#include "guide.h"

#include <math.h>

#include "mtwister.h"
#include "nnet.h"
#include "task.h"

void init_guide(guide_builder_t* builder) {
    builder->items = NULL;
    builder->_items_mem = new_block(256, sizeof(guide_item_t));
}

guide_t* build_guide(guide_builder_t* builder) {
    guide_t* guide = malloc(sizeof(guide_t));
    guide->items = builder->items;
    guide->_trail_mem = new_block(256, sizeof(trail_t));
    guide->_random = seedRand(42l);

    guide_net_builder_t nnet_builder = create_network();
    for (guide_item_t* item = guide->items; item; item = item->next) {
        add_choice_to_net(nnet_builder, item->n_choices, item->name);
    }
    guide->_nnet_guide = build_network(nnet_builder);
    return guide;
}

void add_choice(guide_builder_t* guide, int n_choices, const char* name) {
    assert(n_choices <= MAX_CHOICES);

    guide_item_t* item = new_item(guide->_items_mem);
    item->n_choices = n_choices;
    item->color_covariant = false;
    item->spatial_repr = 0;
    item->name = name;
    item->next = NULL;
    guide_item_t** p_item = &guide->items;
    while (*p_item) {
        p_item = &((*p_item)->next);
    }
    *p_item = item;
}

void add_color(guide_builder_t* guide, const char* name) {
    guide_item_t* item = new_item(guide->_items_mem);
    item->n_choices = 0;
    item->color_covariant = true;
    item->spatial_repr = 0;
    item->name = name;
    item->next = NULL;
    guide_item_t** p_item = &guide->items;
    while (*p_item) {
        p_item = &((*p_item)->next);
    }
    *p_item = item;
}

void add_spatial(guide_builder_t* guide, repr_dihedral_t repr, const char* name) {
    guide_item_t* item = new_item(guide->_items_mem);
    item->n_choices = 0;
    item->color_covariant = false;
    item->spatial_repr = repr;
    item->name = name;
    item->next = NULL;
    guide_item_t** p_item = &guide->items;
    while (*p_item) {
        p_item = &((*p_item)->next);
    }
    *p_item = item;
}

trail_t* new_trail(const graph_t* input, const graph_t* output, guide_t* guide) {
    trail_t* trail = new_item(guide->_trail_mem);
    trail->guide = guide;
    trail->cursor = guide->items;
    trail->prev = NULL;

    trail->dist.size = trail->cursor->n_choices;
    trail->dist.rnd = &guide->_random;

    unsigned int* input_pixels = malloc(input->width * input->height * sizeof(int));
    for (int x = 0; x < input->width; x++) {
        for (int y = 0; y < input->height; y++) {
            int idx = y * input->width + x;
            input_pixels[idx] = input->background_color;
        }
    }
    for (node_t* node = input->nodes; node; node = node->next) {
        for (int i = 0; i < node->n_subnodes; i++) {
            subnode_t subnode = get_subnode(node, i);
            int idx = subnode.coord.sec * input->width + subnode.coord.pri;
            input_pixels[idx] = subnode.color;
        }
    }

    unsigned int* output_pixels = malloc(output->width * output->height * sizeof(int));
    for (int x = 0; x < output->width; x++) {
        for (int y = 0; y < output->height; y++) {
            int idx = y * output->width + x;
            output_pixels[idx] = output->background_color;
        }
    }
    for (node_t* node = output->nodes; node; node = node->next) {
        for (int i = 0; i < node->n_subnodes; i++) {
            subnode_t subnode = get_subnode(node, i);
            int idx = subnode.coord.sec * output->width + subnode.coord.pri;
            output_pixels[idx] = subnode.color;
        }
    }

    trail->_nnet_trail = create_network_trail(
        guide->_nnet_guide,
        input->width,
        input->height,
        input_pixels,
        output->width,
        output->height,
        output_pixels);
    free(input_pixels);
    free(output_pixels);
    return trail;
}

float free_trail(guide_t* guide, trail_t* trail, bool success) {
    float result = complete_trail(trail->_nnet_trail, success);
    for (trail_t* prev = trail->prev; trail; trail = prev, prev = trail ? trail->prev : NULL) {
        free_item(guide->_trail_mem, trail);
    }
    return result;
}

trail_t* backtrack(trail_t* trail) {
    trail_t* prev = trail->prev;
    free_item(trail->guide->_trail_mem, trail);
    return prev;
}

trail_t* observe_choice(trail_t* prev, int choice) {
    prev->choice = choice;
    trail_t* trail = new_item(prev->guide->_trail_mem);
    trail->guide = prev->guide;
    trail->cursor = prev->cursor->next;
    trail->prev = prev;

    if (trail->cursor) {
        trail->dist.size = trail->cursor->n_choices;
    } else {
        trail->dist.size = 0;
    }
    trail->dist.rnd = &trail->guide->_random;
    trail->choice = -1;

    observe_network_choice(prev->_nnet_trail, choice);
    trail->_nnet_trail = prev->_nnet_trail;
    return trail;
}

const categorical_t* next_choice(trail_t* trail) {
    guide_item_t* item = trail->cursor;
    categorical_t* dist = &trail->dist;
    if (item->n_choices) {
        dist->size = item->n_choices;
    } else if (item->color_covariant) {
        dist->size = 10;
    } else {
        switch (item->spatial_repr) {
            case DIHEDRAL_SIDE_CORNER:
                dist->size = 8;
                break;
            case DIHEDRAL_SIDE:
            case DIHEDRAL_CORNER:
            case DIHEDRAL_MIRROR:
                dist->size = 4;
                break;
            case DIHEDRAL_AXIS:
                dist->size = 2;
                break;
        }
    }
    // double p[dist->size];
    next_network_choice(trail->_nnet_trail, dist->p);

    // encourage exploration - try something new in at least 10% of the cases
    double base = 0.1;
    for (int i = 0; i < dist->size; i++) {
        dist->p[i] = (base / dist->size + dist->p[i]) / (1.0 + base);
    }
    return dist;
}

int choose(const categorical_t* dist) {
    double x = genRand(dist->rnd);
    for (int i = 0; i < dist->size; i++) {
        if (x < dist->p[i]) {
            return i;
        }
        x -= dist->p[i];
    }
    return dist->size - 1;
}

int choose_from(const categorical_t* dist, long valid_flags) {
    double sum = 0.0;
    for (int i = 0; i < dist->size; i++) {
        if (valid_flags & (1 << i)) {
            sum += dist->p[i];
        }
    }
    double x = sum * genRand(dist->rnd);
    int last_valid = -1;
    for (int i = 0; i < dist->size; i++) {
        if (valid_flags & (1 << i)) {
            if (x < dist->p[i]) {
                return i;
            }
            x -= dist->p[i];
            last_valid = i;
        }
    }
    return last_valid;
}
