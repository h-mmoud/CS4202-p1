#include "cache.hpp"
#include "config.hpp"
#include "trace.hpp"
#include <iostream>

using namespace CacheSim;

void print_stats(const CacheConfig& config, uint64_t main_memory_accesses) {
    for (const auto& cache : config.caches) {
        std::cout << "\n[" << cache.name << "]"
                  << "\n  hits:   " << cache.hits
                  << "\n  misses: " << cache.misses;
    }
    std::cout << "\nmain memory accesses: " << main_memory_accesses << "\n";
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <config.json> <trace_file>\n";
        return 1;
    }

    CacheConfig config;
    if (parse_config(&config, argv[1]) != 0) {
        return 1;
    }

    TraceReader reader;
    if (!reader.open(argv[2])) {
        return 1;
    }

    uint64_t timer = 0;
    uint64_t main_memory_accesses = 0;
    uint64_t line_size = config.caches[0].line_size;
    TraceEntry entry;

    while (reader.next(entry)) {
        timer++;

        uint64_t start_line = entry.addr / line_size;
        uint64_t end_line = (entry.addr + entry.size - 1) / line_size;

        for (uint64_t line = start_line; line <= end_line; line++) {
            uint64_t addr = line * line_size;
            bool hit = false;

            for (auto& cache : config.caches) {
                if (access_cache(&cache, addr, timer)) {
                    hit = true;
                    break;
                }
            }

            if (!hit) {
                main_memory_accesses++;
            }
        }
    }

    print_stats(config, main_memory_accesses);
    return 0;
}