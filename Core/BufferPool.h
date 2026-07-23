#pragma once

#include <cstdint>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <memory>
#include "Core/Common/Types.h"
#include "Core/Common/Result.h"
#include "Core/Common/Constants.h"

namespace ht {

struct BufferSegment {
    uint8_t* data = nullptr;
    uint64_t capacity = 0;
    uint64_t size = 0;
    uint32_t segment_id = 0;

    BufferSegment() = default;
    BufferSegment(const BufferSegment&) = delete;
    BufferSegment& operator=(const BufferSegment&) = delete;
    BufferSegment(BufferSegment&& other) noexcept
        : data(other.data), capacity(other.capacity),
          size(other.size), segment_id(other.segment_id) {
        other.data = nullptr;
        other.capacity = 0;
        other.size = 0;
        other.segment_id = 0;
    }
    BufferSegment& operator=(BufferSegment&& other) noexcept {
        if (this != &other) {
            data = other.data;
            capacity = other.capacity;
            size = other.size;
            segment_id = other.segment_id;
            other.data = nullptr;
            other.capacity = 0;
            other.size = 0;
            other.segment_id = 0;
        }
        return *this;
    }
};

class IBufferPool {
public:
    virtual ~IBufferPool() = default;
    virtual Result<BufferSegment> acquire() = 0;
    virtual void release(BufferSegment segment) = 0;
    virtual uint64_t totalSize() const = 0;
    virtual uint64_t segmentSize() const = 0;
    virtual uint32_t availableCount() const = 0;
};

class BufferPool : public IBufferPool {
public:
    explicit BufferPool(uint64_t pool_size = kBufferPoolSize,
                        uint64_t seg_size = kChunkSize);
    ~BufferPool() override;

    Result<BufferSegment> acquire() override;
    void release(BufferSegment segment) override;
    uint64_t totalSize() const override;
    uint64_t segmentSize() const override;
    uint32_t availableCount() const override;

private:
    uint64_t pool_size_;
    uint64_t segment_size_;
    std::vector<std::unique_ptr<uint8_t[]>> segments_;
    std::vector<uint32_t> available_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
};

class BufferGuard {
public:
    BufferGuard(std::shared_ptr<IBufferPool> pool, BufferSegment segment)
        : pool_(std::move(pool)), segment_(std::move(segment)) {}

    ~BufferGuard() {
        if (pool_ && segment_.data) {
            pool_->release(std::move(segment_));
        }
    }

    BufferGuard(const BufferGuard&) = delete;
    BufferGuard& operator=(const BufferGuard&) = delete;

    BufferGuard(BufferGuard&& other) noexcept
        : pool_(std::move(other.pool_)), segment_(std::move(other.segment_)) {}

    BufferGuard& operator=(BufferGuard&& other) noexcept {
        if (this != &other) {
            if (pool_ && segment_.data) {
                pool_->release(std::move(segment_));
            }
            pool_ = std::move(other.pool_);
            segment_ = std::move(other.segment_);
        }
        return *this;
    }

    BufferSegment& get() { return segment_; }
    const BufferSegment& get() const { return segment_; }
    uint8_t* data() { return segment_.data; }
    uint64_t size() const { return segment_.size; }

private:
    std::shared_ptr<IBufferPool> pool_;
    BufferSegment segment_;
};

}