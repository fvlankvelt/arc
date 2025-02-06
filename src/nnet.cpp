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

struct NNetConfig {
    int n_conv_channels;
    int projected_width;
};

struct NNetState {
    Tensor input_state;
    Tensor output_state;
    Tensor projected_state;
    Tensor dist_state;
    Tensor loss;
};

struct NNetModuleImpl : nn::Module {
    NNetModuleImpl(const NNetConfig& config, int n_choices, const string& name)
        : config(config),
          name(name),
          conv_input(register_module(
              "conv_input",
              nn::Conv3d(
                  nn::Conv3dOptions(config.n_conv_channels, config.n_conv_channels, {1, 3, 3})
                      .padding({0, 1, 1})))),
          conv_output(register_module(
              "conv_output",
              nn::Conv3d(
                  nn::Conv3dOptions(config.n_conv_channels, config.n_conv_channels, {1, 3, 3})
                      .padding({0, 1, 1})))),
          conv_input_color(register_module(
              "conv_input_color",
              nn::Conv3d(nn::Conv3dOptions(
                  config.n_conv_channels, config.n_conv_channels, {1, 1, 1})))),
          conv_output_color(register_module(
              "conv_output_color",
              nn::Conv3d(nn::Conv3dOptions(
                  config.n_conv_channels, config.n_conv_channels, {1, 1, 1})))),
          project_input(register_module(
              "project_input", nn::Linear(config.n_conv_channels, config.projected_width / 2))),
          project_output(register_module(
              "project_output",
              nn::Linear(config.n_conv_channels, config.projected_width / 2))),
          decode(register_module("decode", nn::Linear(config.projected_width, n_choices))),
          encode(register_module("encode", nn::Linear(n_choices, config.projected_width))) {}

    NNetState forward(NNetState& state) {
        Tensor input_state = torch::relu(conv_input->forward(state.input_state));
        auto input_sizes = input_state.sizes();
        int input_height = input_sizes.at(2);
        int input_width = input_sizes.at(3);
        auto max_input_color =
            max_pool3d(input_state, {10, 1, 1})
                .expand({config.n_conv_channels, 10, input_height, input_width});
        input_state = input_state + conv_input_color->forward(max_input_color);
        auto max_input = max_pool3d(input_state, {10, input_height, input_width})
                             .reshape({config.n_conv_channels});
        // cout << "INPUT SIZES: " << max_input.sizes() << endl;
        auto projected_input = torch::relu(project_input->forward(max_input));

        Tensor output_state = torch::relu(conv_output->forward(state.output_state));
        auto output_sizes = output_state.sizes();
        int output_height = output_sizes.at(2);
        int output_width = output_sizes.at(3);
        auto max_output_color =
            max_pool3d(output_state, {10, 1, 1})
                .expand({config.n_conv_channels, 10, output_height, output_width});
        output_state = output_state + conv_output_color->forward(max_output_color);
        auto max_output = max_pool3d(output_state, {10, output_height, output_width})
                              .reshape({config.n_conv_channels});
        // cout << "OUTPUT SIZES: " << max_output.sizes() << endl;
        auto projected_output = torch::relu(project_output->forward(max_output));

        auto projected = torch::cat({projected_input, projected_output});
        // cout << "PROJECTED SIZES: " << projected.sizes() << endl;
        Tensor projected_state = state.projected_state + projected;
        Tensor dist_state = decode->forward(projected_state);

        return {input_state, output_state, projected_state, dist_state, state.loss};
    }

    NNetState observe(NNetState& state, int choice) {
        Tensor target = torch::zeros(state.dist_state.sizes());
        target[choice] = 1.0;
        Tensor cuda_target = target.cuda();
        Tensor projected_state = state.projected_state + encode->forward(cuda_target);
        // cout << "adding loss " << endl;
        Tensor loss =
            state.loss + cross_entropy_loss(state.dist_state, cuda_target).set_requires_grad(true);
        return {state.input_state, state.output_state, projected_state, state.dist_state, loss};
    }

    NNetConfig config;
    string name;
    nn::Conv3d conv_input;
    nn::Conv3d conv_output;
    nn::Conv3d conv_input_color;
    nn::Conv3d conv_output_color;
    nn::Linear project_input;
    nn::Linear project_output;
    nn::Sequential decode;
    nn::Sequential encode;
};

TORCH_MODULE(NNetModule);

class NNetGuide : nn::Module {
    friend class NNetTrail;

   public:
    NNetGuide(NNetConfig& config, vector<NNetModule>& steps)
        : config(config),
          init_input(register_module(
              "init_input",
              nn::Conv3d(
                  nn::Conv3dOptions(1, config.n_conv_channels, {1, 3, 3}).padding({0, 1, 1})))),
          init_output(register_module(
              "init_output",
              nn::Conv3d(
                  nn::Conv3dOptions(1, config.n_conv_channels, {1, 3, 3}).padding({0, 1, 1})))),
          steps(steps),
          optimizer(std::vector<optim::OptimizerParamGroup>()) {
        int index = 0;
        for (auto& step : steps) {
            std::string name = "choice_" + std::to_string(index++);
            register_module(name, step);
        }
        to(kCUDA);
        optimizer.add_param_group(parameters());
    }

    NNetState new_state(const Tensor& input, const Tensor& output) {
        return {
            init_input->forward(input.cuda()),
            init_output->forward(output.cuda()),
            torch::zeros({config.projected_width}).cuda(),
            torch::zeros({0}).cuda(),
            torch::zeros({1}, TensorOptions().requires_grad(true)).cuda(),
        };
    }

   private:
    void step() {
        optimizer.step();
        optimizer.zero_grad(true);
    }

    NNetConfig config;
    // initial transformation from image to the internal (variable image) dimensions
    nn::Conv3d init_input;
    nn::Conv3d init_output;
    vector<NNetModule> steps;
    optim::Adam optimizer;
};

class NNetTrail {
   public:
    NNetTrail(NNetGuide* guide, const Tensor& input, const Tensor& output)
        : guide(guide), iter(guide->steps.begin()), state(guide->new_state(input, output)) {}

    void next_choice(double* p) {
        state = (*iter)->forward(state);
        auto soft_dist = state.dist_state.softmax(0).to(kFloat64).cpu();
        double* dist_values = (double*)soft_dist.data_ptr();
        for (int i = 0; i < soft_dist.sizes().at(0); i++) {
            p[i] = dist_values[i];
        }
    }

    void observe(int choice) {
        if (choice >= 0) {
            state = (*iter)->observe(state, choice);
        }
        ++iter;
    }

    float train() {
        double loss_value = state.loss.item().toDouble();
        // cout << "loss: " << loss_value[0] << endl;
        state.loss.backward();
        guide->step();
        return loss_value;
    }

   private:
    NNetGuide* guide;
    vector<NNetModule>::iterator iter;
    NNetState state;
};

class NNetBuilder {
   public:
    NNetBuilder() : steps() {}

    NNetBuilder& n_conv_channels(int n_channels) {
        config.n_conv_channels = n_channels;
        return *this;
    }

    NNetBuilder& projected_width(int width) {
        config.projected_width = width;
        return *this;
    }

    void add_choice(unsigned int n_choices, const string& name) {
        steps.push_back(NNetModule(config, n_choices, name));
    }

    NNetGuide* build() {
        NNetGuide* guide = new NNetGuide(config, steps);
        return guide;
    }

   private:
    NNetConfig config;
    vector<NNetModule> steps;
};

extern "C" {

guide_net_builder_t create_network() {
    NNetBuilder* builder = new NNetBuilder();
    builder->n_conv_channels(256).projected_width(128);
    return builder;
}

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

float complete_trail(trail_net_t c_trail, bool success) {
    NNetTrail* trail = static_cast<NNetTrail*>(c_trail);
    float result = 0.0f;
    if (success) {
        result = trail->train();
    }
    delete trail;
    return result;
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