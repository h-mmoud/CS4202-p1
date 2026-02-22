#include "config.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <rapidjson/document.h>
#include <rapidjson/error/en.h>

namespace CacheSim {

int parse_config(CacheConfig* config, const std::string& filename) {
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
        std::cerr << "JSON parse error: " 
                  << rapidjson::GetParseError_En(doc.GetParseError())
                  << " (" << doc.GetErrorOffset() << ")" << std::endl;
        return 1;
    }

    if (!doc.HasMember("caches") || !doc["caches"].IsArray()) {
        std::cerr << "Invalid config: missing 'caches' array" << std::endl;
        return 1;
    }

    for (const auto& c : doc["caches"].GetArray()) {
        Cache cache;
        
        if (c.HasMember("name") && c["name"].IsString())
            cache.name = c["name"].GetString();
        if (c.HasMember("size") && c["size"].IsUint64())
            cache.size = c["size"].GetUint64();
        if (c.HasMember("line_size") && c["line_size"].IsUint64())
            cache.line_size = c["line_size"].GetUint64();
        if (c.HasMember("kind") && c["kind"].IsString())
            cache.kind = c["kind"].GetString();
        if (c.HasMember("replacement_policy") && c["replacement_policy"].IsString())
            cache.replacement_policy = c["replacement_policy"].GetString();

        init_cache(&cache);
        config->caches.push_back(cache);
    }

    return 0;
}

}  // namespace CacheSim