#pragma once

#include "PAL/IPlatformFileIO.h"
#include "Core/Common/Result.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace ht {

class Win32FileIO : public IPlatformFileIO {
public:
    Result<void*> open(const std::string& path, bool for_write, bool resume) override {
        std::wstring wpath(path.begin(), path.end());
        DWORD access = for_write ? (GENERIC_READ | GENERIC_WRITE) : GENERIC_READ;
        DWORD share = FILE_SHARE_READ | FILE_SHARE_WRITE;
        DWORD disp = for_write ? (resume ? OPEN_ALWAYS : CREATE_ALWAYS) : OPEN_EXISTING;
        DWORD flags = FILE_FLAG_SEQUENTIAL_SCAN;
        HANDLE h = CreateFileW(wpath.c_str(), access, share, nullptr, disp, flags, nullptr);
        if (h == INVALID_HANDLE_VALUE) {
            return Result<void*>::failure(ErrorCode::IOError, "CreateFileW failed");
        }
        return Result<void*>::success(h);
    }

    Result<void> close(void* handle) override {
        if (handle && handle != INVALID_HANDLE_VALUE) {
            CloseHandle(static_cast<HANDLE>(handle));
        }
        return Result<void>::success();
    }

    Result<size_t> read(void* handle, uint64_t offset, void* buffer, size_t size) override {
        OVERLAPPED ov{};
        ov.Offset = static_cast<DWORD>(offset & 0xFFFFFFFF);
        ov.OffsetHigh = static_cast<DWORD>(offset >> 32);
        DWORD bytes_read = 0;
        if (!ReadFile(static_cast<HANDLE>(handle), buffer, static_cast<DWORD>(size), &bytes_read, &ov)) {
            return Result<size_t>::failure(ErrorCode::IOError, "ReadFile failed");
        }
        return Result<size_t>::success(static_cast<size_t>(bytes_read));
    }

    Result<size_t> write(void* handle, uint64_t offset, const void* buffer, size_t size) override {
        OVERLAPPED ov{};
        ov.Offset = static_cast<DWORD>(offset & 0xFFFFFFFF);
        ov.OffsetHigh = static_cast<DWORD>(offset >> 32);
        DWORD bytes_written = 0;
        if (!WriteFile(static_cast<HANDLE>(handle), buffer, static_cast<DWORD>(size), &bytes_written, &ov)) {
            return Result<size_t>::failure(ErrorCode::IOError, "WriteFile failed");
        }
        return Result<size_t>::success(static_cast<size_t>(bytes_written));
    }

    Result<uint64_t> getSize(void* handle) override {
        LARGE_INTEGER li;
        if (!GetFileSizeEx(static_cast<HANDLE>(handle), &li)) {
            return Result<uint64_t>::failure(ErrorCode::IOError, "GetFileSizeEx failed");
        }
        return Result<uint64_t>::success(static_cast<uint64_t>(li.QuadPart));
    }

    Result<void> preallocate(void* handle, uint64_t size) override {
        LARGE_INTEGER li;
        li.QuadPart = static_cast<LONGLONG>(size);
        if (!SetFilePointerEx(static_cast<HANDLE>(handle), li, nullptr, FILE_BEGIN)) {
            return Result<void>::failure(ErrorCode::IOError, "SetFilePointerEx failed");
        }
        if (!SetEndOfFile(static_cast<HANDLE>(handle))) {
            return Result<void>::failure(ErrorCode::IOError, "SetEndOfFile failed");
        }
        return Result<void>::success();
    }

    Result<void> truncate(void* handle, uint64_t size) override {
        return preallocate(handle, size);
    }
};

}

#endif