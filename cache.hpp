#pragma once
#include "policies/replacement_policy.hpp"
#include <vector>
#include <cstdint>
#include <cmath>
#include <memory>
#include <string>
#include <cassert>

// ─── Cache line (one slot in a set) ────────────────────────────────────────
struct CacheLine {
    bool     valid = false;
    bool     dirty = false;
    uint64_t tag   = 0;
};

// ─── Cache statistics ───────────────────────────────────────────────────────
struct CacheStats {
    uint64_t accesses  = 0;
    uint64_t hits      = 0;
    uint64_t misses    = 0;
    uint64_t evictions = 0;
    uint64_t writebacks = 0;   // dirty evictions (write-back cache)

    double hit_rate()  const { return accesses ? (double)hits / accesses : 0.0; }
    double miss_rate() const { return 1.0 - hit_rate(); }
    double mpki()      const { return accesses ? (double)misses / (accesses / 1000.0) : 0.0; }
};

// ─── Cache configuration ────────────────────────────────────────────────────
struct CacheConfig {
    uint64_t    size_bytes;          // Total cache size in bytes
    uint32_t    block_size;          // Cache line size in bytes
    uint32_t    associativity;       // Ways per set (1 = direct-mapped)
    std::string level;               // "L1" or "L2"
    bool        write_back = true;   // true = write-back, false = write-through
    bool        write_allocate = true;
};

// ─── Cache ──────────────────────────────────────────────────────────────────
class Cache {
public:
    Cache(const CacheConfig& cfg, std::unique_ptr<ReplacementPolicy> policy,
          Cache* next_level = nullptr)
        : cfg_(cfg), policy_(std::move(policy)), next_(next_level)
    {
        assert((cfg.size_bytes & (cfg.size_bytes - 1)) == 0 && "Cache size must be power of 2");
        assert((cfg.block_size & (cfg.block_size - 1)) == 0 && "Block size must be power of 2");

        num_sets_       = cfg.size_bytes / (cfg.block_size * cfg.associativity);
        offset_bits_    = static_cast<int>(std::log2(cfg.block_size));
        index_bits_     = static_cast<int>(std::log2(num_sets_));
        offset_mask_    = (1ULL << offset_bits_) - 1;
        index_mask_     = ((1ULL << index_bits_) - 1) << offset_bits_;

        // Initialise all sets and ways
        sets_.assign(num_sets_, std::vector<CacheLine>(cfg.associativity));
    }

    // ─── Main access function ────────────────────────────────────────────
    // is_write: true = store instruction, false = load instruction
    // Returns: true = hit
    bool access(uint64_t addr, bool is_write = false) {
        stats_.accesses++;
        auto [tag, set_idx] = decode(addr);

        // Search for tag in set
        for (int way = 0; way < (int)cfg_.associativity; way++) {
            auto& line = sets_[set_idx][way];
            if (line.valid && line.tag == tag) {
                // ── HIT ──
                stats_.hits++;
                if (is_write) line.dirty = true;
                policy_->on_access(set_idx, way, tag);
                return true;
            }
        }

        // ── MISS ──
        stats_.misses++;

        if (!is_write || cfg_.write_allocate) {
            // Fetch block from next level (or memory)
            if (next_) next_->access(addr, false);

            int evict_way = find_invalid_way(set_idx);
            if (evict_way < 0) {
                evict_way = policy_->get_evict_way(set_idx);
                stats_.evictions++;
                auto& old = sets_[set_idx][evict_way];
                if (cfg_.write_back && old.dirty) {
                    stats_.writebacks++;
                    // Write dirty block back to next level
                    if (next_) {
                        uint64_t old_addr = rebuild_addr(old.tag, set_idx);
                        next_->access(old_addr, true);
                    }
                }
            }

            auto& line    = sets_[set_idx][evict_way];
            line.valid    = true;
            line.dirty    = is_write;
            line.tag      = tag;
            policy_->on_insert(set_idx, evict_way, tag);
        } else {
            // Write-no-allocate: write directly to next level
            if (next_) next_->access(addr, true);
        }

        return false;
    }

    void reset() {
        for (auto& s : sets_)
            for (auto& l : s) l = {};
        policy_->reset();
        stats_ = {};
    }

    const CacheStats& stats()  const { return stats_; }
    const CacheConfig& config() const { return cfg_; }
    uint64_t num_sets()         const { return num_sets_; }
    std::string policy_name()   const { return policy_->name(); }

private:
    std::pair<uint64_t, uint64_t> decode(uint64_t addr) const {
        uint64_t set_idx = (addr & index_mask_) >> offset_bits_;
        uint64_t tag     = addr >> (offset_bits_ + index_bits_);
        return {tag, set_idx};
    }

    uint64_t rebuild_addr(uint64_t tag, uint64_t set_idx) const {
        return (tag << (offset_bits_ + index_bits_)) | (set_idx << offset_bits_);
    }

    int find_invalid_way(uint64_t set_idx) const {
        for (int w = 0; w < (int)cfg_.associativity; w++)
            if (!sets_[set_idx][w].valid) return w;
        return -1;
    }

    CacheConfig  cfg_;
    uint64_t     num_sets_;
    int          offset_bits_, index_bits_;
    uint64_t     offset_mask_, index_mask_;

    std::vector<std::vector<CacheLine>> sets_;
    std::unique_ptr<ReplacementPolicy>  policy_;
    Cache*                              next_;   // nullptr = main memory
    CacheStats                          stats_;
};
