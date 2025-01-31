#ifndef __NNET_H__
#define __NNET_H__

#ifdef __cplusplus
extern "C" {
#endif

typedef void * guide_net_builder_t;

guide_net_builder_t create_network();
void add_choice_to_net(guide_net_builder_t net, int n_choices, const char * name);

typedef void * guide_net_t;
guide_net_t build_network(guide_net_builder_t builder);

typedef void * trail_net_t;

trail_net_t create_network_trail(
  guide_net_t net,
  unsigned int input_width,
  unsigned int input_height,
  unsigned int * input_pixels,
  unsigned int output_width,
  unsigned int output_height,
  unsigned int * output_pixels
);

void next_network_choice(trail_net_t trail, double * p);
trail_net_t observe_network_choice(trail_net_t trail, int choice);
void complete_trail(trail_net_t trail, bool success);

#ifdef __cplusplus
}
#endif

#endif // __NNET_H__
