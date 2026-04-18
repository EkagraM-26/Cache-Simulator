#pragma once
#include "replacement_policy.hpp"
#include <vector>
#include <list>
#include <unordered_map>

// Least Recently Used (LRU) replacement policy
// Uses a per-set doubly-linked list to track usage order
class LRUPolicy : public ReplacementPolicy {
public:
    LRUPolicy(int num_sets, int associativity)
        : num_sets_(num_sets), assoc_(associativity),
          lru_order_(num_sets), way_iter_(num_sets) {}

    void on_access(int set_index, int way, uint64_t /*tag*/) override {
        // Move this way to front (most recently used)
        auto& order = lru_order_[set_index];
        auto& iters = way_iter_[set_index];
        if (iters.count(way)) {
            order.erase(iters[way]);
        }
        order.push_front(way);
        iters[way] = order.begin();
    }

    void on_insert(int set_index, int way, uint64_t tag) override {
        on_access(set_index, way, tag);
    }

    int get_evict_way(int set_index) override {
        auto& order = lru_order_[set_index];
        if (order.empty()) return 0;
        return order.back(); // Least recently used is at back
    }

    void reset() override {
        for (auto& o : lru_order_) o.clear();
        for (auto& m : way_iter_) m.clear();
    }

    std::string name() const override { return "LRU"; }

private:
    int num_sets_, assoc_;
    std::vector<std::list<int>> lru_order_;
    std::vector<std::unordered_map<int, std::list<int>::iterator>> way_iter_;
};
