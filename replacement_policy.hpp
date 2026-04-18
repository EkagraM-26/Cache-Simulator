#pragma once
#include <cstdint>
#include <string>

// Abstract base class for all replacement policies
class ReplacementPolicy {
public:
    virtual ~ReplacementPolicy() = default;

    // Called on every cache access (hit or miss) to update internal state
    virtual void on_access(int set_index, int way, uint64_t tag) = 0;

    // Called on a miss: returns which 'way' to evict in the given set
    virtual int get_evict_way(int set_index) = 0;

    // Called when a line is inserted into a specific way
    virtual void on_insert(int set_index, int way, uint64_t tag) = 0;

    // Reset internal state
    virtual void reset() = 0;

    virtual std::string name() const = 0;
};
