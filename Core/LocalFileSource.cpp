#include "LocalFileSource.h"
#include "Core/Common/Types.h"
#include <filesystem>

namespace ht {

LocalFileSource::~LocalFileSource() { Close(); }

Result<void> LocalFileSource::Open(const std::string& path) {
    path_ = path;
#ifdef _WIN32
    auto fs_path = utf8ToPath(path);
    handle_ = CreateFileW(fs_path.wstring().c_str(),
        GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING,
        FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
    if (handle_ == INVALID_HANDLE_VALUE) {
        return Result<void>::failure(ErrorCode::SourceError, "Cannot open source file for reading");
    }
    LARGE_INTEGER li;
    if (GetFileSizeEx(handle_, &li)) {
        file_size_ = static_cast<uint64_t>(li.QuadPart);
    }
#else
    file_size_ = std::filesystem::file_size(path);
#endif
    return Result<void>::success();
}

Result<size_t> LocalFileSource::Read(offset_t offset, void* buffer, size_t size) {
#ifdef _WIN32
    if (handle_ == INVALID_HANDLE_VALUE) {
        return Result<size_t>::failure(ErrorCode::SourceError, "File not open");
    }
    std::lock_guard lock(mutex_);
    LARGE_INTEGER li;
    li.QuadPart = static_cast<LONGLONG>(offset);
    SetFilePointerEx(handle_, li, nullptr, FILE_BEGIN);
    DWORD bytes_read = 0;
    if (!ReadFile(handle_, buffer, static_cast<DWORD>(size), &bytes_read, nullptr)) {
        return Result<size_t>::failure(ErrorCode::IOError, "ReadFile failed");
    }
    return Result<size_t>::success(static_cast<size_t>(bytes_read));
#else
    return Result<size_t>::failure(ErrorCode::IOError, "Not implemented");
#endif
}

void LocalFileSource::Close() {
#ifdef _WIN32
    if (handle_ != INVALID_HANDLE_VALUE) {
        CloseHandle(handle_);
        handle_ = INVALID_HANDLE_VALUE;
    }
#endif
}

Result<uint64_t> LocalFileSource::GetSize() const {
    return Result<uint64_t>::success(file_size_);
}

}
