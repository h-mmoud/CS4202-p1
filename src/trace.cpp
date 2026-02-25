#include "trace.hpp"
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <iostream>
#include <array>

namespace CacheSim {

    /**
    * Lookup table for fast parsing
    * Faster than std::stoi or manual per-char parsing
    */
    static constexpr auto make_hex_lut() {
        std::array<uint8_t, 256> lut{};
        for (int i = 0; i < 256; i++) lut[i] = 0xFF;
        for (int i = '0'; i <= '9'; i++) lut[i] = i - '0';
        for (int i = 'a'; i <= 'f'; i++) lut[i] = 10 + (i - 'a');
        for (int i = 'A'; i <= 'F'; i++) lut[i] = 10 + (i - 'A');
        return lut;
    }
    static constexpr auto hex_lut = make_hex_lut();

    /* Destructor to avoid leaks */
    TraceReader::~TraceReader() { close(); }

    /* Uses mmap to reduce I/O overhead (much much faster than ifstream or fread) */
    bool TraceReader::open(const std::string& filename) {
        fd_ = ::open(filename.c_str(), O_RDONLY);
        if (fd_ == -1) {
            std::cerr << "Failed to open trace file: " << filename << "\n";
            return false;
        }
        struct stat sb;
        if (fstat(fd_, &sb) == -1) {
            std::cerr << "Failed to get file size\n";
            close();
            return false;
        }
        file_size_ = sb.st_size;
        file_data_ = static_cast<const char*>(
            mmap(nullptr, file_size_, PROT_READ, MAP_PRIVATE, fd_, 0));
        if (file_data_ == MAP_FAILED) {
            std::cerr << "mmap failed\n";
            file_data_ = nullptr;
            close();
            return false;
        }
        
        // Tells kernel that file will be read sequentially
        madvise(const_cast<char*>(file_data_), file_size_, MADV_SEQUENTIAL);
        ptr_ = file_data_;
        end_ = file_data_ + file_size_;
        return true;
    }

    /* Unmaps memory to avoid leaks */
    void TraceReader::close() {
        if (file_data_) {
            munmap(const_cast<char*>(file_data_), file_size_);
            file_data_ = nullptr;
        }
        if (fd_ != -1) {
            ::close(fd_);
            fd_ = -1;
        }
    }

    /** 
    * Unrolls hex parsing to maxmise ILP and minimise function call overhead 
    * Much faster than loops/std::stringstream
    */
    __attribute__((always_inline))
    inline uint64_t parse_16hex_fast(const char* p) {
        return 
            (static_cast<uint64_t>(hex_lut[static_cast<uint8_t>(p[0])]) << 60) |
            (static_cast<uint64_t>(hex_lut[static_cast<uint8_t>(p[1])]) << 56) |
            (static_cast<uint64_t>(hex_lut[static_cast<uint8_t>(p[2])]) << 52) |
            (static_cast<uint64_t>(hex_lut[static_cast<uint8_t>(p[3])]) << 48) |
            (static_cast<uint64_t>(hex_lut[static_cast<uint8_t>(p[4])]) << 44) |
            (static_cast<uint64_t>(hex_lut[static_cast<uint8_t>(p[5])]) << 40) |
            (static_cast<uint64_t>(hex_lut[static_cast<uint8_t>(p[6])]) << 36) |
            (static_cast<uint64_t>(hex_lut[static_cast<uint8_t>(p[7])]) << 32) |
            (static_cast<uint64_t>(hex_lut[static_cast<uint8_t>(p[8])]) << 28) |
            (static_cast<uint64_t>(hex_lut[static_cast<uint8_t>(p[9])]) << 24) |
            (static_cast<uint64_t>(hex_lut[static_cast<uint8_t>(p[10])]) << 20) |
            (static_cast<uint64_t>(hex_lut[static_cast<uint8_t>(p[11])]) << 16) |
            (static_cast<uint64_t>(hex_lut[static_cast<uint8_t>(p[12])]) << 12) |
            (static_cast<uint64_t>(hex_lut[static_cast<uint8_t>(p[13])]) << 8) |
            (static_cast<uint64_t>(hex_lut[static_cast<uint8_t>(p[14])]) << 4) |
            (static_cast<uint64_t>(hex_lut[static_cast<uint8_t>(p[15])]));
    }

    /**
    * Reads fields in-place, skipping whitespace and handling line endings efficiently
    */
    bool TraceReader::next(TraceEntry& entry) {
        // Hints to compiler for branch prediction/memory prefetching 
        if (__builtin_expect(ptr_ + 40 > end_, 0)) {
            return false;
        }

        __builtin_prefetch(ptr_ + 256, 0, 0);

        // Parse data in parallel
        entry.pc = parse_16hex_fast(ptr_);
        entry.addr = parse_16hex_fast(ptr_ + 17);
        entry.op = ptr_[34];
        entry.size = (ptr_[36] * 100) + (ptr_[37] * 10) + ptr_[38] - 5328;

        // jump to next line
        ptr_ += 40;
        return true;
    }
} // namespace CacheSim