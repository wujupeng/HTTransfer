#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include "Core/Common/Result.h"

namespace ht {

using FileHandle = void*;
using IoCompletionCallback = std::function<void(uint64_t completion_key,
                                                 uint32_t bytes_transferred,
                                                 uint64_t error)>;

class IAsyncIoEngine {
public:
    virtual ~IAsyncIoEngine() = default;
    virtual Result<void> initialize(uint32_t worker_threads) = 0;
    virtual Result<void> shutdown() = 0;
    virtual Result<void> associateFile(FileHandle file_handle, uint64_t completion_key) = 0;
    virtual Result<void> submitRead(FileHandle file_handle, uint64_t offset, uint32_t size,
                                    uint8_t* buffer, uint64_t completion_key) = 0;
    virtual Result<void> submitWrite(FileHandle file_handle, uint64_t offset, uint32_t size,
                                     const uint8_t* buffer, uint64_t completion_key) = 0;
    virtual void registerCompletionCallback(IoCompletionCallback callback) = 0;
};

}