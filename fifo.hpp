#pragma once
#include "replacement_policy.hpp"
#include <vector>
#include <queue>

// First-In, First-Out (FIFO) replacement policy
// Evicts the cache line that was loaded first, regardless of access pattern
class FIFOPolicy : public ReplacementPolicy {
public:
    FIFOPolicy(int num_sets, int associativity)
        : num_sets_(num_sets), assoc_(associativity),
          insert_order_(num_sets) {}

    void on_access(int /*set_index*/, int /*way*/, uint64_t /*tag*/) override {
        // FIFO doesn't update state on access, only on insert
    }

    void on_insert(int set_index, int way, uint64_t /*tag*/) override {
        insert_order_[set_index].push(way);
    }

    int get_evict_way(int set_index) override {
        auto& q = insert_order_[set_index];
        if (q.empty()) return 0;
        int way = q.front();
        q.pop();
        return way;
    }

    void reset() override {
        for (auto& q : insert_order_) {
            while (!q.empty()) q.pop();
        }
    }

    std::string name() const override { return "FIFO"; }

private:
    int num_sets_, assoc_;
    std::vector<std::queue<int>> insert_order_;
};
