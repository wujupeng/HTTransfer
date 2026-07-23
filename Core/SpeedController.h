#pragma once

#include <cstdint>
#include <chrono>
#include <mutex>
#include <memory>
#include <functional>
#include "Core/Common/Result.h"
#include "Core/Common/Constants.h"
#include "Config/ConfigManager.h"

namespace ht {

class ISpeedController {
public:
    virtual ~ISpeedController() = default;
    virtual Result<uint64_t> getCurrentSpeedLimit() const = 0;
    virtual Result<void> setSpeedLimit(uint64_t limit_mbps) = 0;
    virtual Result<void> updateSchedule(const SpeedSchedule& schedule) = 0;
    virtual Result<bool> tryConsume(uint64_t bytes) = 0;
    virtual Result<void> waitForTokens(uint64_t bytes) = 0;
    virtual void onBandwidthChange(uint64_t available_bandwidth) = 0;
};

class SpeedController : public ISpeedController {
public:
    explicit SpeedController(uint64_t initial_limit_mbps = 0);

    Result<uint64_t> getCurrentSpeedLimit() const override;
    Result<void> setSpeedLimit(uint64_t limit_mbps) override;
    Result<void> updateSchedule(const SpeedSchedule& schedule) override;
    Result<bool> tryConsume(uint64_t bytes) override;
    Result<void> waitForTokens(uint64_t bytes) override;
    void onBandwidthChange(uint64_t available_bandwidth) override;

private:
    void refillTokens();
    uint64_t speed_limit_mbps_;
    uint64_t bucket_capacity_;
    uint64_t tokens_;
    std::chrono::steady_clock::time_point last_refill_;
    mutable std::mutex mutex_;
};

}