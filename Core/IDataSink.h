#pragma once

#include <cstdint>
#include <string>
#include "Core/Common/Result.h"
#include "Core/Common/Types.h"

namespace ht {

class IDataSink {
public:
    virtual ~IDataSink() = default;
    virtual Result<void> Open(const std::string& path, uint64_t preallocate_size = 0) = 0;
    virtual Result<size_t> Write(offset_t offset, const void* buffer, size_t size) = 0;
    virtual void Close() = 0;
    virtual Result<void> Flush() = 0;
};

}