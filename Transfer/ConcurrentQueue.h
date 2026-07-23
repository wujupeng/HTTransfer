#pragma once

#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include "Transfer/DataChunk.h"

namespace ht {

class ConcurrentQueue {
public:
    explicit ConcurrentQueue(size_t capacity = 64);

    bool push(DataChunk&& chunk);
    bool pop(DataChunk& chunk);

    void signalShutdown();
    void signalWriterError();
    void decrementActiveReaders();
    void setActiveReaders(uint32_t count) { active_readers_.store(count); }

    bool isWriterError() const { return writer_error_.load(); }
    bool isShutdown() const { return shutdown_.load(); }
    size_t size() const;

    ConcurrentQueue(const ConcurrentQueue&) = delete;
    ConcurrentQueue& operator=(const ConcurrentQueue&) = delete;
    ConcurrentQueue(ConcurrentQueue&&) = delete;
    ConcurrentQueue& operator=(ConcurrentQueue&&) = delete;

private:
    std::queue<DataChunk> queue_;
    mutable std::mutex mutex_;
    std::condition_variable not_empty_;
    std::condition_variable not_full_;
    size_t capacity_;
    std::atomic<bool> shutdown_{false};
    std::atomic<bool> writer_error_{false};
    std::atomic<uint32_t> active_readers_{0};
};

}