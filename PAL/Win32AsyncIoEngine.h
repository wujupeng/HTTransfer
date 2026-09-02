#pragma once

#include "PAL/IAsyncIoEngine.h"
#include "Core/Common/Result.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <winsock2.h>
#include <vector>
#include <thread>
#include <atomic>

namespace ht {

class Win32AsyncIoEngine : public IAsyncIoEngine {
public:
    Win32AsyncIoEngine() = default;
    ~Win32AsyncIoEngine() override { shutdown(); }

    Result<void> initialize(uint32_t worker_threads) override {
        if (iocp_port_ != INVALID_HANDLE_VALUE) {
            return Result<void>::failure(ErrorCode::ConfigError, "IOCP already initialized");
        }
        iocp_port_ = CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, worker_threads);
        if (iocp_port_ == nullptr) {
            return Result<void>::failure(ErrorCode::IOError, "Failed to create IOCP port");
        }
        running_ = true;
        for (uint32_t i = 0; i < worker_threads; ++i) {
            workers_.emplace_back(&Win32AsyncIoEngine::workerLoop, this);
        }
        return Result<void>::success();
    }

    Result<void> shutdown() override {
        if (!running_) return Result<void>::success();
        running_ = false;
        for (size_t i = 0; i < workers_.size(); ++i) {
            PostQueuedCompletionStatus(iocp_port_, 0, 0, nullptr);
        }
        for (auto& t : workers_) {
            if (t.joinable()) t.join();
        }
        workers_.clear();
        if (iocp_port_ != INVALID_HANDLE_VALUE) {
            CloseHandle(iocp_port_);
            iocp_port_ = INVALID_HANDLE_VALUE;
        }
        return Result<void>::success();
    }

    Result<void> associateFile(FileHandle file_handle, uint64_t completion_key) override {
        HANDLE result = CreateIoCompletionPort(
            static_cast<HANDLE>(file_handle), iocp_port_, static_cast<ULONG_PTR>(completion_key), 0);
        if (result == nullptr) {
            return Result<void>::failure(ErrorCode::IOError, "Failed to associate file with IOCP");
        }
        return Result<void>::success();
    }

    Result<void> submitRead(FileHandle file_handle, uint64_t offset, uint32_t size,
                            uint8_t* buffer, uint64_t completion_key) override {
        auto overlapped = new OVERLAPPED{};
        overlapped->Offset = static_cast<DWORD>(offset & 0xFFFFFFFF);
        overlapped->OffsetHigh = static_cast<DWORD>(offset >> 32);
        DWORD bytes_read = 0;
        BOOL ok = ReadFile(static_cast<HANDLE>(file_handle), buffer, size, &bytes_read, overlapped);
        if (!ok && GetLastError() != ERROR_IO_PENDING) {
            delete overlapped;
            return Result<void>::failure(ErrorCode::IOError, "ReadFile failed");
        }
        return Result<void>::success();
    }

    Result<void> submitWrite(FileHandle file_handle, uint64_t offset, uint32_t size,
                             const uint8_t* buffer, uint64_t completion_key) override {
        auto overlapped = new OVERLAPPED{};
        overlapped->Offset = static_cast<DWORD>(offset & 0xFFFFFFFF);
        overlapped->OffsetHigh = static_cast<DWORD>(offset >> 32);
        DWORD bytes_written = 0;
        BOOL ok = WriteFile(static_cast<HANDLE>(file_handle), buffer, size, &bytes_written, overlapped);
        if (!ok && GetLastError() != ERROR_IO_PENDING) {
            delete overlapped;
            return Result<void>::failure(ErrorCode::IOError, "WriteFile failed");
        }
        return Result<void>::success();
    }

    void registerCompletionCallback(IoCompletionCallback callback) override {
        callback_ = std::move(callback);
    }

private:
    void workerLoop() {
        while (running_) {
            DWORD bytes_transferred = 0;
            ULONG_PTR completion_key = 0;
            LPOVERLAPPED overlapped = nullptr;
            BOOL ok = GetQueuedCompletionStatus(iocp_port_, &bytes_transferred, &completion_key, &overlapped, INFINITE);
            if (!running_ && completion_key == 0 && overlapped == nullptr) break;
            DWORD error = ok ? 0 : GetLastError();
            if (callback_) callback_(static_cast<uint64_t>(completion_key), bytes_transferred, static_cast<uint64_t>(error));
            if (overlapped) delete overlapped;
        }
    }

    HANDLE iocp_port_ = INVALID_HANDLE_VALUE;
    std::vector<std::thread> workers_;
    std::atomic<bool> running_{false};
    IoCompletionCallback callback_;
};

}

#endif