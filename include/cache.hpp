#ifndef CACHE_HPP
#define CACHE_HPP

#include <cstdint>
#include <cstddef>
#include <vector>
#include <string>
#include <optional>

namespace CacheSim {

// Custom span class for accessing cache sets
template <typename T>
class Span {
public:
    Span(T* ptr, size_t len) : ptr_(ptr), len_(len) {}

    T& operator[](size_t idx) { return ptr_[idx]; }
    const T& operator[](size_t idx) const { return ptr_[idx]; }

    size_t size() const { return len_; }
    T* begin() { return ptr_; }
    T* end() { return ptr_ + len_; }

private:
    T* ptr_;
    size_t len_;
};

// Represents a single cache line
struct CacheLine {
    uint64_t tag = 0;
    bool valid = false;
    uint64_t last_access = 0;   // For LRU: timestamp of last access
    uint64_t access_count = 0;  // For LFU: number of accesses
};

// Cache configuration and state
struct Cache {
    // Configuration
    std::string name;
    size_t size;
    size_t line_size;
    std::string kind;
    std::optional<std::string> replacement_policy;

    // Derived metadata
    unsigned int num_sets;
    unsigned int lines_per_set;
    unsigned int tag_size;
    unsigned int index_size;
    unsigned int offset_size;

    // Runtime state
    uint64_t hits = 0;
    uint64_t misses = 0;
    std::vector<CacheLine> storage;
    std::vector<uint32_t> rr_counters;  // Round-robin counters per set

    // Methods
    Span<CacheLine> get_set(unsigned int index);
    uint64_t get_tag(uint64_t addr) const;
    uint64_t get_index(uint64_t addr) const;
};

// Initialise cache derived values
void init_cache(Cache* cache);

// Access cache, returns true on hit
bool access_cache(Cache* cache, uint64_t addr, uint64_t timer);

}  // namespace CacheSim

#endif