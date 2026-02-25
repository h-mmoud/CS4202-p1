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

void calc_num_sets(Cache *c) {
    switch (c->kind) {
        case CacheKind::full:
            c->num_sets = 1;
            break;
        case CacheKind::direct:
            c->num_sets = (uint32_t)(c->size / c->line_size);
            break;
        case CacheKind::_2way:
            c->num_sets = (uint32_t)(c->size / (2 * c->line_size));
            break;
        case CacheKind::_4way:
            c->num_sets = (uint32_t)(c->size / (4 * c->line_size));
            break;
        case CacheKind::_8way:
            c->num_sets = (uint32_t)(c->size / (8 * c->line_size));
            break;
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


// Helper to move a cache line to the Most Recently Used (head) position
void move_to_mru(Cache* cache, uint64_t set_idx, int32_t line_idx) {
    auto set = cache->get_set(set_idx);
    int32_t head = cache->lru_head[set_idx];
    
    if (head == line_idx) return; // Already at MRU

    // Unlink from current position
    int32_t prev = set[line_idx].prev;
    int32_t next = set[line_idx].next;

    if (prev != -1) set[prev].next = next;
    if (next != -1) set[next].prev = prev;

    // If it was the tail, update the tail
    if (cache->lru_tail[set_idx] == line_idx) {
        cache->lru_tail[set_idx] = prev;
    }

    // Move to head
    set[line_idx].prev = -1;
    set[line_idx].next = head;
    if (head != -1) set[head].prev = line_idx;
    
    cache->lru_head[set_idx] = line_idx;
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
    
    // Initialize LRU tracking
    cache->lru_head.resize(cache->num_sets, -1);
    cache->lru_tail.resize(cache->num_sets, -1);
    cache->tag_maps.resize(cache->num_sets);

    for (unsigned int s = 0; s < cache->num_sets; s++) {
        auto set = cache->get_set(s);
        if (cache->lines_per_set > 0) {
            cache->lru_head[s] = 0;
            cache->lru_tail[s] = cache->lines_per_set - 1;
            // Pre-link all lines in the set
            for (unsigned int i = 0; i < cache->lines_per_set; i++) {
                set[i].prev = i - 1;
                set[i].next = (i == cache->lines_per_set - 1) ? -1 : i + 1;
            }
        }
    }
}

bool access_cache(Cache* cache, uint64_t addr, uint64_t timer) {
    uint64_t idx = cache->get_index(addr);
    uint64_t tag = cache->get_tag(addr);
    auto set = cache->get_set(idx);
    uint32_t lines = cache->lines_per_set;

    int32_t hit_idx = -1;

    // Only use the hash map for fully associative caches
    if (cache->kind != CacheKind::full) {
        for (uint32_t i = 0; i < lines; ++i) {
            if (set[i].valid && set[i].tag == tag) {
                hit_idx = i;
                break;
            }
        }
    } else {
        auto& tag_map = cache->tag_maps[idx];
        auto it = tag_map.find(tag);
        if (it != tag_map.end()) {
            hit_idx = it->second;
        }
    }

    // --- HANDLE HIT ---
    if (hit_idx != -1) {
        cache->hits++;
        set[hit_idx].last_access = timer;
        set[hit_idx].access_count++;
        if (cache->replacement_policy == ReplacementPolicy::lru) {
            move_to_mru(cache, idx, hit_idx);
        }
        return true;
    }

    // --- HANDLE MISS ---
    cache->misses++;
    int32_t victim = -1;

    // OPTIMIZATION 2: Single-Pass Victim Selection
    if (cache->replacement_policy == ReplacementPolicy::lfu) {
        uint64_t min_count = UINT64_MAX;
        for (uint32_t i = 0; i < lines; ++i) {
            if (!set[i].valid) {
                victim = i; // Empty spot found, stop searching instantly
                break;
            }
            if (set[i].access_count < min_count) {
                min_count = set[i].access_count;
                victim = i;
            }
        }
    } else {
        // LRU, RR, or Direct policies
        for (uint32_t i = 0; i < lines; ++i) {
            if (!set[i].valid) {
                victim = i;
                break;
            }
        }
        
        // If the set is full, use O(1) eviction logic
        if (victim == -1) {
            if (cache->kind == CacheKind::direct) {
                victim = 0;
            } else if (cache->replacement_policy == ReplacementPolicy::lru) {
                victim = cache->lru_tail[idx];
            } else {
                victim = cache->rr_counters[idx];
                cache->rr_counters[idx] = (victim + 1) % lines;
            }
        }
    }

    // Update Hash Map ONLY if it's a Fully Associative cache
    if (cache->kind == CacheKind::full) {
        auto& tag_map = cache->tag_maps[idx];
        if (set[victim].valid) {
            tag_map.erase(set[victim].tag);
        }
        tag_map[tag] = victim;
    }

    // Overwrite the victim line
    set[victim].valid = true;
    set[victim].tag = tag;
    set[victim].last_access = timer;
    set[victim].access_count = 1;

    if (cache->replacement_policy == ReplacementPolicy::lru) {
        move_to_mru(cache, idx, victim);
    }

    return false;
}

}  // namespace CacheSim
