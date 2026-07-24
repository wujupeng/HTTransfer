#include "SpeedController.h"
#include <thread>
#include <condition_variable>

namespace ht {

SpeedController::SpeedController(uint64_t initial_limit_mbps)
    : speed_limit_mbps_(initial_limit_mbps) {
    if (speed_limit_mbps_ > 0) {
        bucket_capacity_ = speed_limit_mbps_ * 1048576;
    } else {
        bucket_capacity_ = UINT64_MAX;
    }
    tokens_ = bucket_capacity_;
    last_refill_ = std::chrono::steady_clock::now();
}

Result<uint64_t> SpeedController::getCurrentSpeedLimit() const {
    std::lock_guard lock(mutex_);
    return Result<uint64_t>::success(speed_limit_mbps_);
}

Result<void> SpeedController::setSpeedLimit(uint64_t limit_mbps) {
    std::lock_guard lock(mutex_);
    speed_limit_mbps_ = limit_mbps;
    if (limit_mbps > 0) {
        bucket_capacity_ = limit_mbps * 1048576;
    } else {
        bucket_capacity_ = UINT64_MAX;
    }
    tokens_ = bucket_capacity_;
    last_refill_ = std::chrono::steady_clock::now();
    return Result<void>::success();
}

Result<void> SpeedController::updateSchedule(const SpeedSchedule& schedule) {
    return setSpeedLimit(schedule.speed_limit);
}

Result<bool> SpeedController::tryConsume(uint64_t bytes) {
    std::lock_guard lock(mutex_);
    refillTokens();
    if (speed_limit_mbps_ == 0) return Result<bool>::success(true);
    if (tokens_ >= bytes) {
        tokens_ -= bytes;
        return Result<bool>::success(true);
    }
    return Result<bool>::success(false);
}

Result<void> SpeedController::waitForTokens(uint64_t bytes) {
    if (speed_limit_mbps_ == 0) return Result<void>::success();

    uint64_t remaining = bytes;
    while (remaining > 0) {
        {
            std::lock_guard lock(mutex_);
            refillTokens();
            uint64_t consume = std::min(remaining, bucket_capacity_);
            if (tokens_ >= consume) {
                tokens_ -= consume;
                remaining -= consume;
                if (remaining == 0) return Result<void>::success();
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return Result<void>::success();
}

void SpeedController::onBandwidthChange(uint64_t available_bandwidth) {
    setSpeedLimit(available_bandwidth / 1048576);
}

void SpeedController::refillTokens() {
    auto now = std::chrono::steady_clock::now();
    auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_refill_).count();
    if (elapsed_ms <= 0) return;

    if (speed_limit_mbps_ > 0) {
        uint64_t refill = (speed_limit_mbps_ * 1048576 * elapsed_ms) / 1000;
        tokens_ = std::min(tokens_ + refill, bucket_capacity_);
    }
    last_refill_ = now;
}

}