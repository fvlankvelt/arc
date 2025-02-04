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

struct NNetModuleImpl : nn::Module {
    NNetModuleImpl(int n_choices, const string& name)
        : name(name),
          conv_input(register_module(
              "conv_input",
              nn::Conv3d(nn::Conv3dOptions(256, 256, {1, 3, 3}).padding({0, 1, 1})))),
          conv_output(register_module(
              "conv_output",
              nn::Conv3d(nn::Conv3dOptions(256, 256, {1, 3, 3}).padding({0, 1, 1})))),
          project_input(register_module("project_input", nn::Linear(256, 64))),
          project_output(register_module("project_output", nn::Linear(256, 64))),
          decode(register_module("decode", nn::Linear(128, n_choices))),
          encode(register_module("encode", nn::Linear(n_choices, 128))) {}

    string name;
    nn::Conv3d conv_input;
    nn::Conv3d conv_output;
    nn::Linear project_input;
    nn::Linear project_output;
    nn::Sequential decode;
    nn::Sequential encode;
};

TORCH_MODULE(NNetModule);

class NNetGuide {
    friend class NNetTrail;

   public:
    NNetGuide(vector<NNetModule>& steps)
        : init_input(nn::Conv3dOptions(1, 256, {1, 3, 3}).padding({0, 1, 1})),
          init_output(nn::Conv3dOptions(1, 256, {1, 3, 3}).padding({0, 1, 1})),
          steps(steps),
          optimizer(init_input->parameters()) {
        for (auto& step : steps) {
            optimizer.add_param_group(optim::OptimizerParamGroup(step->parameters()));
        }
    }

   private:
    void step() {
        optimizer.step();
        optimizer.zero_grad(true);
    }

    // initial transformation from image to the internal (variable image) dimensions
    nn::Conv3d init_input;
    nn::Conv3d init_output;
    vector<NNetModule> steps;
    optim::Adam optimizer;
};

class NNetTrail {
   public:
    NNetTrail(NNetGuide* guide, const Tensor& input, const Tensor& output)
        : guide(guide),
          loss(torch::zeros({1}, TensorOptions().requires_grad(true))),
          iter(guide->steps.begin()),
          input_state(guide->init_input->forward(input)),
          output_state(guide->init_output->forward(output)),
          projected_state(torch::zeros({128})) {}

    void next_choice(double* p) {
        auto mod = *iter;
        input_state = mod->conv_input->forward(input_state);
        auto input_sizes = input_state.sizes();
        int input_height = input_sizes.at(2);
        int input_width = input_sizes.at(3);
        auto max_input =
            max_pool3d(input_state, {10, input_height, input_width}).reshape({256});
        // cout << "INPUT SIZES: " << max_input.sizes() << endl;
        auto projected_input = mod->project_input->forward(max_input);

        output_state = mod->conv_output->forward(output_state);
        auto output_sizes = output_state.sizes();
        int output_height = output_sizes.at(2);
        int output_width = output_sizes.at(3);
        auto max_output =
            max_pool3d(output_state, {10, output_height, output_width}).reshape({256});
        // cout << "OUTPUT SIZES: " << max_output.sizes() << endl;
        auto projected_output = mod->project_output->forward(max_output);

        auto projected = torch::cat({projected_input, projected_output});
        // cout << "PROJECTED SIZES: " << projected.sizes() << endl;
        projected_state = projected_state + projected;
        dist_state = mod->decode->forward(projected_state).softmax(0);
        float* dist_values = (float*)dist_state.cpu().const_data_ptr();
        for (int i = 0; i < dist_state.sizes().at(0); i++) {
            p[i] = (double)dist_values[i];
        }
    }

    void observe(int choice) {
        if (choice >= 0) {
            Tensor target = torch::zeros(dist_state.sizes());
            target[choice] = 1.0;
            projected_state = projected_state + (*iter)->encode->forward(target);
            // cout << "adding loss " << endl;
            loss = loss + binary_cross_entropy(dist_state, target).set_requires_grad(true);
        }
        ++iter;
    }

    void train() {
        loss.backward();
        guide->step();
    }

   private:
    NNetGuide* guide;
    vector<NNetModule>::iterator iter;
    Tensor input_state;
    Tensor output_state;
    Tensor projected_state;
    Tensor dist_state;
    Tensor loss;
};

class NNetBuilder {
   public:
    NNetBuilder() : choices() {}

    void add_choice(unsigned int n_choices, const string& name) {
        choices.push_back(NNetModule(n_choices, name));
    }

    NNetGuide* build() {
        NNetGuide* guide = new NNetGuide(choices);
        return guide;
    }

   private:
    vector<NNetModule> choices;
};

extern "C" {

guide_net_builder_t create_network() { return new NNetBuilder(); }

void add_choice_to_net(guide_net_builder_t net, int n_choices, const char* name) {
    NNetBuilder* builder = static_cast<NNetBuilder*>(net);
    builder->add_choice(n_choices, name);
}

guide_net_t build_network(guide_net_builder_t net) {
    NNetBuilder* builder = static_cast<NNetBuilder*>(net);
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
    NNetGuide* guide = static_cast<NNetGuide*>(c_guide);

    // copy the input data by creating a 3d representation (each color has a depth)
    int input_size = (input_width + 2) * (input_height + 2);
    float* input_data = (float*)calloc(10 * input_size, sizeof(float));
    for (int y = 0; y < input_height; y++) {
        for (int x = 0; x < input_width; x++) {
            int input_idx = y * input_width + x;
            int z = input_pixels[input_idx];
            int input_data_idx = (y + 1) * (input_width + 2) + (x + 1);
            assert(input_data_idx < input_size);
            input_data[z * input_size + input_data_idx] = 1.0f;
        }
    }

    // copy the input data by creating a 3d representation (each color has a depth)
    int output_size = (output_width + 2) * (output_height + 2);
    float* output_data = (float*)calloc(10 * output_size, sizeof(float));
    for (int y = 0; y < output_height; y++) {
        for (int x = 0; x < output_width; x++) {
            int output_idx = y * output_width + x;
            int z = output_pixels[output_idx];
            int output_data_idx = (y + 1) * (output_width + 2) + (x + 1);
            assert(output_data_idx < output_size);
            output_data[z * output_size + output_data_idx] = 1.0f;
        }
    }

    Tensor input_tensor = torch::from_blob(
        input_data,
        {1, 10, input_height + 2, input_width + 2},
        [&](void* data) { free(data); },
        TensorOptions().dtype(kFloat));
    Tensor output_tensor = torch::from_blob(
        output_data,
        {1, 10, output_height + 2, output_width + 2},
        [&](void* data) { free(data); },
        TensorOptions().dtype(kFloat));
    return new NNetTrail(guide, input_tensor, output_tensor);
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