#ifndef __GUIDE_H__
#define __GUIDE_H__

#include "graph.h"
#include "mem.h"
#include "mtwister.h"

#define MAX_CHOICES 32

/**
 * A guide provides a sequence of categorical probability distributions,
 * taking as observations the (input, output) pair initially and folding in
 * samples/observations from the categorical distributions.
 *
 * The sequence is determined by the order in which the main program executes.
 */

/**
 * specify how the choices transform under symmetry operations
 * - rotation & flips (D_4 dihedral group)
 */
typedef enum _repr_dihedral {
    DIHEDRAL_SIDE_CORNER,  // 2x 4-dimensional
    DIHEDRAL_CORNER,       // 4 dimensional
    DIHEDRAL_SIDE,         // 4 dimensional
    DIHEDRAL_MIRROR,       // 2x 2-dimensional
    DIHEDRAL_AXIS          // 2 dimensional (horizontal, vertical)
} repr_dihedral_t;

typedef struct _guide_item {
    struct _guide_item* next;
    int n_choices;
    bool color_covariant;
    repr_dihedral_t spatial_repr;
    const char* name;
} guide_item_t;


typedef struct _guide_builder {
    guide_item_t* items;
    mem_block_t* _items_mem;
} guide_builder_t;

typedef struct _guide {
    guide_item_t* items;
    MTRand _random;
    mem_block_t* _trail_mem;
    void * _nnet_guide;
} guide_t;

/**
 * A trail corresponds to an execution.  It consists of an interplay between
 * providing distributions to sample choices from and observing choices to
 * inform future distributions.
 *
 * This allows (in principle) a full joint distribution to be constructed as
 *   P(t_1) * P(t_2|t_1) * P(t_3|t_1,t_2) * ...
 */

/**
 * Normalized categorical distribution
 */
typedef struct {
    int size;
    MTRand* rnd;
    double p[MAX_CHOICES];
} categorical_t;

typedef struct _trail {
    guide_t* guide;
    guide_item_t* cursor;
    struct _trail* prev;

    categorical_t dist;
    int choice;

    void * _nnet_trail;
} trail_t;

void init_guide(guide_builder_t * builder);

void add_choice(guide_builder_t* builder, int n_choices, const char* name);

// add a covariant color - 10 values that transform under the color permutation group
void add_color(guide_builder_t* builder, const char* name);

// add a spatial representation, transforming under the dihedral group
void add_spatial(guide_builder_t* builder, repr_dihedral_t repr, const char* name);

guide_t * build_guide(guide_builder_t * builder);

trail_t* new_trail(const graph_t* input, const graph_t* output, guide_t* guide);

/**
 * Before continuing to the next choice on the trail, the observed choice
 * must be provided.  This is the sampled choice when searching for solutions,
 * or the used choice when training.
 *
 * If the random variable was not used, use -1 for choice - it will then be
 * marginalized out (i.e. the distribution is used as observation instead of
 * the one-hot encoded choice)
 */
trail_t* observe_choice(trail_t* trail, int choice);

trail_t* backtrack(trail_t* trail);

const categorical_t* next_choice(trail_t* trail);

int choose(const categorical_t* dist);

int choose_from(const categorical_t* dist, long valid_flags);

/**
 * When training the guide, it optimizes the provided distributions for the
 * choices that were made.  Setting success to true on a trail means that the
 * observed choices led to a succesful transformation of the input image to
 * the output image.
 */
float free_trail(guide_t* guide, trail_t* trail, bool success);

/*

typedef struct {
    categorical_t func;
    categorical_t size;
    categorical_t degree;
    categorical_t exclude;
    categorical_t color;
} binding_dist_t;

typedef struct {
    int func;
    int size;
    int degree;
    int exclude;
    int color;
} binding_sample_t;

typedef struct {
    categorical_t func;
    categorical_t color;
    categorical_t direction;
    categorical_t overlap;
    categorical_t rotation_dir;
    categorical_t mirror_axis;
    categorical_t mirror_direction;
    categorical_t relative_pos;
    categorical_t point;
    // color_t color;
    // direction_t direction;
    // bool overlap;
    // rotation_t rotation_dir;
    // mirror_axis_t mirror_axis;
    // mirror_t mirror_direction;
    // short object_id;
    // image_points_t point;
    // relative_position_t relative_pos;

    // to be used when parameter value is "dynamic"
    binding_dist_t dynamic_color;
    binding_dist_t dynamic_direction;
    binding_dist_t dynamic_mirror_axis;
    binding_dist_t dynamic_point;
} transform_dist_t;

typedef struct {
    int func;
    int color;
    int direction;
    int overlap;
    int rotation_dir;
    int mirror_axis;
    int mirror_direction;
    int relative_pos;
    int point;

    binding_sample_t dynamic_color;
    binding_sample_t dynamic_direction;
    binding_sample_t dynamic_mirror_axis;
    binding_sample_t dynamic_point;
} transform_sample_t;
*/

#endif  // __GUIDE_H__