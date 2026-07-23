#pragma once

#include <string>
#include <vector>
#include <functional>
#include <memory>
#include "Core/Common/Result.h"

namespace ht {

struct SpeedSchedule {
    std::string schedule_id;
    std::string name;
    std::string time_start;
    std::string time_end;
    uint64_t speed_limit = 0;
    uint32_t priority = 0;
    bool enabled = true;
};

enum class TransferPreset : uint8_t {
    Fast,
    Secure,
    Balanced
};

struct PresetConfig {
    TransferPreset preset;
    uint32_t parallelism = 16;
    bool enable_crc32 = true;
    bool enable_sha256 = true;
    uint64_t speed_limit = 0;
};

class ConfigManager {
public:
    ConfigManager() = default;

    Result<void> loadFromFile(const std::string& path);
    Result<void> saveToFile(const std::string& path);

    PresetConfig getPreset(TransferPreset preset) const;
    void setPreset(TransferPreset preset, const PresetConfig& config);

    using ChangeCallback = std::function<void(const std::string& key)>;
    void registerChangeCallback(ChangeCallback callback);

private:
    std::string config_path_;
    std::vector<PresetConfig> presets_;
    std::vector<ChangeCallback> callbacks_;
};

class PresetRepository {
public:
    PresetConfig get(TransferPreset preset);
    void set(TransferPreset preset, const PresetConfig& config);
    std::vector<PresetConfig> list() const;

private:
    std::vector<PresetConfig> presets_ = {
        {TransferPreset::Fast, 16, false, false, 0},
        {TransferPreset::Secure, 8, true, true, 100},
        {TransferPreset::Balanced, 16, true, true, 0},
    };
};

class ScheduleRepository {
public:
    Result<void> add(const SpeedSchedule& schedule);
    Result<void> remove(const std::string& schedule_id);
    Result<void> update(const SpeedSchedule& schedule);
    std::vector<SpeedSchedule> list() const;
    SpeedSchedule findActive(const std::string& current_time) const;

private:
    std::vector<SpeedSchedule> schedules_;
};

}