#pragma once

#include "Core/IDataSink.h"
#include <string>
#include <mutex>

#ifdef _WIN32
#include <windows.h>
#endif

namespace ht {

class LocalFileSink : public IDataSink {
public:
    LocalFileSink() = default;
    ~LocalFileSink() override;

    LocalFileSink(const LocalFileSink&) = delete;
    LocalFileSink& operator=(const LocalFileSink&) = delete;

    Result<void> Open(const std::string& path, uint64_t preallocate_size = 0) override;
    Result<size_t> Write(offset_t offset, const void* buffer, size_t size) override;
    void Close() override;
    Result<void> Flush() override;

private:
#ifdef _WIN32
    HANDLE handle_ = INVALID_HANDLE_VALUE;
#else
    int fd_ = -1;
#endif
    std::string path_;
    std::mutex mutex_;
};

}
