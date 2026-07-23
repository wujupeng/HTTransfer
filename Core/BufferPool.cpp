#include "BufferPool.h"


namespace ht {

BufferPool::BufferPool(uint64_t pool_size, uint64_t seg_size)
    : pool_size_(pool_size), segment_size_(seg_size) {
    uint32_t count = static_cast<uint32_t>(pool_size / seg_size);
    segments_.resize(count);
    available_.reserve(count);
    for (uint32_t i = 0; i < count; ++i) {
        segments_[i] = std::make_unique<uint8_t[]>(seg_size);
        available_.push_back(i);
    }
}

BufferPool::~BufferPool() = default;

Result<BufferSegment> BufferPool::acquire() {
    std::unique_lock lock(mutex_);
    cv_.wait(lock, [this] { return !available_.empty(); });

    uint32_t id = available_.back();
    available_.pop_back();

    BufferSegment seg;
    seg.data = segments_[id].get();
    seg.capacity = segment_size_;
    seg.size = 0;
    seg.segment_id = id;
    return Result<BufferSegment>::success(std::move(seg));
}

void BufferPool::release(BufferSegment segment) {
    std::unique_lock lock(mutex_);
    available_.push_back(segment.segment_id);
    lock.unlock();
    cv_.notify_one();
}

uint64_t BufferPool::totalSize() const { return pool_size_; }
uint64_t BufferPool::segmentSize() const { return segment_size_; }

uint32_t BufferPool::availableCount() const {
    std::lock_guard lock(mutex_);
    return static_cast<uint32_t>(available_.size());
}

}

