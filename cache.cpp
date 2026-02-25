#include "cache.hpp"

namespace CacheSim {

// ============================================================================
// Cache Class Methods
// ============================================================================

Span<CacheLine> Cache::get_set(unsigned int index) {
    return Span<CacheLine>(&storage[index * lines_per_set], lines_per_set);
}

uint64_t Cache::get_tag(uint64_t addr) const {
    return addr >> (index_size + offset_size);
}

uint64_t Cache::get_index(uint64_t addr) const {
    return (addr >> offset_size) & ((1ULL << index_size) - 1);
}

// ============================================================================
// Internal Helpers
// ============================================================================
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

    // log_2 bit shifting operation
    while (num_sets >>= 1) index_bits++;
    while (line_size >>= 1) offset_bits++;

    c->index_size = index_bits;
    c->offset_size = offset_bits;
    c->tag_size = 64 - (index_bits + offset_bits);
}

/* Helper to move a cache line to the Most Recently Used (head) position */
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

/* Swaps two elements in the LFU heap and updates their positions */
void swap_heap(Cache* cache, uint32_t set_idx, int32_t h1, int32_t h2) {
    uint32_t offset = set_idx * cache->lines_per_set;
    int32_t line1 = cache->lfu_heaps[offset + h1];
    int32_t line2 = cache->lfu_heaps[offset + h2];
    
    // Swap the elements in the heap array
    cache->lfu_heaps[offset + h1] = line2;
    cache->lfu_heaps[offset + h2] = line1;
    
    // Update the reverse mapping array
    cache->heap_pos[offset + line1] = h2;
    cache->heap_pos[offset + line2] = h1;
}

/* Maintains the LFU heap property after access count changes, ensuring O(log n) victim selection  */
void sift_down_lfu(Cache* cache, uint32_t set_idx, int32_t heap_idx) {
    auto set = cache->get_set(set_idx);
    uint32_t offset = set_idx * cache->lines_per_set;
    int32_t size = cache->lines_per_set;

    /* Prefer smaller count, break ties with smaller physical index */
    auto is_smaller = [&](int32_t line_a, int32_t line_b) {
        if (set[line_a].access_count != set[line_b].access_count) {
            return set[line_a].access_count < set[line_b].access_count;
        }
        return line_a < line_b; // Tie-breaker
    };

    while (true) {
        int32_t left = 2 * heap_idx + 1;
        int32_t right = 2 * heap_idx + 2;
        int32_t smallest = heap_idx;

        // Check left child
        if (left < size) {
            int32_t line_left = cache->lfu_heaps[offset + left];
            int32_t line_smallest = cache->lfu_heaps[offset + smallest];
            
            if (is_smaller(line_left, line_smallest)) {
                smallest = left;
            }
        }

        // Check right child
        if (right < size) {
            int32_t line_right = cache->lfu_heaps[offset + right];
            int32_t line_smallest = cache->lfu_heaps[offset + smallest];
            
            if (is_smaller(line_right, line_smallest)) {
                smallest = right;
            }
        }

        if (smallest != heap_idx) {
            swap_heap(cache, set_idx, heap_idx, smallest);
            heap_idx = smallest; // Continue sifting down
        } else {
            break;
        }
    }
}

/**
 * Finds a matching cache line by tag, using a hash map for fully associative caches and linear scan otherwise
 * Linear scan is faster for lower associativity caches due to less overhead
 */
int32_t find_hit_index(Cache* cache, uint64_t idx, uint64_t tag, Span<CacheLine> set) {
    if (cache->kind == CacheKind::full) {
        auto& tag_map = cache->tag_maps[idx];
        auto it = tag_map.find(tag);
        return (it != tag_map.end()) ? it->second : -1;
    } 
    
    // Linear scan for set-associative or direct mapped
    for (uint32_t i = 0; i < cache->lines_per_set; ++i) { 
        if (set[i].valid && set[i].tag == tag) return i;
    }
    return -1;
}

/* Updates metadata on a cache hit and adjusts replacement structures (LRU/LFU) */
void process_hit(Cache* cache, uint64_t idx, int32_t hit_idx, uint64_t timer) {
    cache->hits++;
    auto set = cache->get_set(idx);
    
    set[hit_idx].last_access = timer;
    set[hit_idx].access_count++;
    
    if (cache->replacement_policy == ReplacementPolicy::lru) {
        move_to_mru(cache, idx, hit_idx); 
    } else if (cache->replacement_policy == ReplacementPolicy::lfu) {
        // Sift down  
        uint32_t current_heap_pos = cache->heap_pos[idx * cache->lines_per_set + hit_idx];
        sift_down_lfu(cache, idx, current_heap_pos);
    }
}

/* Selects a victim line for replacement based on policy */
int32_t select_victim(Cache* cache, uint64_t idx, Span<CacheLine> set) {
    
    // O(1) LFU Victim Selection 
    if (cache->replacement_policy == ReplacementPolicy::lfu) {
        return cache->lfu_heaps[idx * cache->lines_per_set + 0]; // Root of the heap
    } 

    // Check for invalid (empty) lines first
    for (uint32_t i = 0; i < cache->lines_per_set; ++i) {
        if (!set[i].valid) return i; 
    }

    // Fallback to other replacement policies
    if (cache->kind == CacheKind::direct) {
        return 0;
    } else if (cache->replacement_policy == ReplacementPolicy::lru) {
        return cache->lru_tail[idx];
    } else {
        // Round Robin
        int32_t victim = cache->rr_counters[idx];
        cache->rr_counters[idx] = (victim + 1) % cache->lines_per_set;
        return victim;
    }
}

/**
 * Handles eviction and replacement, updating all relevant structures
 */
void replace_victim(Cache* cache, uint64_t idx, int32_t victim, uint64_t tag, uint64_t timer) {
    auto set = cache->get_set(idx);

    // Update Fully Associative Hash Map
    if (cache->kind == CacheKind::full) {
        auto& tag_map = cache->tag_maps[idx];
        if (set[victim].valid) tag_map.erase(set[victim].tag);
        tag_map[tag] = victim;
    }

    // Overwrite victim
    set[victim].valid = true;
    set[victim].tag = tag;
    set[victim].last_access = timer;
    set[victim].access_count = 1;

    // Update replacement policy structures for the new line
    if (cache->replacement_policy == ReplacementPolicy::lru) {
        move_to_mru(cache, idx, victim);
    } else if (cache->replacement_policy == ReplacementPolicy::lfu) {
        // The root count changed to exactly 1. Sift down to re-balance.
        sift_down_lfu(cache, idx, 0); 
    }
}

}  // anonymous namespace

// ============================================================================
// Main API
// ============================================================================

/**
 * Initialises cache structures, including LRU lists and LFU heaps, keeping storage contiguous and efficient.
 */
void init_cache(Cache* cache) {
    calc_num_sets(cache);
    calc_lines_per_set(cache);
    calc_bit_counts(cache);
    
    cache->storage.resize(cache->num_sets * cache->lines_per_set);
    cache->rr_counters.resize(cache->num_sets, 0);
    cache->tag_maps.resize(cache->num_sets);

    // Initialise LRU structures
    if (cache->replacement_policy == ReplacementPolicy::lru) {
        cache->lru_head.resize(cache->num_sets, -1);
        cache->lru_tail.resize(cache->num_sets, -1);

        for (unsigned int s = 0; s < cache->num_sets; s++) {
            auto set = cache->get_set(s);

            cache->lru_head[s] = 0;
            cache->lru_tail[s] = cache->lines_per_set - 1;

            // Pre-link all lines in the set
            for (unsigned int i = 0; i < cache->lines_per_set; i++) {
                set[i].prev = i - 1;
                set[i].next = (i == cache->lines_per_set - 1) ? -1 : i + 1;
            }
        }
    } 
    // Initialise LFU structures
    else if (cache->replacement_policy == ReplacementPolicy::lfu) {
        uint32_t total_lines = cache->num_sets * cache->lines_per_set;
        cache->lfu_heaps.resize(total_lines);
        cache->heap_pos.resize(total_lines);
        
        for (uint32_t s = 0; s < cache->num_sets; s++) {
            for (uint32_t i = 0; i < cache->lines_per_set; i++) {
                uint32_t idx = s * cache->lines_per_set + i;
                // Initialise heap such that line i is at heap position i 
                cache->lfu_heaps[idx] = i;
                cache->heap_pos[idx] = i;
            }
        }
    }
}

/**
 * Checks if an address is a hit or miss for a specific cache.
 */
bool access_cache(Cache* cache, uint64_t addr, uint64_t timer) {
    uint64_t idx = cache->get_index(addr);
    uint64_t tag = cache->get_tag(addr);
    auto set = cache->get_set(idx);

    // 1. Check for Hit
    int32_t hit_idx = find_hit_index(cache, idx, tag, set);

    if (hit_idx != -1) {
        process_hit(cache, idx, hit_idx, timer);
        return true;
    }

    // 2. Handle Miss
    cache->misses++;
    int32_t victim_idx = select_victim(cache, idx, set);
    replace_victim(cache, idx, victim_idx, tag, timer);

    return false;
}

}  // namespace CacheSim