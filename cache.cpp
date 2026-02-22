#include "cache.hpp"

namespace CacheSim {

Span<CacheLine> Cache::get_set(unsigned int index) {
    return Span<CacheLine>(&storage[index * lines_per_set], lines_per_set);
}

uint64_t Cache::get_tag(uint64_t addr) const {
    return addr >> (index_size + offset_size);
}

uint64_t Cache::get_index(uint64_t addr) const {
    return (addr >> offset_size) & ((1ULL << index_size) - 1);
}

namespace {

void calc_num_sets(Cache* c) {
    if (c->kind == "full") {
        c->num_sets = 1;
    } else if (c->kind == "direct") {
        c->num_sets = static_cast<uint32_t>(c->size / c->line_size);
    } else if (c->kind == "2way") {
        c->num_sets = static_cast<uint32_t>(c->size / (2 * c->line_size));
    } else if (c->kind == "4way") {
        c->num_sets = static_cast<uint32_t>(c->size / (4 * c->line_size));
    } else if (c->kind == "8way") {
        c->num_sets = static_cast<uint32_t>(c->size / (8 * c->line_size));
    }
}

void calc_lines_per_set(Cache* c) {
    c->lines_per_set = (c->size / c->line_size) / c->num_sets;
}

void calc_bit_counts(Cache* c) {
    unsigned int num_sets = c->num_sets;
    unsigned int line_size = c->line_size;
    unsigned int index_bits = 0;
    unsigned int offset_bits = 0;

    while (num_sets >>= 1) index_bits++;
    while (line_size >>= 1) offset_bits++;

    c->index_size = index_bits;
    c->offset_size = offset_bits;
    c->tag_size = 64 - (index_bits + offset_bits);
}

size_t find_victim_lru(Span<CacheLine>& set) {
    size_t victim = 0;
    uint64_t min_time = set[0].last_access;
    for (size_t i = 1; i < set.size(); i++) {
        if (set[i].last_access < min_time) {
            min_time = set[i].last_access;
            victim = i;
        }
    }
    return victim;
}

size_t find_victim_lfu(Span<CacheLine>& set) {
    size_t victim = 0;
    uint64_t min_count = set[0].access_count;
    for (size_t i = 1; i < set.size(); i++) {
        if (set[i].access_count < min_count) {
            min_count = set[i].access_count;
            victim = i;
        }
    }
    return victim;
}

}  // anonymous namespace

void init_cache(Cache* cache) {
    calc_num_sets(cache);
    calc_lines_per_set(cache);
    calc_bit_counts(cache);
    cache->storage.resize(cache->num_sets * cache->lines_per_set);
    cache->rr_counters.resize(cache->num_sets, 0);
}

bool access_cache(Cache* cache, uint64_t addr, uint64_t timer) {
    uint64_t idx = cache->get_index(addr);
    uint64_t tag = cache->get_tag(addr);
    auto set = cache->get_set(idx);

    // Check for hit
    for (size_t i = 0; i < set.size(); i++) {
        if (set[i].valid && set[i].tag == tag) {
            cache->hits++;
            set[i].last_access = timer;
            set[i].access_count++;
            return true;
        }
    }

    cache->misses++;

    // Look for empty line
    for (size_t i = 0; i < set.size(); i++) {
        if (!set[i].valid) {
            set[i].valid = true;
            set[i].tag = tag;
            set[i].last_access = timer;
            set[i].access_count = 1;
            return false;
        }
    }

    // Select victim for eviction
    size_t victim = 0;
    if (cache->kind == "direct") {
        victim = 0;
    } else if (cache->replacement_policy == "lru") {
        victim = find_victim_lru(set);
    } else if (cache->replacement_policy == "lfu") {
        victim = find_victim_lfu(set);
    } else {
        // Round-robin (default)
        victim = cache->rr_counters[idx];
        cache->rr_counters[idx] = (victim + 1) % cache->lines_per_set;
    }

    set[victim].tag = tag;
    set[victim].last_access = timer;
    set[victim].access_count = 1;

    return false;
}

}  // namespace CacheSim