#include "LocalFileSink.h"
#include "Core/Common/Types.h"
#include <filesystem>

namespace ht {

LocalFileSink::~LocalFileSink() { Close(); }

Result<void> LocalFileSink::Open(const std::string& path, uint64_t preallocate_size, bool resume) {
    path_ = path;
    std::error_code ec;
    auto fs_path = utf8ToPath(path);
    auto parent = fs_path.parent_path();
    if (!parent.empty() && !std::filesystem::exists(parent, ec) && !ec) {
        std::filesystem::create_directories(parent, ec);
    }

#ifdef _WIN32
    bool file_exists = std::filesystem::exists(fs_path, ec) && !ec;
    DWORD creation_disposition = (resume && file_exists) ? OPEN_EXISTING : CREATE_ALWAYS;

    handle_ = CreateFileW(fs_path.wstring().c_str(),
        GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr, creation_disposition,
        FILE_ATTRIBUTE_NORMAL, nullptr);
    if (handle_ == INVALID_HANDLE_VALUE) {
        return Result<void>::failure(ErrorCode::TargetError, "Cannot create target file for writing");
    }

    if (preallocate_size > 0 && !file_exists) {
        LARGE_INTEGER li;
        li.QuadPart = static_cast<LONGLONG>(preallocate_size);
        SetFilePointerEx(handle_, li, nullptr, FILE_BEGIN);
        SetEndOfFile(handle_);
        SetFilePointerEx(handle_, {}, nullptr, FILE_BEGIN);
    }
#else
    return Result<void>::failure(ErrorCode::IOError, "Not implemented");
#endif
    return Result<void>::success();
}

Result<size_t> LocalFileSink::Write(offset_t offset, const void* buffer, size_t size) {
#ifdef _WIN32
    if (handle_ == INVALID_HANDLE_VALUE) {
        return Result<size_t>::failure(ErrorCode::TargetError, "File not open");
    }
    std::lock_guard lock(mutex_);
    LARGE_INTEGER li;
    li.QuadPart = static_cast<LONGLONG>(offset);
    SetFilePointerEx(handle_, li, nullptr, FILE_BEGIN);

    size_t total_written = 0;
    const char* ptr = static_cast<const char*>(buffer);
    while (total_written < size) {
        size_t remaining = size - total_written;
        DWORD to_write = static_cast<DWORD>(std::min(remaining, static_cast<size_t>(0x80000000u)));
        DWORD bytes_written = 0;
        if (!WriteFile(handle_, ptr + total_written, to_write, &bytes_written, nullptr)) {
            return Result<size_t>::failure(ErrorCode::IOError, "WriteFile failed");
        }
        if (bytes_written == 0) {
            return Result<size_t>::failure(ErrorCode::IOError, "WriteFile wrote zero bytes");
        }
        total_written += bytes_written;
    }
    return Result<size_t>::success(total_written);
#else
    return Result<size_t>::failure(ErrorCode::IOError, "Not implemented");
#endif
}

void LocalFileSink::Close() {
#ifdef _WIN32
    if (handle_ != INVALID_HANDLE_VALUE) {
        CloseHandle(handle_);
        handle_ = INVALID_HANDLE_VALUE;
    }
#endif
}

Result<void> LocalFileSink::Flush() {
#ifdef _WIN32
    if (handle_ != INVALID_HANDLE_VALUE) {
        FlushFileBuffers(handle_);
    }
#endif
    return Result<void>::success();
}

}
