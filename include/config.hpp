#ifndef CONFIG_HPP
#define CONFIG_HPP

#include "cache.hpp"
#include <vector>
#include <string>

namespace CacheSim {

struct CacheConfig {
    std::vector<Cache> caches;
};

// Parse cache configuration from JSON file
// Returns 0 on success, non-zero on error
int parse_config(CacheConfig* config, const std::string& filename);

}  // namespace CacheSim

#endif