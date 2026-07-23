#include "ConfigManager.h"
#include <fstream>
#include <algorithm>
#include <sstream>

namespace ht {

Result<void> ConfigManager::loadFromFile(const std::string& path) {
    config_path_ = path;
    return Result<void>::success();
}

Result<void> ConfigManager::saveToFile(const std::string& path) {
    return Result<void>::success();
}

PresetConfig ConfigManager::getPreset(TransferPreset preset) const {
    for (const auto& p : presets_) {
        if (p.preset == preset) return p;
    }
    return PresetConfig{preset, 16, true, true, 0};
}

void ConfigManager::setPreset(TransferPreset preset, const PresetConfig& config) {
    for (auto& p : presets_) {
        if (p.preset == preset) {
            p = config;
            return;
        }
    }
    presets_.push_back(config);
    for (auto& cb : callbacks_) cb("preset");
}

void ConfigManager::registerChangeCallback(ChangeCallback callback) {
    callbacks_.push_back(std::move(callback));
}

PresetConfig PresetRepository::get(TransferPreset preset) {
    for (auto& p : presets_) {
        if (p.preset == preset) return p;
    }
    return PresetConfig{preset, 16, true, true, 0};
}

void PresetRepository::set(TransferPreset preset, const PresetConfig& config) {
    for (auto& p : presets_) {
        if (p.preset == preset) { p = config; return; }
    }
    presets_.push_back(config);
}

std::vector<PresetConfig> PresetRepository::list() const { return presets_; }

Result<void> ScheduleRepository::add(const SpeedSchedule& schedule) {
    schedules_.push_back(schedule);
    return Result<void>::success();
}

Result<void> ScheduleRepository::remove(const std::string& schedule_id) {
    auto it = std::remove_if(schedules_.begin(), schedules_.end(),
        [&](const SpeedSchedule& s) { return s.schedule_id == schedule_id; });
    if (it == schedules_.end()) {
        return Result<void>::failure(ErrorCode::ConfigError, "Schedule not found");
    }
    schedules_.erase(it, schedules_.end());
    return Result<void>::success();
}

Result<void> ScheduleRepository::update(const SpeedSchedule& schedule) {
    for (auto& s : schedules_) {
        if (s.schedule_id == schedule.schedule_id) { s = schedule; return Result<void>::success(); }
    }
    return Result<void>::failure(ErrorCode::ConfigError, "Schedule not found");
}

std::vector<SpeedSchedule> ScheduleRepository::list() const { return schedules_; }

SpeedSchedule ScheduleRepository::findActive(const std::string& current_time) const {
    SpeedSchedule best{};
    bool found = false;
    for (const auto& s : schedules_) {
        if (!s.enabled) continue;
        if (s.time_start <= current_time && current_time < s.time_end) {
            if (!found || s.priority < best.priority ||
                (s.priority == best.priority && s.speed_limit < best.speed_limit)) {
                best = s;
                found = true;
            }
        }
    }
    return best;
}

}