#include "guide.h"

#include <math.h>

#include "mtwister.h"
#include "task.h"

guide_t* new_guide() {
    guide_t* guide = malloc(sizeof(guide_t));
    guide->items = NULL;
    guide->_items_mem = new_block(256, sizeof(guide_item_t));
    guide->_trail_mem = new_block(256, sizeof(trail_t));
    guide->_random = seedRand(42l);
    return guide;
}

void add_choice(guide_t* guide, int n_choices, const char* name) {
    assert(n_choices <= MAX_CHOICES);

    guide_item_t* item = new_item(guide->_items_mem);
    item->n_choices = n_choices;
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
    return trail;
}

void free_trail(guide_t* guide, trail_t* trail) {
    for (trail_t* prev = trail->prev; trail; trail = prev, prev = trail ? trail->prev : NULL) {
        free_item(guide->_trail_mem, trail);
    }
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
    return trail;
}

const categorical_t* next_choice(trail_t* trail) {
    guide_item_t* item = trail->cursor;
    categorical_t* dist = &trail->dist;
    dist->size = item->n_choices;
    for (int i = 0; i < item->n_choices; i++) {
        dist->p[i] = 1.0 / item->n_choices;
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

void mark_success(trail_t* trail) {}