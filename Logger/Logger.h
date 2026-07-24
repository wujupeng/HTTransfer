#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <mutex>
#include <unordered_map>
#include "ILogger.h"

struct sqlite3;
struct sqlite3_stmt;

namespace ht {

class SQLiteDB {
public:
    SQLiteDB() = default;
    ~SQLiteDB();

    SQLiteDB(const SQLiteDB&) = delete;
    SQLiteDB& operator=(const SQLiteDB&) = delete;

    Result<void> open(const std::string& db_path);
    void close();

    Result<void> execute(const std::string& sql);
    Result<std::vector<std::vector<std::string>>> query(const std::string& sql);
    Result<void> executePrepared(const std::string& sql, const std::vector<std::string>& params);
    Result<std::vector<std::vector<std::string>>> queryPrepared(const std::string& sql, const std::vector<std::string>& params);

    sqlite3* handle() const { return db_; }

private:
    sqlite3* db_ = nullptr;
};

class Logger : public ILogger {
public:
    explicit Logger(const std::string& db_path = "ht_audit.db");
    ~Logger() override = default;

    void log(Level level,
             const std::string& task_id,
             const std::string& message,
             const std::string& chunk_id = "",
             const std::source_location& loc = std::source_location::current()) override;

    Result<void> writeAuditLog(const TransferAuditLog& audit) override;
    Result<std::vector<TransferAuditLog>> queryAuditLogs(const AuditLogQuery& query) override;

private:
    Result<void> initSchema();
    Result<void> writeFallbackJson(const TransferAuditLog& audit);

    SQLiteDB db_;
    std::string db_path_;
    std::mutex write_mutex_;
};

class AuditLogRepository {
public:
    explicit AuditLogRepository(std::shared_ptr<Logger> logger);
    Result<std::vector<TransferAuditLog>> findByTimeRange(
        const std::string& start, const std::string& end);
    Result<std::vector<TransferAuditLog>> findBySourcePath(const std::string& path);
    Result<std::vector<TransferAuditLog>> findByResult(AuditResult result);

private:
    std::shared_ptr<Logger> logger_;
};

}