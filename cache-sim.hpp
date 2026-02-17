#include <cstdint>
#include <cstddef>
#include <vector>
#include <string>
#include <optional>


// custom span class 
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

/* Cache*/
struct CacheLine {
    uint64_t tag;
    bool valid = false;
    uint64_t last_access = 0; 
};

/* Cache metadata */
struct Cache {
  std::string name; 
  size_t size;
  size_t line_size;
  std::string kind;
  std::optional<std::string> replacement_policy;

  unsigned int num_sets;
  unsigned int lines_per_set;

  /* how many bits we need */
  unsigned int tag_size;
  unsigned int index_size;
  unsigned int offset_size;

  std::vector<CacheLine> storage;

  Span<CacheLine> get_set(unsigned int index) {
      return Span<CacheLine>(&storage[index * lines_per_set], lines_per_set);
  }

  uint64_t get_tag(uint64_t addr) {
    return addr >> (index_size + offset_size);
  }

  uint64_t get_index(uint64_t addr) {
    return (addr >> offset_size) & ((1ULL << index_size) - 1);
  }
};

/* Multi cache scenarios */
struct CacheConfig {
  std::vector<Cache> caches;
};
