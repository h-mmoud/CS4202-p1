#include <iostream>
#include <glaze/glaze.hpp>

struct Cache {
  std::string name; 
  size_t size;
  size_t line_size;
  std::string kind;
  std::optional<std::string> replacement_policy;
};

struct CacheConfig {
  std::vector<Cache> caches;
};

template <>
struct glz::meta<Cache> {
    using T = Cache;
    static constexpr auto value = object(
        "name", &T::name,
        "size", &T::size,
        "line_size", &T::line_size,
        "kind", &T::kind,
        "replacement_policy", &T::replacement_policy
    );
};


int main() {
    CacheConfig config;
    std::string filename = "sample-inputs/direct.json";

    auto error = glz::read_file_json(config, filename, std::string{});

    for (const auto& c : config.caches) {
        std::cout << "Found Cache: " << c.name << " (Size: " << c.size << ")\n";
    }

    return 0;
}
