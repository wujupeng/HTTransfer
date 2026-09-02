#pragma once

#include "PAL/IAsyncIoEngine.h"
#include "Core/Common/Result.h"

#ifdef __linux__
#include <sys/epoll.h>
#include <vector>
#include <thread>
#include <atomic>
#include <functional>

namespace ht {

class EpollEngine : public IAsyncIoEngine {
public:
    EpollEngine() = default;
    ~EpollEngine() override { shutdown(); }

    Result<void> initialize(uint32_t worker_threads) override {
        epoll_fd_ = epoll_create1(0);
        if (epoll_fd_ < 0) {
            return Result<void>::failure(ErrorCode::IOError, "epoll_create1 failed");
        }
        running_ = true;
        for (uint32_t i = 0; i < worker_threads; ++i) {
            workers_.emplace_back(&EpollEngine::workerLoop, this);
        }
        return Result<void>::success();
    }

    Result<void> shutdown() override {
        if (!running_) return Result<void>::success();
        running_ = false;
        for (auto& t : workers_) {
            if (t.joinable()) t.join();
        }
        workers_.clear();
        if (epoll_fd_ >= 0) {
            close(epoll_fd_);
            epoll_fd_ = -1;
        }
        return Result<void>::success();
    }

    Result<void> associateFile(FileHandle, uint64_t) override {
        return Result<void>::success();
    }

    Result<void> submitRead(FileHandle file_handle, uint64_t offset, uint32_t size,
                            uint8_t* buffer, uint64_t completion_key) override {
        int fd = static_cast<int>(reinterpret_cast<intptr_t>(file_handle));
        ssize_t ret = pread64(fd, buffer, size, static_cast<off64_t>(offset));
        if (callback_) {
            if (ret < 0) {
                callback_(completion_key, 0, static_cast<uint64_t>(-ret));
            } else {
                callback_(completion_key, static_cast<uint32_t>(ret), 0);
            }
        }
        return Result<void>::success();
    }

    Result<void> submitWrite(FileHandle file_handle, uint64_t offset, uint32_t size,
                             const uint8_t* buffer, uint64_t completion_key) override {
        int fd = static_cast<int>(reinterpret_cast<intptr_t>(file_handle));
        ssize_t ret = pwrite64(fd, buffer, size, static_cast<off64_t>(offset));
        if (callback_) {
            if (ret < 0) {
                callback_(completion_key, 0, static_cast<uint64_t>(-ret));
            } else {
                callback_(completion_key, static_cast<uint32_t>(ret), 0);
            }
        }
        return Result<void>::success();
    }

    void registerCompletionCallback(IoCompletionCallback callback) override {
        callback_ = std::move(callback);
    }

private:
    void workerLoop() {
        while (running_) {
            struct epoll_event events[16];
            int n = epoll_wait(epoll_fd_, events, 16, 100);
            if (n < 0 || !running_) break;
        }
    }

    int epoll_fd_ = -1;
    std::atomic<bool> running_{false};
    std::vector<std::thread> workers_;
    IoCompletionCallback callback_;
};

}

#endif