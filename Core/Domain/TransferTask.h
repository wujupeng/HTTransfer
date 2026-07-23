#pragma once

#include <string>
#include <chrono>
#include <random>
#include "Core/Common/Types.h"
#include "Config/ConfigManager.h"

namespace ht {

enum class TaskStatus : uint8_t {
    Created,
    Queued,
    Stabilizing,
    Transferring,
    Paused,
    Verifying,
    Completed,
    Failed,
    Cancelled
};

enum class ProtocolType : uint8_t {
    SMB,
    HTTPS,
    FTP,
    SFTP
};

struct TransferTask {
    std::string task_id;
    std::string source_path;
    std::string target_path;

    TaskStatus status = TaskStatus::Created;
    TransferPreset preset = TransferPreset::Balanced;
    ProtocolType protocol = ProtocolType::SMB;

    uint32_t parallelism = 16;
    uint64_t speed_limit = 0;

    uint64_t total_bytes = 0;
    uint64_t transferred_bytes = 0;

    std::chrono::system_clock::time_point created_at;
    std::chrono::system_clock::time_point updated_at;

    std::string error_code;
    std::string error_message;
};

inline ProtocolType detectProtocol(const std::string& path) {
    if (path.find("sftp://") == 0) return ProtocolType::SFTP;
    if (path.find("ftp://") == 0) return ProtocolType::FTP;
    if (path.find("http://") == 0 || path.find("https://") == 0) return ProtocolType::HTTPS;
    return ProtocolType::SMB;
}

inline std::string generateTaskId() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::tm tm_buf{};
#ifdef _WIN32
    localtime_s(&tm_buf, &time_t);
#else
    localtime_r(&time_t, &tm_buf);
#endif
    char date_buf[16];
    std::strftime(date_buf, sizeof(date_buf), "%Y%m%d", &tm_buf);

    static const char alphanum[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    std::random_device rd;
    char random_part[7];
    for (int i = 0; i < 6; ++i) {
        random_part[i] = alphanum[rd() % 36];
    }
    random_part[6] = '\0';

    return std::string("HT-") + date_buf + "-" + random_part;
}

}