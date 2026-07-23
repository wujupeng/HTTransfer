#include "Logger/Logger.h"

namespace ht {

AuditLogRepository::AuditLogRepository(std::shared_ptr<Logger> logger)
    : logger_(std::move(logger)) {}

Result<std::vector<TransferAuditLog>> AuditLogRepository::findByTimeRange(
    const std::string& start, const std::string& end) {
    AuditLogQuery q;
    q.started_after = start;
    q.started_before = end;
    return logger_->queryAuditLogs(q);
}

Result<std::vector<TransferAuditLog>> AuditLogRepository::findBySourcePath(const std::string& path) {
    AuditLogQuery q;
    q.source_path_contains = path;
    return logger_->queryAuditLogs(q);
}

Result<std::vector<TransferAuditLog>> AuditLogRepository::findByResult(AuditResult result) {
    AuditLogQuery q;
    q.result = (result == AuditResult::Success) ? "SUCCESS" : "FAILED";
    return logger_->queryAuditLogs(q);
}

}