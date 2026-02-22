#include "trace.hpp"
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <cctype>
#include <iostream>

namespace CacheSim {

TraceReader::~TraceReader() {
    close();
}

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

    ptr_ = file_data_;
    end_ = file_data_ + file_size_;
    return true;
}

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

void TraceReader::skip_whitespace() {
    while (ptr_ < end_ && std::isspace(*ptr_)) ptr_++;
}

uint64_t TraceReader::parse_hex() {
    uint64_t value = 0;
    skip_whitespace();
    while (ptr_ < end_ && !std::isspace(*ptr_)) {
        value = (value << 4) | (*ptr_ <= '9' ? *ptr_ - '0' : (*ptr_ & ~0x20) - 'A' + 10);
        ptr_++;
    }
    return value;
}

int TraceReader::parse_decimal() {
    int value = 0;
    skip_whitespace();
    while (ptr_ < end_ && !std::isspace(*ptr_)) {
        value = value * 10 + (*ptr_ - '0');
        ptr_++;
    }
    return value;
}

bool TraceReader::next(TraceEntry& entry) {
    if (ptr_ >= end_) return false;

    entry.pc = parse_hex();
    entry.addr = parse_hex();
    
    skip_whitespace();
    entry.op = (ptr_ < end_) ? *ptr_++ : 0;
    
    entry.size = parse_decimal();

    // Skip to next line
    while (ptr_ < end_ && *ptr_ != '\n') ptr_++;
    if (ptr_ < end_) ptr_++;

    return entry.size != 0 || entry.op != 0;
}

}  // namespace CacheSim