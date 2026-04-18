#pragma once
#include "replacement_policy.hpp"
#include <random>

// Random replacement policy
// Evicts a randomly selected cache line — simple but surprisingly competitive
class RandomPolicy : public ReplacementPolicy {
public:
    RandomPolicy(int num_sets, int associativity, uint64_t seed = 42)
        : num_sets_(num_sets), assoc_(associativity),
          rng_(seed), dist_(0, associativity - 1) {}

    void on_access(int, int, uint64_t) override {}

    void on_insert(int, int, uint64_t) override {}

    int get_evict_way(int /*set_index*/) override {
        return dist_(rng_);
    }

    void reset() override {
        rng_.seed(42); // Reset to deterministic state
    }

    std::string name() const override { return "Random"; }

private:
    int num_sets_, assoc_;
    std::mt19937_64 rng_;
    std::uniform_int_distribution<int> dist_;
};
