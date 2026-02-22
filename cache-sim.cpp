#include <iostream>
#include <fstream>
#include <sstream>
#include "cache-sim.hpp"
#include <rapidjson/document.h>
#include <rapidjson/error/en.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <cctype>

void calc_num_sets(Cache *c);
void calc_lines_per_set(Cache *c);
void calc_bit_counts(Cache *c);

void calc_num_sets(Cache *c) {
    if (c->kind.compare("full") == 0) {
        c->num_sets = 1;
    } else if (c->kind.compare("direct") == 0) {
        c->num_sets = (uint32_t) (c->size / c->line_size);
    } else if (c->kind.compare("2way") == 0) {
        c->num_sets = (uint32_t) (c->size / (2 * c->line_size));
    } else if (c->kind.compare("4way") == 0) {
        c->num_sets = (uint32_t) (c->size / (4 * c->line_size));
    } else if (c->kind.compare("8way") == 0) {
        c->num_sets = (uint32_t) (c->size / (8 * c->line_size));
    } 
}

void calc_lines_per_set(Cache *c) {
    c->lines_per_set = (c->size / c->line_size) / (c->num_sets);
}

void calc_bit_counts(Cache *c) {
    unsigned int num_sets = c->num_sets;
    unsigned int line_size = c->line_size;
    unsigned int index_bits = 0;
    unsigned int offset_bits = 0;
    unsigned int tag_bits;
    
    /* log_2 of set count and line size to get index and offset bits respectively */
    while (num_sets >>= 1) index_bits++;
    while (line_size >>= 1) offset_bits++;
    tag_bits = 64 - (index_bits + offset_bits);

    c->index_size = index_bits;
    c->offset_size = offset_bits;
    c->tag_size = tag_bits;
}

int parse_config_json(CacheConfig *config, std::string filename) {
    std::ifstream ifs(filename);
    if (!ifs.is_open()) {
        std::cerr << "Could not open file: " << filename << std::endl;
        return 1;
    }

    std::stringstream buffer;
    buffer << ifs.rdbuf();
    std::string content = buffer.str();

    rapidjson::Document doc;
    if (doc.Parse(content.c_str()).HasParseError()) {
        std::cerr << "JSON parse error: " << rapidjson::GetParseError_En(doc.GetParseError()) 
                  << " (" << doc.GetErrorOffset() << ")" << std::endl;
        return 1;
    }

    if (doc.HasMember("caches") && doc["caches"].IsArray()) {
        for (const auto& c : doc["caches"].GetArray()) {
            Cache cache;
            if (c.HasMember("name") && c["name"].IsString()) cache.name = c["name"].GetString();
            if (c.HasMember("size") && c["size"].IsUint64()) cache.size = c["size"].GetUint64();
            if (c.HasMember("line_size") && c["line_size"].IsUint64()) cache.line_size = c["line_size"].GetUint64();
            if (c.HasMember("kind") && c["kind"].IsString()) cache.kind = c["kind"].GetString();
            if (c.HasMember("replacement_policy") && c["replacement_policy"].IsString()) {
                cache.replacement_policy = c["replacement_policy"].GetString();
            }
            calc_num_sets(&cache);
            calc_lines_per_set(&cache);
            calc_bit_counts(&cache);
            cache.storage.resize(cache.num_sets * cache.lines_per_set);
            cache.rr_counters.resize(cache.num_sets, 0); // Initialize RR counters
            config->caches.push_back(cache);

        }
    }

    for (const auto& c : config->caches) {
        std::cerr << "Found Cache: " << c.name
        << " (Size: " << c.size << ")\n"
        << "number of sets: " << c.num_sets
        << "\nlines per set: " << c.lines_per_set
        << "\noffset bits: " << c.offset_size
        << "\nindex bits: " << c.index_size
        << "\ntag bits: " << c.tag_size;
    }


    return 0;
}

bool access_cache(Cache* cache, uint64_t addr, uint64_t timer) {
  uint64_t idx = cache->get_index(addr);
  uint64_t tag = cache->get_tag(addr);
  auto set = cache->get_set(idx);

  /* check for hit */
  for (size_t i = 0; i < set.size(); i++) {
    if (set[i].valid && set[i].tag == tag) {
      cache->hits++;
      set[i].last_access = timer;
      set[i].access_count++;
      return true;
    }
  }

  cache->misses++;

  /* check for an empty (invalid) line first */
  for (size_t i = 0; i < set.size(); i++) {
    if (!set[i].valid) {
      set[i].valid = true;
      set[i].tag = tag;
      set[i].last_access = timer;
      set[i].access_count = 1;
      return false;
    }
  }

  /* eviction needed */
  size_t victim_idx = 0;

  if (cache->kind == "direct") {
    /* direct-mapped: only one line per set */
    victim_idx = 0;
  } else if (cache->replacement_policy == "lru") {
    /* Least Recently Used: find line with smallest last_access */
    uint64_t min_time = set[0].last_access;
    for (size_t i = 1; i < set.size(); i++) {
      if (set[i].last_access < min_time) {
        min_time = set[i].last_access;
        victim_idx = i;
      }
    }
  } else if (cache->replacement_policy == "lfu") {
    /* Least Frequently Used: find line with smallest access_count */
    uint64_t min_count = set[0].access_count;
    victim_idx = 0;
    for (size_t i = 1; i < set.size(); i++) {
      if (set[i].access_count < min_count) {
        min_count = set[i].access_count;
        victim_idx = i;
      }
    }
  } else {
    /* Default: Round Robin */
    victim_idx = cache->rr_counters[idx];
    cache->rr_counters[idx] = (victim_idx + 1) % cache->lines_per_set;
  }

  set[victim_idx].valid = true;
  set[victim_idx].tag = tag;
  set[victim_idx].last_access = timer;
  set[victim_idx].access_count = 1;

  return false;
}



int main(int argc, char* argv[]) {
    CacheConfig config;
    std::string filename = argv[1];
    std::string tracefilename = argv[2];
    parse_config_json(&config, filename);

    int fd = open(tracefilename.c_str(), O_RDONLY);
    if (fd == -1) {
        std::cerr << "Failed to open trace file\n";
        return 1;
    }

    struct stat sb;
    if (fstat(fd, &sb) == -1) {
        std::cerr << "Failed to get file size\n";
        close(fd);
        return 1;
    }

    const char* file_data = static_cast<const char*>(mmap(NULL, sb.st_size, PROT_READ, MAP_PRIVATE, fd, 0));
    if (file_data == MAP_FAILED) {
        std::cerr << "mmap failed\n";
        close(fd);
        return 1;
    }


    // std::ifstream ifs;
    // ifs.open(tracefilename, std::ifstream::in);

    // std::string tf_line;
    int counter = 0;
    uint64_t timer = 0;
    uint64_t main_memory_accesses = 0;

    const char* ptr = file_data;
    const char* end = file_data + sb.st_size;

    while (ptr < end) {
        uint64_t pc = 0, addr = 0;
        char op = 0;
        int size = 0;

        // Fast parse PC (hex)
        while (ptr < end && std::isspace(*ptr)) ptr++;
        while (ptr < end && !std::isspace(*ptr)) {
            pc = (pc << 4) | (*ptr <= '9' ? *ptr - '0' : (*ptr & ~0x20) - 'A' + 10);
            ptr++;
        }

        // Fast parse Address (hex)
        while (ptr < end && std::isspace(*ptr)) ptr++;
        while (ptr < end && !std::isspace(*ptr)) {
            addr = (addr << 4) | (*ptr <= '9' ? *ptr - '0' : (*ptr & ~0x20) - 'A' + 10);
            ptr++;
        }

        // Fast parse Op (char)
        while (ptr < end && std::isspace(*ptr)) ptr++;
        if (ptr < end) {
            op = *ptr;
            ptr++;
        }

        // Fast parse Size (decimal)
        while (ptr < end && std::isspace(*ptr)) ptr++;
        while (ptr < end && !std::isspace(*ptr)) {
            size = size * 10 + (*ptr - '0');
            ptr++;
        }

        // Skip to next line
        while (ptr < end && *ptr != '\n') ptr++;
        if (ptr < end && *ptr == '\n') ptr++;

        if (size == 0 && op == 0) continue; // Skip empty lines

        uint64_t start_addr = addr;
        uint64_t end_addr = addr + size - 1;
        timer++;
        
        // Calculate which line boundaries we cross
        uint64_t line_size = config.caches[0].line_size;
        uint64_t start_line = start_addr / line_size;
        uint64_t end_line = end_addr / line_size;

        for (uint64_t l = start_line; l <= end_line; l++) {
            uint64_t current_addr = l * line_size;

            bool found = false;
            for (auto& cache : config.caches) {
            if (access_cache(&cache, current_addr, timer)){
                found = true;
                break;
            }
            }

            if (!found) {
                main_memory_accesses++;
            }
        }
    }

    munmap((void*)file_data, sb.st_size);
    close(fd);

    for (const auto& cache : config.caches) {
        uint64_t total = cache.hits + cache.misses;
        std::cout << "\n[" << cache.name << "]"
                  << "\n  hits:   " << cache.hits
                  << "\n  misses: " << cache.misses;
    }
    std::cout << "\nmain memory accesses: " << main_memory_accesses << "\n";

    return 0;
}
