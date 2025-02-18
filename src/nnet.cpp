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
 * The approach to dealing with correlations between samples (variables or other choices when
 * synthesizing the program) is to use attention.
 *
 * The (input,output) pair is encoded for each choice into a Query vector (length k).
 * Each preceding choice is encoded into a Key (length k) and a Value vector (length v).
 * The attention matmul/softmax/matmul mechanism creates a vector (length v) that is then
 * transformed into a distribution.
 */

class NNetTrail;

struct NNetConfig {
    int n_conv_channels;
    int k;
    int v;
};

struct NNetState {
    Tensor observations;
    Tensor keys;
    Tensor values;
    Tensor dist_state;
    Tensor loss;
};

struct NNetPrepareState {
    Tensor input;
    Tensor output;
};

/**
 * evolve image in a color-permutation symmetric way
 * The kernels used are shared across colors
 */
struct NNetImageStepImpl : nn::Module {
    NNetImageStepImpl(const NNetConfig& config)
        : config(config),
          batchnorm(register_module(
              "batchnorm",
              nn::BatchNorm3d(
                  nn::BatchNormOptions(config.n_conv_channels).affine(false).momentum(0.99)))),
          conv(register_module(
              "conv",
              nn::Conv3d(
                  nn::Conv3dOptions(config.n_conv_channels, config.n_conv_channels, {1, 3, 3})
                      .padding({0, 1, 1})))),
          batchnorm_color(register_module(
              "batchnorm_color",
              nn::BatchNorm3d(
                  nn::BatchNormOptions(config.n_conv_channels).affine(false).momentum(0.99)))),
          conv_color(register_module(
              "conv_color",
              nn::Conv3d(nn::Conv3dOptions(
                  config.n_conv_channels, config.n_conv_channels, {1, 1, 1})))),
          batchnorm_horizontal(register_module(
              "batchnorm_horizontal",
              nn::BatchNorm3d(
                  nn::BatchNormOptions(config.n_conv_channels).affine(false).momentum(0.99)))),
          conv_horizontal(register_module(
              "conv_horizontal",
              nn::Conv3d(nn::Conv3dOptions(
                  config.n_conv_channels, config.n_conv_channels, {1, 1, 1})))),
          batchnorm_vertical(register_module(
              "batchnorm_vertical",
              nn::BatchNorm3d(
                  nn::BatchNormOptions(config.n_conv_channels).affine(false).momentum(0.99)))),
          conv_vertical(register_module(
              "conv_vertical",
              nn::Conv3d(nn::Conv3dOptions(
                  config.n_conv_channels, config.n_conv_channels, {1, 1, 1})))),
          conv_peer(register_module(
              "conv_peer",
              nn::Conv3d(nn::Conv3dOptions(
                  config.n_conv_channels, config.n_conv_channels, {1, 1, 1})))) {}

    Tensor forward(const Tensor& input) {
        auto input_sizes = input.sizes();
        int input_height = input_sizes.at(3);
        int input_width = input_sizes.at(4);

        Tensor input_state = conv->forward(torch::relu(batchnorm->forward(input)));

        // compute the max value per (channel, x, y) over all colors
        // add affine transform back from the original
        auto project_color =
            input_state +
            conv_color
                ->forward(
                    torch::relu(batchnorm_color->forward(max_pool3d(input_state, {10, 1, 1}))))
                .expand({1, config.n_conv_channels, 10, input_height, input_width});

        // compute max value per (channel, color, y) over width of image
        // add affine transform back to the original
        auto project_horizontal =
            project_color +
            conv_horizontal
                ->forward(torch::relu(batchnorm_horizontal->forward(
                    max_pool3d(project_color, {1, 1, input_width}))))
                .expand({1, config.n_conv_channels, 10, input_height, input_width});

        // compute max value per (channel, color, x) over height of image
        // add affine transform back to the original
        auto project_vertical =
            project_horizontal +
            conv_vertical
                ->forward(torch::relu(batchnorm_vertical->forward(
                    max_pool3d(project_horizontal, {1, input_height, 1}))))
                .expand({1, config.n_conv_channels, 10, input_height, input_width});

        return project_vertical;
    }

    Tensor merge(const Tensor& input, const Tensor& peer) {
        auto peer_sizes = peer.sizes();
        int peer_height = peer_sizes.at(3);
        int peer_width = peer_sizes.at(4);
        auto channels =
            conv_peer->forward(torch::relu(max_pool3d(peer, {10, peer_height, peer_width})));

        auto input_sizes = input.sizes();
        int input_height = input_sizes.at(3);
        int input_width = input_sizes.at(4);
        return input +
               channels.expand({1, config.n_conv_channels, 10, input_height, input_width});
    }

   private:
    NNetConfig config;
    nn::BatchNorm3d batchnorm;
    nn::Conv3d conv;
    nn::BatchNorm3d batchnorm_color;
    nn::Conv3d conv_color;
    nn::BatchNorm3d batchnorm_horizontal;
    nn::Conv3d conv_horizontal;
    nn::BatchNorm3d batchnorm_vertical;
    nn::Conv3d conv_vertical;
    nn::Conv3d conv_peer;
};

TORCH_MODULE(NNetImageStep);

struct NNetPrepareModuleImpl : nn::Module {
    NNetPrepareModuleImpl(const NNetConfig& config)
        : input_step(register_module("input_step", NNetImageStep(config))),
          output_step(register_module("output_step", NNetImageStep(config))) {}

    NNetPrepareState forward(const NNetPrepareState& state) {
        auto tmp_input = input_step->forward(state.input);
        auto tmp_output = output_step->forward(state.output);
        return {
            input_step->merge(tmp_input, tmp_output),
            output_step->merge(tmp_output, tmp_input),
        };
    }

   private:
    NNetImageStep input_step;
    NNetImageStep output_step;
};

TORCH_MODULE(NNetPrepareModule);

struct NNetModuleImpl : nn::Module {
    NNetModuleImpl(const NNetConfig& config, int n_choices, const std::string& name)
        : config(config),
          name(name),
          project(register_module("project", nn::Linear(2 * config.n_conv_channels, config.k))),
          decode(register_module("decode", nn::Linear(config.v, n_choices))),
          encode_key(register_module("encode_key", nn::Linear(n_choices, config.k))),
          encode_value(register_module("encode_value", nn::Linear(n_choices, config.v))) {}

    NNetState forward(NNetState& state) {
        auto query = torch::relu(project->forward(state.observations));

        // compute weight of each preceding choice, add their corresponding values
        auto weights = torch::mv(state.keys, query).softmax(0);
        auto value = torch::mv(state.values.t(), weights);

        Tensor dist_state = decode->forward(value);

        return {
            state.observations,
            state.keys,
            state.values,
            dist_state,
            state.loss,
        };
    }

    NNetState observe(NNetState& state, int choice) {
        Tensor target = torch::zeros(state.dist_state.sizes());
        target[choice] = 1.0;
        Tensor cuda_target = target.cuda();
        // cout << "adding loss " << endl;
        Tensor loss = state.loss +
                      cross_entropy_loss(state.dist_state, cuda_target).set_requires_grad(true);

        Tensor key = encode_key->forward(cuda_target).unsqueeze(0);
        Tensor value = encode_value->forward(cuda_target).unsqueeze(0);

        return {
            state.observations,
            torch::cat({state.keys, key}),
            torch::cat({state.values, value}),
            state.dist_state,
            loss,
        };
    }

    NNetConfig config;
    std::string name;
    nn::Sequential project;
    nn::Sequential decode;
    nn::Sequential encode_key;
    nn::Sequential encode_value;
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
          prepare1(register_module("prepare1", NNetPrepareModule(config))),
          prepare2(register_module("prepare2", NNetPrepareModule(config))),
          steps(steps),
          optimizer(
              std::vector<optim::OptimizerParamGroup>(), optim::AdamWOptions().amsgrad(true)),
          scheduler(optimizer, 1000, 0.9),
          minibatch_size(0),
          loss(torch::zeros({1}, TensorOptions().requires_grad(true)).cuda()) {
        int index = 0;
        for (auto& step : steps) {
            std::string name = "choice_" + std::to_string(index++);
            register_module(name, step);
        }
        to(kCUDA);
        optimizer.add_param_group(parameters());
    }

    NNetState new_state(const Tensor& input, const Tensor& output) {
        Tensor tmp_input = init_input->forward(input.cuda());
        Tensor tmp_output = init_output->forward(output.cuda());
        auto prep1 = prepare1->forward({tmp_input, tmp_output});
        auto prep2 = prepare2->forward(prep1);

        auto input_sizes = input.sizes();
        int input_height = input_sizes.at(3);
        int input_width = input_sizes.at(4);
        auto max_input = max_pool3d(prep2.input, {10, input_height, input_width})
                             .reshape({config.n_conv_channels});

        auto output_sizes = output.sizes();
        int output_height = output_sizes.at(3);
        int output_width = output_sizes.at(4);
        auto max_output = max_pool3d(prep2.output, {10, output_height, output_width})
                              .reshape({config.n_conv_channels});
        auto observations = torch::cat({max_input, max_output});

        return {
            observations,
            torch::zeros({1, config.k}).cuda(),
            torch::zeros({1, config.v}).cuda(),
            torch::zeros({0}).cuda(),
            torch::zeros({1}, TensorOptions().requires_grad(true)).cuda(),
        };
    }

   private:
    void train(Tensor sample_loss) {
        loss = loss + sample_loss;
        minibatch_size += 1;
        if (minibatch_size == 10) {
            optimizer.zero_grad(true);
            loss.backward();
            optimizer.step();
            scheduler.step();

            minibatch_size = 0;
            loss = torch::zeros({1}, TensorOptions().requires_grad(true)).cuda();
        }
    }

    NNetConfig config;
    // initial transformation from image to the internal (variable image) dimensions
    nn::Conv3d init_input;
    nn::Conv3d init_output;
    NNetPrepareModule prepare1;
    NNetPrepareModule prepare2;

    vector<NNetModule> steps;
    optim::AdamW optimizer;
    optim::StepLR scheduler;

    // dynamically built up mini-batch
    int minibatch_size;
    Tensor loss;
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
        guide->train(state.loss);
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

    NNetBuilder& n_conv_channels(int n_conv_channels) {
        config.n_conv_channels = n_conv_channels;
        return *this;
    }

    NNetBuilder& k(int k) {
        config.k = k;
        return *this;
    }

    NNetBuilder& v(int v) {
        config.v = v;
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
    builder->k(128).v(64).n_conv_channels(512);
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
        {1, 1, 10, input_height + 2, input_width + 2},
        [&](void* data) { free(data); },
        TensorOptions().dtype(kFloat));
    Tensor output_tensor = torch::from_blob(
        output_data,
        {1, 1, 10, output_height + 2, output_width + 2},
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