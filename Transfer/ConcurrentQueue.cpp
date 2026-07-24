#include "ConcurrentQueue.h"

namespace ht {

ConcurrentQueue::ConcurrentQueue(size_t capacity)
    : capacity_(capacity) {}

bool ConcurrentQueue::push(DataChunk&& chunk) {
    std::unique_lock lock(mutex_);
    not_full_.wait(lock, [this] {
        return queue_.size() < capacity_ || writer_error_.load() || shutdown_.load();
    });
    if (writer_error_.load() || shutdown_.load()) {
        return false;
    }
    queue_.push(std::move(chunk));
    not_empty_.notify_one();
    return true;
}

bool ConcurrentQueue::pop(DataChunk& chunk) {
    std::unique_lock lock(mutex_);
    not_empty_.wait(lock, [this] {
        return !queue_.empty() || shutdown_.load() || active_readers_.load() == 0;
    });
    if (queue_.empty()) {
        return false;
    }
    chunk = std::move(queue_.front());
    queue_.pop();
    not_full_.notify_one();
    return true;
}

void ConcurrentQueue::signalShutdown() {
    shutdown_.store(true);
    not_empty_.notify_all();
    not_full_.notify_all();
}

void ConcurrentQueue::signalWriterError() {
    writer_error_.store(true);
    not_full_.notify_all();
}

void ConcurrentQueue::decrementActiveReaders() {
    uint32_t prev = active_readers_.fetch_sub(1);
    if (prev == 0) {
        active_readers_.store(0);
        return;
    }
    if (prev == 1) {
        {
            std::lock_guard lock(mutex_);
            if (queue_.empty()) {
                shutdown_.store(true);
            }
        }
        not_empty_.notify_all();
        not_full_.notify_all();
    }
}

size_t ConcurrentQueue::size() const {
    std::lock_guard lock(mutex_);
    return queue_.size();
}

}