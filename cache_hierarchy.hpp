#pragma once
#include "cache.hpp"
#include "policies/lru.hpp"
#include "policies/fifo.hpp"
#include "policies/random_policy.hpp"
#include <memory>
#include <string>
#include <stdexcept>

// ─── Policy factory ──────────────────────────────────────────────────────────
inline std::unique_ptr<ReplacementPolicy>
make_policy(const std::string& name, int sets, int assoc) {
    if (name == "LRU"    || name == "lru")    return std::make_unique<LRUPolicy>(sets, assoc);
    if (name == "FIFO"   || name == "fifo")   return std::make_unique<FIFOPolicy>(sets, assoc);
    if (name == "Random" || name == "random") return std::make_unique<RandomPolicy>(sets, assoc);
    throw std::invalid_argument("Unknown policy: " + name + " (choose LRU, FIFO, or Random)");
}

// ─── Hierarchy configuration ─────────────────────────────────────────────────
struct HierarchyConfig {
    // L1
    uint64_t l1_size        = 32  * 1024;   // 32 KB
    uint32_t l1_assoc       = 8;
    uint32_t l1_block       = 64;
    std::string l1_policy   = "LRU";

    // L2 (optional; set l2_size = 0 to disable)
    uint64_t l2_size        = 256 * 1024;   // 256 KB
    uint32_t l2_assoc       = 16;
    uint32_t l2_block       = 64;
    std::string l2_policy   = "LRU";

    bool write_back         = true;
    bool write_allocate     = true;
};

// ─── Cache hierarchy (L1 → L2 → memory) ─────────────────────────────────────
struct CacheHierarchy {
    std::unique_ptr<Cache> l2;   // may be null
    std::unique_ptr<Cache> l1;

    // Access the hierarchy (L1 first)
    bool access(uint64_t addr, bool is_write = false) {
        return l1->access(addr, is_write);
    }

    void reset() {
        l1->reset();
        if (l2) l2->reset();
    }
};

inline CacheHierarchy build_hierarchy(const HierarchyConfig& hcfg) {
    CacheHierarchy h;

    // Build L2 first (L1 will point to it)
    if (hcfg.l2_size > 0) {
        int l2_sets = hcfg.l2_size / (hcfg.l2_block * hcfg.l2_assoc);
        CacheConfig l2cfg{ hcfg.l2_size, hcfg.l2_block, hcfg.l2_assoc, "L2",
                           hcfg.write_back, hcfg.write_allocate };
        h.l2 = std::make_unique<Cache>(l2cfg,
                    make_policy(hcfg.l2_policy, l2_sets, hcfg.l2_assoc));
    }

    int l1_sets = hcfg.l1_size / (hcfg.l1_block * hcfg.l1_assoc);
    CacheConfig l1cfg{ hcfg.l1_size, hcfg.l1_block, hcfg.l1_assoc, "L1",
                       hcfg.write_back, hcfg.write_allocate };
    h.l1 = std::make_unique<Cache>(l1cfg,
                make_policy(hcfg.l1_policy, l1_sets, hcfg.l1_assoc),
                h.l2.get());

    return h;
}
