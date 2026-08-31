#pragma once

#include <string>
#include <filesystem>
#include <chrono>
#include <functional>
#include <unordered_map>
#include <cstdint>
#include "Core/Common/Result.h"

namespace ht {

enum class ChangeType : uint8_t {
    Created,
    Modified
};

enum class BackupStatus : uint8_t {
    Pending,
    BackingUp,
    Completed,
    Failed,
    Skipped
};

enum class WatchStatus : uint8_t {
    Idle,
    Running,
    Stopped,
    Error
};

struct FileMeta {
    uint64_t size = 0;
    std::filesystem::file_time_type modify_time{};
};

struct FileChangeEvent {
    std::string file_path;
    ChangeType change_type = ChangeType::Created;
    std::chrono::system_clock::time_point detected_at;
    uint64_t file_size = 0;
    std::filesystem::file_time_type modify_time{};
    BackupStatus backup_status = BackupStatus::Pending;
};

struct WatchStatistics {
    WatchStatus status = WatchStatus::Idle;
    int scan_interval = 0;
    uint64_t total_detected = 0;
    uint64_t total_backed_up = 0;
    std::chrono::system_clock::time_point last_scan_time;
};

using FileSnapshot = std::unordered_map<std::string, FileMeta>;

using WatchStatusCallback = std::function<void(WatchStatus, const WatchStatistics&)>;

}