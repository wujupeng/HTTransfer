#pragma once

#include <cstdint>
#include <string>
#include "Core/Common/Result.h"
#include "Core/Common/Types.h"

namespace ht {

class IDataSource {
public:
    virtual ~IDataSource() = default;
    virtual Result<void> Open(const std::string& path) = 0;
    virtual Result<size_t> Read(offset_t offset, void* buffer, size_t size) = 0;
    virtual void Close() = 0;
    virtual Result<uint64_t> GetSize() const = 0;
};

}