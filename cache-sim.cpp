#include <iostream>
#include <vector>
#include <string>
#include <optional>
#include <fstream>
#include <sstream>
#include <rapidjson/document.h>
#include <rapidjson/error/en.h>

struct Cache {
  std::string name; 
  size_t size;
  size_t line_size;
  std::string kind;
  std::optional<std::string> replacement_policy;
  unsigned int num_sets;
  unsigned int lines_per_set;
};

struct CacheConfig {
  std::vector<Cache> caches;
};

void calc_num_sets(Cache *c);
void calc_lines_per_set(Cache *c);

void calc_num_sets(Cache *c) {
    if (c->kind.compare("direct") == 0) {
        c->num_sets = 1;
    } else if (c->kind.compare("full") == 0) {
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

int parse_config_json(CacheConfig config, std::string filename) {
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
            config.caches.push_back(cache);

        }
    }

    for (const auto& c : config.caches) {
        std::cerr << "Found Cache: " << c.name << " (Size: " << c.size << ")\n" << "number of sets: " << c.num_sets << "\nlines per set: " << c.lines_per_set << "\n";
    }

    return 0;
}

std::tuple<uint64_t, char, int> parse_line(std::string tracefile_line) {
    uint64_t pointer;
    char rw;
    int size; 
    sscanf(tracefile_line.c_str(), "# %d %lx %d", &type, &address, &instructions);

    return {type, address, instructions};

}

int main(int argc, char* argv[]) {
    CacheConfig config;
    std::string filename = argv[1];
    parse_config_json(config, filename);

    return 0;
}