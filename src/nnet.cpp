#include "nnet.h"

#include <torch/torch.h>

#include <iostream>

using namespace torch;
using namespace std;

/**
 * Construct a neural network that can guide the search for succesful transformations.
 * A couple of things are at play here
 * - varying input size
 * - varying output size
 * - color permutation symmetry (with the exception of black & gray?)
 * - rotation & reflection symmetry (dihedral-4 group)
 *
 * For dealing with varying input/output sizes, we can decompose the solution into 3 parts:
 * - convolution (per color)
 *    The convolution can be implemented using a 3D convolution,
 *    with kernel-size = 1 in the color dimension (depth).
 *    Kernels are  restricted to follow representations of the dihedral group.
 * - max-pool (per color, channel)
 *   this takes care of the varying input problem
 *
 * Then
 * - for color-independent choices, sum over the colors
 *   (this enforces color permutation symmetry)
 *    - continue with fully connected layers
 *    - produce a vector of choice dimension and softmax it
 * - for a color-selection choice, do not sum
 *    - layers work on each color separately (with weight sharing)
 *    - ultimately for each color a scalar is produced, distribution is a softmax over these
 *
 * Rotations (90-degrees and multiples) and reflections form the dihedral-4 group.  Since these
 * are also symmetries, they should be taken care of as this reduces parameter count
 * significantly.
 *
 * Since a sequence of random variables must be produced, we need to find a way to fold choices
 * that were made (observations) back into the network.  This is like the decoder part of an
 * auto-encoder. Maybe easiest is to keep this restricted to the scale-free part of the network.
 *
 *  (In, Out)                                0 vector
 *      v                                       |
 * (conv|pool)+                                 |
 *      v                                       v
 *     1 st    >   max-pool   >  dense+  >  1st internal  >  dense+  >  soft-max
 *      |                                       |
 *      v                                       v
 * (conv|pool)+                                add        <  dense+  <   choice
 *      |                                       |
 *      v                                       v
 *     2 nd    >   max-pool   >  dense+  >  2nd internal  >  dense+  >  soft-max
 *      |
 *     ...
 *
 * For the (conv|pool) layers to retain permutation invariance, they should either work
 * per-color or aggregate over all layers.  With a commutative aggregation, permutation
 * invariance is retained.
 *
 * Convolutional kernels that transform according to the dihedral group:
 *
 * The invariant kernel (3 degrees of freedom, 1 output channel):
 *     1  2  1
 *     2  3  2
 *     1  2  1
 *
 * The side kernel (1 degree of freedom, 4 output channels):
 *     -  1  -     -  -  -     -  -  -     -  -  -
 *     -  -  -     1  -  -     -  -  -     -  -  1
 *     -  -  -     -  -  -     -  1  -     -  -  -
 *
 * The corner kernel (1 degree of freedom, 4 output channels):
 *     -  -  1     1  -  -     -  -  -     -  -  -
 *     -  -  -     -  -  -     -  -  -     -  -  -
 *     -  -  -     -  -  -     1  -  -     -  -  1
 *
 */

class NNetTrail;

struct NNetModule : nn::Module {
    NNetModule(int n_choices, const string& name)
        : name(name),
          conv(register_module("conv", nn::Conv3d(nn::Conv3dOptions(1, 256, {1, 3, 3})))),
          project(register_module("project", nn::Linear(256, 128))),
          decode(register_module("decode", nn::Linear(128, n_choices))),
          encode(register_module("encode", nn::Linear(n_choices, 128))) {}

    string name;
    nn::Conv3d conv;
    nn::Linear project;
    nn::Sequential decode;
    nn::Sequential encode;
};

class NNetGuide {
    friend class NNetTrail;

   public:
    NNetGuide(vector<shared_ptr<NNetModule>> steps)
        : to_state(nn::Conv3dOptions(1, 256, {1, 3, 3})),
          steps(steps),
          optimizer(to_state->parameters()) {
        for (auto& step : steps) {
            optimizer.add_parameters(step->parameters());
        }
    }

   private:
    void step() {
        optimizer.step();
        optimizer.zero_grad(true);
    }

    // initial transformation from image to the internal (variable image) dimensions
    nn::Conv3d to_state;
    vector<shared_ptr<NNetModule>> steps;
    optim::Adam optimizer;
};

class NNetTrail {
   public:
    NNetTrail(NNetGuide* guide, const Tensor& input, const float* data)
        : guide(guide),
          loss(torch::zeros({1}, TensorOptions().requires_grad(true))),
          iter(guide->steps.begin()),
          image_state(guide->to_state->forward(input)),
          data(data) {}

    ~NNetTrail() { delete data; }

    void next_choice(double* p) {
        auto mod = *iter;
        image_state = mod->conv->forward(image_state);
        projected_state += mod->project->forward(image_state);
        dist_state = mod->decode->forward(projected_state);
    }

    void observe(int choice) {
        if (choice >= 0) {
            Tensor target = torch::zeros(dist_state.sizes());
            target[choice] = 1.0;
            projected_state += (*iter)->encode->forward(target);
            loss += binary_cross_entropy(dist_state, target).set_requires_grad(true);
        }
        ++iter;
    }

    void train() {
        loss.backward();
        guide->step();
    }

   private:
    NNetGuide* guide;
    vector<shared_ptr<NNetModule>>::iterator iter;
    Tensor image_state;
    Tensor projected_state;
    Tensor dist_state;
    Tensor loss;
    const float* data;
};

class NNetBuilder {
   public:
    NNetBuilder() : choices() {}

    void add_choice(unsigned int n_choices, const string& name) {
        choices.push_back(make_shared<NNetModule>(NNetModule(n_choices, name)));
    }

    NNetGuide* build() {
        NNetGuide* guide = new NNetGuide(choices);
        return guide;
    }

   private:
    vector<shared_ptr<NNetModule>> choices;
};

extern "C" {

guide_net_builder_t create_network() { return new NNetBuilder(); }

void add_choice_to_net(guide_net_builder_t net, int n_choices, const char* name) {
    NNetBuilder* builder = static_cast<NNetBuilder*>(net);
    builder->add_choice(n_choices, name);
}

guide_net_t build_network(guide_net_builder_t net) {
    unique_ptr<NNetBuilder> builder(static_cast<NNetBuilder*>(net));
    return builder->build();
}

trail_net_t create_network_trail(
    guide_net_t c_guide,
    unsigned int input_width,
    unsigned int input_height,
    unsigned int* input_pixels,
    unsigned int output_width,
    unsigned int output_height,
    unsigned int* output_pixels) {
    // copy the input data by creating a 3d representation (each color has a depth)
    NNetGuide* guide = static_cast<NNetGuide*>(c_guide);
    float* data = (float*)calloc(10 * (input_width + 2) * (input_height + 2), sizeof(float));
    for (int y = 0; y < input_height; y++) {
        for (int x = 0; x < input_width; x++) {
            int input_idx = y * input_width + x;
            int z = input_pixels[input_idx];
            int input_data_idx = (y + 1) * (input_width + 2) + (x + 1);
            // printf("SETTING (0, %d, %d, %d) to 1.0", z, y, z);
            // fflush(stdout);
            // tensor[0, z, y, x] = 1.0;
            data[10 * input_data_idx + z] = 1.0f;
        }
    }

    return new NNetTrail(
        guide,
        torch::from_blob(
            data, {1, 10, input_height + 2, input_width + 2}, TensorOptions().dtype(kFloat)),
        data);
}

void next_network_choice(trail_net_t c_trail, double* p) {
    NNetTrail* trail = static_cast<NNetTrail*>(c_trail);
    trail->next_choice(p);
}

trail_net_t observe_network_choice(trail_net_t c_trail, int choice) {
    NNetTrail* trail = static_cast<NNetTrail*>(c_trail);
    trail->observe(choice);
    return trail;
}

void complete_trail(trail_net_t c_trail, bool success) {
    NNetTrail* trail = static_cast<NNetTrail*>(c_trail);
    if (success) {
        trail->train();
    }
    delete trail;
}
}

/*
int main() {
    Tensor tensor = torch::eye(3);
    tensor.repeat({3, 4});
    auto conv = nn::Conv2d();
    conv->weight;
    cout << tensor << std::endl;
}
*/