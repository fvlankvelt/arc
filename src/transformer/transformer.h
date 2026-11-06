#pragma once
#include <torch/torch.h>
#include "encoder.h"
#include "decoder.h"
#include "mask.h"
#include <fstream>

class TransformerImpl : public torch::nn::Module {
public:
    TransformerImpl(int64_t vocab_size, int64_t d_model, int64_t n_heads, 
                   int64_t n_layers, int64_t d_ff, double dropout = 0.1)
        : encoder_(register_module("encoder", Encoder(vocab_size, d_model, n_heads, n_layers, d_ff, dropout))),
          decoder_(register_module("decoder", Decoder(vocab_size, d_model, n_heads, n_layers, d_ff, dropout))),
          projection_(register_module("projection", torch::nn::Linear(d_model, vocab_size))) {}

    torch::Tensor forward(torch::Tensor src, torch::Tensor tgt) {
        auto src_mask = mask::create_padding_mask(src);
        auto tgt_mask = mask::create_combined_mask(tgt);
        
        torch::Tensor enc_output = encoder_->forward(src, src_mask);
        torch::Tensor dec_output = decoder_->forward(tgt, enc_output, tgt_mask, src_mask);
        return projection_->forward(dec_output);
    }

    torch::Tensor compute_loss(torch::Tensor logits, torch::Tensor targets, torch::Tensor pad_mask) {
        auto loss_fn = torch::nn::CrossEntropyLoss();
        auto loss = loss_fn->forward(logits.reshape({-1, logits.size(-1)}), 
                                   targets.reshape({-1}));
        return loss;
    }

    std::tuple<torch::Tensor, float> train_step(
        torch::Tensor src, torch::Tensor tgt, torch::Tensor tgt_y,
        torch::optim::Optimizer& optimizer) {
        
        optimizer.zero_grad();
        
        auto logits = this->forward(src, tgt);
        
        auto pad_mask = (tgt_y != 0);
        auto loss = compute_loss(logits, tgt_y, pad_mask);
        
        loss.backward();
        
        torch::nn::utils::clip_grad_norm_(this->parameters(), 1.0);
        
        optimizer.step();
        
        return std::make_tuple(logits, loss.item<float>());
    }

    void save(const std::string& path) {
        torch::save(encoder_, path + ".encoder");
        torch::save(decoder_, path + ".decoder");
        torch::save(projection_, path + ".projection");
    }

    void load(const std::string& path) {
        torch::load(encoder_, path + ".encoder");
        torch::load(decoder_, path + ".decoder");
        torch::load(projection_, path + ".projection");
    }

    std::tuple<torch::Tensor, float> validate_step(
        torch::Tensor src, torch::Tensor tgt, torch::Tensor tgt_y) {
        torch::NoGradGuard no_grad;
        
        auto logits = this->forward(src, tgt);
        auto pad_mask = (tgt_y != 0);
        auto loss = compute_loss(logits, tgt_y, pad_mask);
        
        return std::make_tuple(logits, loss.item<float>());
    }

    torch::Tensor generate(torch::Tensor src, int64_t max_length = 100) {
        torch::NoGradGuard no_grad;
        
        auto src_mask = mask::create_padding_mask(src);
        torch::Tensor enc_output = encoder_->forward(src, src_mask);
        
        auto batch_size = src.size(0);
        auto device = src.device();
        torch::Tensor tgt = torch::zeros({batch_size, 1}, torch::kLong).to(device);
        
        for (int64_t i = 0; i < max_length - 1; ++i) {
            auto tgt_mask = mask::create_combined_mask(tgt);
            
            auto dec_output = decoder_->forward(tgt, enc_output, tgt_mask, src_mask);
            auto output = projection_->forward(dec_output);
            
            auto next_token = output.select(1, -1).argmax(-1, true);
            tgt = torch::cat({tgt, next_token}, 1);
            
            if ((next_token == 2).all().item<bool>()) {
                break;
            }
        }
        
        return tgt;
    }

    torch::Tensor generate_beam(torch::Tensor src, int64_t beam_size = 5, int64_t max_length = 100) {
        torch::NoGradGuard no_grad;
        
        auto batch_size = src.size(0);
        auto device = src.device();

        auto src_mask = mask::create_padding_mask(src);
        torch::Tensor enc_output = encoder_->forward(src, src_mask);
        
        enc_output = enc_output.repeat({beam_size, 1, 1});
        src_mask = src_mask.repeat({beam_size, 1, 1, 1});
        
        std::vector<torch::Tensor> seqs = {torch::zeros({batch_size * beam_size, 1}, torch::kLong).to(device)};
        std::vector<float> scores = {0.0f};
        
        for (int64_t i = 0; i < max_length - 1; ++i) {
            std::vector<torch::Tensor> candidates;
            std::vector<float> candidate_scores;
            
            for (size_t j = 0; j < seqs.size(); ++j) {
                auto seq = seqs[j];
                auto score = scores[j];
                
                auto tgt_mask = mask::create_combined_mask(seq);
                auto dec_output = decoder_->forward(seq, enc_output, tgt_mask, src_mask);
                auto output = projection_->forward(dec_output);
                auto probs = torch::log_softmax(output.select(1, -1), -1);
                
                auto [values, indices] = probs.topk(beam_size);
                
                for (int64_t k = 0; k < beam_size; ++k) {
                    auto new_seq = torch::cat({seq, indices.select(1, k).unsqueeze(1)}, 1);
                    candidates.push_back(new_seq);
                    candidate_scores.push_back(score + values.select(1, k).item().toFloat());
                }
            }
            
            std::vector<size_t> indices(candidate_scores.size());
            std::iota(indices.begin(), indices.end(), 0);
            std::partial_sort(indices.begin(), indices.begin() + beam_size, indices.end(),
                            [&candidate_scores](size_t i1, size_t i2) {
                                return candidate_scores[i1] > candidate_scores[i2];
                            });
            
            seqs.clear();
            scores.clear();
            for (int64_t k = 0; k < beam_size; ++k) {
                seqs.push_back(candidates[indices[k]]);
                scores.push_back(candidate_scores[indices[k]]);
            }
            
            bool all_ended = true;
            for (const auto& seq : seqs) {
                if (seq.select(1, -1).item<int64_t>() != 2) {  // 假设2是结束标记
                    all_ended = false;
                    break;
                }
            }
            if (all_ended) break;
        }
        
        return seqs[0];
    }

private:
    Encoder encoder_;
    Decoder decoder_;
    torch::nn::Linear projection_;
};

TORCH_MODULE(Transformer); 