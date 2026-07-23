#pragma once

#include "Core/IDataSource.h"
#include <string>
#include <mutex>

#ifdef _WIN32
#include <windows.h>
#endif

namespace ht {

class LocalFileSource : public IDataSource {
public:
    LocalFileSource() = default;
    ~LocalFileSource() override;

    LocalFileSource(const LocalFileSource&) = delete;
    LocalFileSource& operator=(const LocalFileSource&) = delete;

    Result<void> Open(const std::string& path) override;
    Result<size_t> Read(offset_t offset, void* buffer, size_t size) override;
    void Close() override;
    Result<uint64_t> GetSize() const override;

private:
#ifdef _WIN32
    HANDLE handle_ = INVALID_HANDLE_VALUE;
#else
    int fd_ = -1;
#endif
    std::string path_;
    uint64_t file_size_ = 0;
    std::mutex mutex_;
};

}
