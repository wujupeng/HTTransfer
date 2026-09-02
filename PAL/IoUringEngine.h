#pragma once

#include "PAL/IAsyncIoEngine.h"
#include "Core/Common/Result.h"

#ifdef __linux__
#include <liburing.h>
#include <vector>
#include <thread>
#include <atomic>
#include <unordered_map>

namespace ht {

class IoUringEngine : public IAsyncIoEngine {
public:
    static bool isAvailable() {
        struct io_uring ring;
        if (io_uring_queue_init(8, &ring, 0) == 0) {
            io_uring_queue_exit(&ring);
            return true;
        }
        return false;
    }

    IoUringEngine() = default;
    ~IoUringEngine() override { shutdown(); }

    Result<void> initialize(uint32_t worker_threads) override {
        if (io_uring_queue_init(256, &ring_, 0) != 0) {
            return Result<void>::failure(ErrorCode::IOError, "io_uring_queue_init failed");
        }
        running_ = true;
        worker_ = std::thread(&IoUringEngine::workerLoop, this);
        return Result<void>::success();
    }

    Result<void> shutdown() override {
        if (!running_) return Result<void>::success();
        running_ = false;
        io_uring_submit(&ring_);
        if (worker_.joinable()) worker_.join();
        io_uring_queue_exit(&ring_);
        return Result<void>::success();
    }

    Result<void> associateFile(FileHandle, uint64_t) override {
        return Result<void>::success();
    }

    Result<void> submitRead(FileHandle file_handle, uint64_t offset, uint32_t size,
                            uint8_t* buffer, uint64_t completion_key) override {
        struct io_uring_sqe* sqe = io_uring_get_sqe(&ring_);
        if (!sqe) {
            return Result<void>::failure(ErrorCode::IOError, "io_uring_get_sqe failed");
        }
        io_uring_prep_read(sqe, static_cast<int>(reinterpret_cast<intptr_t>(file_handle)),
                           buffer, size, static_cast<off_t>(offset));
        io_uring_sqe_set_data(sqe, reinterpret_cast<void*>(completion_key));
        io_uring_submit(&ring_);
        return Result<void>::success();
    }

    Result<void> submitWrite(FileHandle file_handle, uint64_t offset, uint32_t size,
                             const uint8_t* buffer, uint64_t completion_key) override {
        struct io_uring_sqe* sqe = io_uring_get_sqe(&ring_);
        if (!sqe) {
            return Result<void>::failure(ErrorCode::IOError, "io_uring_get_sqe failed");
        }
        io_uring_prep_write(sqe, static_cast<int>(reinterpret_cast<intptr_t>(file_handle)),
                            buffer, size, static_cast<off_t>(offset));
        io_uring_sqe_set_data(sqe, reinterpret_cast<void*>(completion_key));
        io_uring_submit(&ring_);
        return Result<void>::success();
    }

    void registerCompletionCallback(IoCompletionCallback callback) override {
        callback_ = std::move(callback);
    }

private:
    void workerLoop() {
        while (running_) {
            struct io_uring_cqe* cqe;
            int ret = io_uring_wait_cqe(&ring_, &cqe);
            if (ret < 0 || !running_) break;
            uint64_t completion_key = reinterpret_cast<uint64_t>(io_uring_cqe_get_data(cqe));
            uint32_t bytes = cqe->res > 0 ? static_cast<uint32_t>(cqe->res) : 0;
            uint64_t error = cqe->res < 0 ? static_cast<uint64_t>(-cqe->res) : 0;
            if (callback_) callback_(completion_key, bytes, error);
            io_uring_cqe_seen(&ring_, cqe);
        }
    }

    struct io_uring ring_;
    std::atomic<bool> running_{false};
    std::thread worker_;
    IoCompletionCallback callback_;
};

}

#endif