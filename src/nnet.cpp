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
 * For dealing with varying input/output sizes, we leverage the transformer architecture.
 * Each pixel is transformed in Query (size_key), Key (size_key) and Value (size_value).
 * This attention triple is layered a couple of times for ultimate flexibility.
 *
 * The transformer is initialized by a convolutional layer that takes the 10 colors as
 * different features.
 *
 * Color permutation symmetry
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
    double batch_norm_momentum;
    int n_prepare;
    int n_conv_channels;
    int n_pos_dim;
    // int k;
    // int v;

    int size() const { return n_pos_dim * n_pos_dim + n_conv_channels; }
};

struct DecoderState {
    Tensor context;
    Tensor keys;
    Tensor values;
    Tensor dist_state;
    Tensor loss;
};

/**
 * Embed input & output images
 * Input, output are 2D images with each color in a separate "feature"
 * each pixel gets a 2D position and is embedded in feature space by a Convolutional layer
 * input/output pixels get differentiated by a separate "position" dimension.
 *
 * The positional embedding makes sure that nearby pixels can find each other as (query, key)
 * inner products.
 */
struct EmbeddingImpl : nn::Module {
    EmbeddingImpl(const NNetConfig &config)
        : config(config),
          conv_input(register_module(
              "conv_input",
              nn::Conv2d(
                  nn::Conv2dOptions(10, config.n_conv_channels, {3, 3}).padding({1, 1})))),
          conv_output(register_module(
              "conv_output",
              nn::Conv2d(
                  nn::Conv2dOptions(10, config.n_conv_channels, {3, 3}).padding({1, 1})))) {}

    Tensor forward(const Tensor &input, const Tensor &output) {
        Tensor input_pos = embed_position(input); // 65536, 7, 7
        Tensor input_feat = conv_input->forward(input); // 128, 7, 7
        // cout << "pos: " << input_pos.sizes() << ", feat: " << input_feat.sizes() << endl;
        Tensor input_flat = flatten({cat({input_pos, input_feat}, 0)}, 1);
        Tensor output_pos = embed_position(output);
        Tensor output_feat = conv_output->forward(output);
        Tensor output_flat = flatten({cat({output_pos, output_feat}, 0)}, 1);
        return transpose(cat({input_flat, output_flat}, 1), 0, 1);
    }

   private:
    Tensor embed_position(const Tensor &input) {
        auto input_sizes = input.sizes();
        int input_height = input_sizes.at(1);
        int input_width = input_sizes.at(2);

        // scale position to be in (0, 2 * pi) range
        // (input_width)
        Tensor input_hor = (at::arange(input_width) + 0.5f) * 2 * M_PI_2f32 / input_width;
        // (input_width, n_pos_dim / 2)
        Tensor input_hor_fourier = input_hor.outer(at::arange(config.n_pos_dim / 2));
        // (input_width, n_pos_dim)
        Tensor input_hor_emb =
            cat({input_hor_fourier.cos(), input_hor_fourier.sin()}, 1).cuda();

        // (input_height)
        Tensor input_ver = (at::arange(input_height) + 0.5f) * 2 * M_PI_2f32 / input_height;
        // (input_height, n_pos_dim / 2)
        Tensor input_ver_fourier = input_ver.outer(at::arange(config.n_pos_dim / 2));
        // (input_height, n_pos_dim)
        Tensor input_ver_emb =
            cat({input_ver_fourier.cos(), input_ver_fourier.sin()}, 1).cuda();

        // embedding is of size 4 * n_pos_dim^2
        // (4 * n_pos_dim^2, input_height, input_width)
        return flatten(einsum("ik,jl->klij", {input_ver_emb, input_hor_emb}), 0, 1);
    }

    NNetConfig config;
    nn::Conv2d conv_input;
    nn::Conv2d conv_output;
};

TORCH_MODULE(Embedding);

struct EncoderStepImpl : nn::Module {
    EncoderStepImpl(const NNetConfig &config)
        : qkv(register_parameter("self_attn", torch::rand({config.size(), 3 * config.size()}))),
          out_proj(register_module("out_proj", nn::Linear(config.size(), config.size()))) {}

    Tensor forward(const Tensor &state) {
        auto proj = chunk(matmul(state, qkv), 3, 1);
        return out_proj->forward(
            scaled_dot_product_attention(proj[0], proj[1], proj[2]));
    }

   private:
    Tensor qkv;
    nn::Linear out_proj;
};

TORCH_MODULE(EncoderStep);

/**
 * Decoder with attention.
 * Each choice is added as (key, value) pair to the decoder state.
 * When a new distribution is needed, the attention mechanism is used to attend to
 * - previous choices (decoder self-attention)
 * - context; the encoded state (cross attention)
 */
struct DecoderStepImpl : nn::Module {
    DecoderStepImpl(const NNetConfig &config, int n_choices, const std::string &name)
        : config(config),
          name(name),
          query(register_parameter("query", torch::rand({1, config.size()}))),
          decode(register_module("decode", nn::Linear(config.size(), n_choices))),
          cross_key(register_module("cross_key", nn::Linear(config.size(), config.size()))),
          cross_value(register_module("cross_value", nn::Linear(config.size(), config.size()))),
          cross_query(register_module("cross_query", nn::Linear(config.size(), config.size()))),
          self_key(register_module("self_key", nn::Linear(n_choices, config.size()))),
          self_value(register_module("self_value", nn::Linear(n_choices, config.size()))),
          self_project(
              register_module("self_project", nn::Linear(config.size(), config.size()))) {}

    DecoderState forward(DecoderState &state) {
        // cout << "query: " << query.sizes() << ", key: " << state.keys.sizes() << ", values: " << state.values.sizes() << endl;
        auto self_query = self_project->forward(
            scaled_dot_product_attention(query, state.keys, state.values));

        Tensor dist_state = decode->forward(
            scaled_dot_product_attention(
                cross_query->forward(self_query),
                cross_key->forward(state.context),
                cross_value->forward(state.context)));

        // cout << name << " => dist_state: " << dist_state.sizes() << endl;

        return {
            state.context,
            state.keys,
            state.values,
            dist_state,
            state.loss,
        };
    }

    DecoderState observe(DecoderState &state, int choice) {
        Tensor target = torch::zeros(state.dist_state.sizes());
        target.index({0, choice}) = 1.0;
        Tensor cuda_target = target.cuda();
        // cout << "adding loss " << endl;
        Tensor loss = state.loss +
                      cross_entropy_loss(state.dist_state, cuda_target).set_requires_grad(true);

        Tensor key = self_key->forward(cuda_target);
        Tensor value = self_value->forward(cuda_target);

        // cout << name << " => state.keys: " << state.keys.sizes() << ", key: " << key.sizes() << endl;
        return {
            state.context,
            cat({state.keys, key}),
            cat({state.values, value}),
            state.dist_state,
            loss,
        };
    }

    NNetConfig config;
    std::string name;
    Tensor query;
    nn::Sequential decode;
    nn::Sequential self_key;
    nn::Sequential self_value;
    nn::Sequential self_project;
    nn::Sequential cross_key;
    nn::Sequential cross_value;
    nn::Sequential cross_query;
};

TORCH_MODULE(DecoderStep);

class NNetGuide : nn::Module {
    friend class NNetTrail;

   public:
    NNetGuide(NNetConfig &config, vector<DecoderStep> &decoder)
        : config(config),
          embedding(register_module("embedding", Embedding(config))),
          encoder_steps(),
          decoder_steps(decoder),
          optimizer(
              std::vector<optim::OptimizerParamGroup>(), optim::AdamWOptions(1e-5).amsgrad(true)),
          scheduler(optimizer, 10000, 0.5),
          minibatch_size(0),
          loss(torch::zeros({1}, TensorOptions().requires_grad(true)).cuda()) {
        int index = 0;
        for (auto &step : decoder) {
            std::string name = "choice_" + std::to_string(index++);
            register_module(name, step);
        }
        for (index = 0; index < config.n_prepare; index++) {
            std::string name = "prepare_" + std::to_string(index++);
            encoder_steps.push_back(register_module(name, EncoderStep(config)));
        }
        to(kCUDA);
        optimizer.add_param_group(parameters());
    }

    DecoderState encode(const Tensor &input, const Tensor &output) {
        Tensor state = embedding->forward(input, output);
        for (auto &prep : encoder_steps) {
            state = prep->forward(state);
        }

        return {
            state,
            torch::zeros({1, config.size()}).cuda(),
            torch::zeros({1, config.size()}).cuda(),
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
    Embedding embedding;
    vector<EncoderStep> encoder_steps;
    vector<DecoderStep> decoder_steps;

    optim::AdamW optimizer;
    optim::StepLR scheduler;

    // dynamically built up mini-batch
    int minibatch_size;
    Tensor loss;
};

class NNetTrail {
   public:
    NNetTrail(NNetGuide *guide, const Tensor &input, const Tensor &output)
        : guide(guide),
          iter(guide->decoder_steps.begin()),
          state(guide->encode(input, output)) {}

    void next_choice(double *p) {
        state = (*iter)->forward(state);
        auto soft_dist = state.dist_state.softmax(0).to(kFloat64).cpu();
        // cout << "  choice: " << soft_dist.sizes() << endl;
        double *dist_values = (double *)soft_dist.data_ptr();
        for (int i = 0; i < soft_dist.sizes().at(1); i++) {
            p[i] = dist_values[i];
        }
    }

    void observe(int choice) {
        state = (*iter)->observe(state, choice);
        ++iter;
    }

    void skip_choice() { ++iter; }

    float train() {
        double loss_value = state.loss.item().toDouble();
        // cout << "loss: " << loss_value << endl;
        guide->train(state.loss);
        return loss_value;
    }

   private:
    NNetGuide *guide;
    vector<DecoderStep>::iterator iter;
    DecoderState state;
};

class NNetBuilder {
   public:
    NNetBuilder() : decoder() {}

    NNetBuilder &batch_norm_momentum(double batch_norm_momentum) {
        config.batch_norm_momentum = batch_norm_momentum;
        return *this;
    }
    NNetBuilder &n_prepare(int n_prepare) {
        config.n_prepare = n_prepare;
        return *this;
    }

    NNetBuilder &n_conv_channels(int n_conv_channels) {
        config.n_conv_channels = n_conv_channels;
        return *this;
    }
    NNetBuilder &n_pos_dim(int n_pos_dim) {
        config.n_pos_dim = n_pos_dim;
        return *this;
    }

    void add_choice(unsigned int n_choices, const string &name) {
        decoder.push_back(DecoderStep(config, n_choices, name));
    }

    NNetGuide *build() {
        NNetGuide *guide = new NNetGuide(config, decoder);
        return guide;
    }

   private:
    NNetConfig config;
    vector<DecoderStep> decoder;
};

extern "C" {

guide_net_builder_t create_network() {
    NNetBuilder *builder = new NNetBuilder();
    builder->n_conv_channels(128).n_pos_dim(32).n_prepare(10).batch_norm_momentum(0.01);
    return builder;
}

void add_choice_to_net(guide_net_builder_t net, int n_choices, const char *name) {
    NNetBuilder *builder = static_cast<NNetBuilder *>(net);
    builder->add_choice(n_choices, name);
}

guide_net_t build_network(guide_net_builder_t net) {
    NNetBuilder *builder = static_cast<NNetBuilder *>(net);
    return builder->build();
}

trail_net_t create_network_trail(
    guide_net_t c_guide,
    unsigned int input_width,
    unsigned int input_height,
    unsigned int *input_pixels,
    unsigned int output_width,
    unsigned int output_height,
    unsigned int *output_pixels) {
    NNetGuide *guide = static_cast<NNetGuide *>(c_guide);

    // copy the input data by creating a 3d representation (each color has a depth)
    int input_size = (input_width + 2) * (input_height + 2);
    float *input_data = (float *)calloc(10 * input_size, sizeof(float));
    for (int y = 0; y < input_height; y++) {
        for (int x = 0; x < input_width; x++) {
            int input_idx = y * input_width + x;
            int z = input_pixels[input_idx];
            assert(z >= 0 && z < 10);
            int input_data_idx = (y + 1) * (input_width + 2) + (x + 1);
            assert(input_data_idx < input_size);
            input_data[z * input_size + input_data_idx] = 1.0f;
        }
    }

    // copy the input data by creating a 3d representation (each color has a depth)
    int output_size = (output_width + 2) * (output_height + 2);
    float *output_data = (float *)calloc(10 * output_size, sizeof(float));
    for (int y = 0; y < output_height; y++) {
        for (int x = 0; x < output_width; x++) {
            int output_idx = y * output_width + x;
            int z = output_pixels[output_idx];
            assert(z >= 0 && z < 10);
            int output_data_idx = (y + 1) * (output_width + 2) + (x + 1);
            assert(output_data_idx < output_size);
            output_data[z * output_size + output_data_idx] = 1.0f;
        }
    }

    Tensor input_tensor = torch::from_blob(
        input_data,
        {10, input_height + 2, input_width + 2},
        [&](void *data) { free(data); },
        TensorOptions().dtype(kFloat)).cuda();
    Tensor output_tensor = torch::from_blob(
        output_data,
        {10, output_height + 2, output_width + 2},
        [&](void *data) { free(data); },
        TensorOptions().dtype(kFloat)).cuda();
    return new NNetTrail(guide, input_tensor, output_tensor);
}

void next_network_choice(trail_net_t c_trail, double *p) {
    NNetTrail *trail = static_cast<NNetTrail *>(c_trail);
    trail->next_choice(p);
}

trail_net_t skip_network_choice(trail_net_t c_trail) {
    NNetTrail *trail = static_cast<NNetTrail *>(c_trail);
    trail->skip_choice();
    return trail;
}

trail_net_t observe_network_choice(trail_net_t c_trail, int choice) {
    NNetTrail *trail = static_cast<NNetTrail *>(c_trail);
    trail->observe(choice);
    return trail;
}

float complete_trail(trail_net_t c_trail, bool success) {
    NNetTrail *trail = static_cast<NNetTrail *>(c_trail);
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