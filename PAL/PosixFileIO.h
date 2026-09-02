#pragma once

#include "PAL/IPlatformFileIO.h"
#include "Core/Common/Result.h"

#ifdef __linux__
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <cstring>

namespace ht {

class PosixFileIO : public IPlatformFileIO {
public:
    Result<void*> open(const std::string& path, bool for_write, bool resume) override {
        int flags = for_write
            ? (O_RDWR | O_CREAT | (resume ? 0 : O_TRUNC))
            : O_RDONLY;
        int fd = ::open(path.c_str(), flags, 0644);
        if (fd < 0) {
            return Result<void*>::failure(ErrorCode::IOError, "open failed");
        }
        return Result<void*>::success(reinterpret_cast<void*>(static_cast<intptr_t>(fd)));
    }

    Result<void> close(void* handle) override {
        int fd = static_cast<int>(reinterpret_cast<intptr_t>(handle));
        if (fd >= 0) ::close(fd);
        return Result<void>::success();
    }

    Result<size_t> read(void* handle, uint64_t offset, void* buffer, size_t size) override {
        int fd = static_cast<int>(reinterpret_cast<intptr_t>(handle));
        ssize_t ret = pread64(fd, buffer, size, static_cast<off64_t>(offset));
        if (ret < 0) {
            return Result<size_t>::failure(ErrorCode::IOError, "pread64 failed");
        }
        return Result<size_t>::success(static_cast<size_t>(ret));
    }

    Result<size_t> write(void* handle, uint64_t offset, const void* buffer, size_t size) override {
        int fd = static_cast<int>(reinterpret_cast<intptr_t>(handle));
        ssize_t ret = pwrite64(fd, buffer, size, static_cast<off64_t>(offset));
        if (ret < 0) {
            return Result<size_t>::failure(ErrorCode::IOError, "pwrite64 failed");
        }
        return Result<size_t>::success(static_cast<size_t>(ret));
    }

    Result<uint64_t> getSize(void* handle) override {
        int fd = static_cast<int>(reinterpret_cast<intptr_t>(handle));
        struct stat64 st;
        if (fstat64(fd, &st) < 0) {
            return Result<uint64_t>::failure(ErrorCode::IOError, "fstat64 failed");
        }
        return Result<uint64_t>::success(static_cast<uint64_t>(st.st_size));
    }

    Result<void> preallocate(void* handle, uint64_t size) override {
        int fd = static_cast<int>(reinterpret_cast<intptr_t>(handle));
        if (posix_fallocate64(fd, 0, static_cast<off64_t>(size)) != 0) {
            if (ftruncate64(fd, static_cast<off64_t>(size)) < 0) {
                return Result<void>::failure(ErrorCode::IOError, "ftruncate64 failed");
            }
        }
        return Result<void>::success();
    }

    Result<void> truncate(void* handle, uint64_t size) override {
        int fd = static_cast<int>(reinterpret_cast<intptr_t>(handle));
        if (ftruncate64(fd, static_cast<off64_t>(size)) < 0) {
            return Result<void>::failure(ErrorCode::IOError, "ftruncate64 failed");
        }
        return Result<void>::success();
    }
};

}

#endif