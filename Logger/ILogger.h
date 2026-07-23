#pragma once

#include <string>
#include <vector>
#include <functional>
#include <chrono>
#include <source_location>
#include <memory>
#include "Core/Common/Result.h"

namespace ht {

enum class AuditResult : uint8_t {
    Success,
    Failed
};

struct TransferAuditLog {
    int64_t log_id = 0;
    std::string task_id;
    std::string source_path;
    std::string target_path;
    std::string username;
    std::string start_hash;
    std::string end_hash;
    AuditResult result = AuditResult::Success;
    double speed_peak = 0.0;
    double speed_average = 0.0;
    std::chrono::system_clock::time_point started_at;
    std::chrono::system_clock::time_point completed_at;
    std::string failure_reason;
};

struct AuditLogQuery {
    std::string task_id;
    std::string source_path_contains;
    std::string started_after;
    std::string started_before;
    std::string result;
    int64_t limit = 100;
    int64_t offset = 0;
};

class ILogger {
public:
    virtual ~ILogger() = default;

    enum class Level { Debug, Info, Warning, Error, Critical };

    virtual void log(Level level,
                     const std::string& task_id,
                     const std::string& message,
                     const std::string& chunk_id = "",
                     const std::source_location& loc = std::source_location::current()) = 0;

    virtual Result<void> writeAuditLog(const TransferAuditLog& audit) = 0;
    virtual Result<std::vector<TransferAuditLog>> queryAuditLogs(const AuditLogQuery& query) = 0;
};

}