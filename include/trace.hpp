#ifndef TRACE_HPP
#define TRACE_HPP

#include <cstdint>
#include <string>

namespace CacheSim {

struct TraceEntry {
    uint64_t pc;
    uint64_t addr;
    char op;
    int size;
};

// Memory-mapped trace file reader
class TraceReader {
public:
    TraceReader() = default;
    ~TraceReader();

    bool open(const std::string& filename);
    void close();
    bool next(TraceEntry& entry);
    bool is_open() const { return file_data_ != nullptr; }

private:
    const char* file_data_ = nullptr;
    const char* ptr_ = nullptr;
    const char* end_ = nullptr;
    size_t file_size_ = 0;
    int fd_ = -1;

    uint64_t parse_hex();
    int parse_decimal();
    void skip_whitespace();
};

}  // namespace CacheSim

#endif