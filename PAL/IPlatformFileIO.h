#pragma once

#include <string>
#include <cstdint>
#include "Core/Common/Result.h"

namespace ht {

class IPlatformFileIO {
public:
    virtual ~IPlatformFileIO() = default;

    virtual Result<void*> open(const std::string& path, bool for_write, bool resume) = 0;
    virtual Result<void> close(void* handle) = 0;
    virtual Result<size_t> read(void* handle, uint64_t offset, void* buffer, size_t size) = 0;
    virtual Result<size_t> write(void* handle, uint64_t offset, const void* buffer, size_t size) = 0;
    virtual Result<uint64_t> getSize(void* handle) = 0;
    virtual Result<void> preallocate(void* handle, uint64_t size) = 0;
    virtual Result<void> truncate(void* handle, uint64_t size) = 0;
};

}