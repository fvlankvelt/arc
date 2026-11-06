#pragma once
#include <torch/torch.h>
#include <vector>
#include <random>
#include <stdexcept>

class TransformerDataset : public torch::data::Dataset<TransformerDataset> {
public:
    TransformerDataset(const std::vector<std::vector<int64_t>>& src_data,
                      const std::vector<std::vector<int64_t>>& tgt_data,
                      int64_t max_length = 100,
                      int64_t pad_idx = 0) 
        : src_data_(src_data), tgt_data_(tgt_data), 
          max_length_(max_length), pad_idx_(pad_idx) {
        if (src_data.size() != tgt_data.size()) {
            throw std::runtime_error("Source and target data size mismatch");
        }
        size_ = src_data.size();
    }

    torch::data::Example<> get(size_t index) override {
        std::vector<int64_t> src = src_data_[index];
        std::vector<int64_t> tgt = tgt_data_[index];

        if (src.size() > max_length_) src.resize(max_length_);
        if (tgt.size() > max_length_) tgt.resize(max_length_);

        src = pad_sequence(src);
        tgt = pad_sequence(tgt);

        torch::Tensor src_tensor = torch::from_blob(src.data(), {max_length_}, torch::kLong).clone();
        torch::Tensor tgt_tensor = torch::from_blob(tgt.data(), {max_length_}, torch::kLong).clone();
        
        return {src_tensor, tgt_tensor};
    }

    torch::optional<size_t> size() const override {
        return size_;
    }

    static TransformerDataset create_test_dataset(
        int64_t vocab_size,
        size_t num_samples = 100,
        int64_t seq_length = 10
    ) {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<int64_t> dis(1, vocab_size - 1);

        std::vector<std::vector<int64_t>> src_data;
        std::vector<std::vector<int64_t>> tgt_data;

        for (size_t i = 0; i < num_samples; ++i) {
            std::vector<int64_t> src(seq_length);
            std::vector<int64_t> tgt(seq_length);
            for (int64_t j = 0; j < seq_length; ++j) {
                src[j] = dis(gen);
                tgt[j] = dis(gen);
            }
            src_data.push_back(src);
            tgt_data.push_back(tgt);
        }

        return TransformerDataset(src_data, tgt_data, seq_length);
    }

private:
    std::vector<std::vector<int64_t>> src_data_;
    std::vector<std::vector<int64_t>> tgt_data_;
    size_t size_;
    int64_t max_length_;
    int64_t pad_idx_;

    std::vector<int64_t> pad_sequence(const std::vector<int64_t>& seq) const {
        std::vector<int64_t> padded = seq;
        while (padded.size() < max_length_) {
            padded.push_back(pad_idx_);
        }
        return padded;
    }
}; 