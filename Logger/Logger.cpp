#include "Logger.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <mutex>
#include <format>
#include <ctime>
#include <cstring>

#include <sqlite3.h>

namespace ht {

static const char* kCreateTableSQL = R"(
CREATE TABLE IF NOT EXISTS transfer_audit_log (
    log_id          INTEGER PRIMARY KEY AUTOINCREMENT,
    task_id         TEXT    NOT NULL,
    source_path     TEXT    NOT NULL,
    target_path     TEXT    NOT NULL,
    username        TEXT    NOT NULL,
    start_hash      TEXT    DEFAULT '',
    end_hash        TEXT    DEFAULT '',
    result          TEXT    NOT NULL CHECK(result IN ('SUCCESS', 'FAILED')),
    speed_peak      REAL    DEFAULT 0.0,
    speed_average   REAL    DEFAULT 0.0,
    started_at      TEXT    NOT NULL,
    completed_at    TEXT    DEFAULT '',
    failure_reason  TEXT    DEFAULT '',
    created_at      TEXT    DEFAULT (datetime('now'))
);

CREATE INDEX IF NOT EXISTS idx_audit_task_id ON transfer_audit_log(task_id);
CREATE INDEX IF NOT EXISTS idx_audit_started_at ON transfer_audit_log(started_at);
CREATE INDEX IF NOT EXISTS idx_audit_result ON transfer_audit_log(result);
CREATE INDEX IF NOT EXISTS idx_audit_source_path ON transfer_audit_log(source_path);
)";

SQLiteDB::~SQLiteDB() { close(); }

Result<void> SQLiteDB::open(const std::string& db_path) {
    if (db_) close();
    int rc = sqlite3_open(db_path.c_str(), &db_);
    if (rc != SQLITE_OK) {
        std::string err = sqlite3_errmsg(db_);
        sqlite3_close(db_);
        db_ = nullptr;
        return Result<void>::failure(ErrorCode::IOError, err);
    }
    sqlite3_exec(db_, "PRAGMA journal_mode=WAL;", nullptr, nullptr, nullptr);
    return Result<void>::success();
}

void SQLiteDB::close() {
    if (db_) {
        sqlite3_close(db_);
        db_ = nullptr;
    }
}

Result<void> SQLiteDB::execute(const std::string& sql) {
    if (!db_) return Result<void>::failure(ErrorCode::IOError, "Database not open");
    char* err_msg = nullptr;
    int rc = sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &err_msg);
    if (rc != SQLITE_OK) {
        std::string err(err_msg);
        sqlite3_free(err_msg);
        return Result<void>::failure(ErrorCode::IOError, err);
    }
    return Result<void>::success();
}

Result<std::vector<std::vector<std::string>>> SQLiteDB::query(const std::string& sql) {
    if (!db_) return Result<std::vector<std::vector<std::string>>>::failure(ErrorCode::IOError, "Database not open");
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return Result<std::vector<std::vector<std::string>>>::failure(ErrorCode::IOError, sqlite3_errmsg(db_));
    }
    std::vector<std::vector<std::string>> rows;
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        int cols = sqlite3_column_count(stmt);
        std::vector<std::string> row(cols);
        for (int i = 0; i < cols; ++i) {
            const char* val = reinterpret_cast<const char*>(sqlite3_column_text(stmt, i));
            row[i] = val ? val : "";
        }
        rows.push_back(std::move(row));
    }
    sqlite3_finalize(stmt);
    return Result<std::vector<std::vector<std::string>>>::success(std::move(rows));
}

Result<void> SQLiteDB::executePrepared(const std::string& sql, const std::vector<std::string>& params) {
    if (!db_) return Result<void>::failure(ErrorCode::IOError, "Database not open");
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return Result<void>::failure(ErrorCode::IOError, sqlite3_errmsg(db_));
    }
    for (int i = 0; i < static_cast<int>(params.size()); ++i) {
        sqlite3_bind_text(stmt, i + 1, params[i].c_str(), -1, SQLITE_TRANSIENT);
    }
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE && rc != SQLITE_ROW) {
        return Result<void>::failure(ErrorCode::IOError, sqlite3_errmsg(db_));
    }
    return Result<void>::success();
}

Result<std::vector<std::vector<std::string>>> SQLiteDB::queryPrepared(const std::string& sql, const std::vector<std::string>& params) {
    if (!db_) return Result<std::vector<std::vector<std::string>>>::failure(ErrorCode::IOError, "Database not open");
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return Result<std::vector<std::vector<std::string>>>::failure(ErrorCode::IOError, sqlite3_errmsg(db_));
    }
    for (int i = 0; i < static_cast<int>(params.size()); ++i) {
        sqlite3_bind_text(stmt, i + 1, params[i].c_str(), -1, SQLITE_TRANSIENT);
    }
    std::vector<std::vector<std::string>> rows;
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        int cols = sqlite3_column_count(stmt);
        std::vector<std::string> row(cols);
        for (int i = 0; i < cols; ++i) {
            const char* val = reinterpret_cast<const char*>(sqlite3_column_text(stmt, i));
            row[i] = val ? val : "";
        }
        rows.push_back(std::move(row));
    }
    sqlite3_finalize(stmt);
    return Result<std::vector<std::vector<std::string>>>::success(std::move(rows));
}

Logger::Logger(const std::string& db_path) : db_path_(db_path) {
    auto r = db_.open(db_path);
    if (r.isOk()) initSchema();
}

void Logger::log(Level level, const std::string& task_id, const std::string& message,
                 const std::string& chunk_id, const std::source_location& loc) {
    std::lock_guard lock(write_mutex_);
    const char* level_str[] = {"DEBUG", "INFO", "WARN", "ERROR", "CRITICAL"};
    auto idx = static_cast<int>(level);
    auto now = std::chrono::system_clock::now();
    auto time_t_val = std::chrono::system_clock::to_time_t(now);
    std::tm tm_buf{};
#ifdef _WIN32
    localtime_s(&tm_buf, &time_t_val);
#else
    localtime_r(&time_t_val, &tm_buf);
#endif
    char time_buf[32];
    std::strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", &tm_buf);
    std::string line = std::format("[{}] [{}] [{}] {}:{} - {}",
        time_buf, level_str[idx], task_id, loc.file_name(), loc.line(), message);
    if (!chunk_id.empty()) line += std::format(" [chunk={}]", chunk_id);

    std::cerr << line << std::endl;

    char date_buf[16];
    std::strftime(date_buf, sizeof(date_buf), "%Y-%m-%d", &tm_buf);
    std::string log_file = std::string(date_buf) + ".log";
    std::ofstream f(log_file, std::ios::app);
    if (f) f << line << std::endl;
}

Result<void> Logger::writeAuditLog(const TransferAuditLog& audit) {
    std::lock_guard lock(write_mutex_);
    const char* sql =
        "INSERT INTO transfer_audit_log "
        "(task_id, source_path, target_path, username, start_hash, end_hash, "
        "result, speed_peak, speed_average, started_at, completed_at, failure_reason) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)";
    std::string result_str = (audit.result == AuditResult::Success) ? "SUCCESS" : "FAILED";
    std::string peak_str = std::to_string(audit.speed_peak);
    std::string avg_str = std::to_string(audit.speed_average);
    std::vector<std::string> params = {
        audit.task_id, audit.source_path, audit.target_path, audit.username,
        audit.start_hash, audit.end_hash, result_str,
        peak_str, avg_str, "", "", audit.failure_reason
    };
    auto r = db_.executePrepared(sql, params);
    if (r.isErr()) { writeFallbackJson(audit); return r; }
    return Result<void>::success();
}

Result<std::vector<TransferAuditLog>> Logger::queryAuditLogs(const AuditLogQuery& query) {
    std::string sql = "SELECT * FROM transfer_audit_log WHERE 1=1";
    std::vector<std::string> params;
    if (!query.task_id.empty()) { sql += " AND task_id=?"; params.push_back(query.task_id); }
    if (!query.result.empty()) { sql += " AND result=?"; params.push_back(query.result); }
    sql += " ORDER BY log_id DESC LIMIT ? OFFSET ?";
    params.push_back(std::to_string(query.limit));
    params.push_back(std::to_string(query.offset));
    auto r = db_.queryPrepared(sql, params);
    if (r.isErr()) return Result<std::vector<TransferAuditLog>>::failure(r.errorCode(), r.errorMessage());
    std::vector<TransferAuditLog> logs;
    for (const auto& row : r.value()) {
        TransferAuditLog log;
        if (row.size() >= 14) {
            log.log_id = std::stoll(row[0]);
            log.task_id = row[1]; log.source_path = row[2]; log.target_path = row[3];
            log.username = row[4]; log.start_hash = row[5]; log.end_hash = row[6];
            log.result = (row[7] == "SUCCESS") ? AuditResult::Success : AuditResult::Failed;
            log.speed_peak = std::stod(row[8]); log.speed_average = std::stod(row[9]);
        }
        logs.push_back(std::move(log));
    }
    return Result<std::vector<TransferAuditLog>>::success(std::move(logs));
}

Result<void> Logger::initSchema() { return db_.execute(kCreateTableSQL); }

Result<void> Logger::writeFallbackJson(const TransferAuditLog& audit) {
    std::ofstream f("ht_audit_fallback.json", std::ios::app);
    if (!f) return Result<void>::failure(ErrorCode::IOError, "Cannot open fallback log");
    f << std::format("{{\"task_id\":\"{}\",\"source\":\"{}\",\"target\":\"{}\",\"result\":\"{}\"}}\n",
        audit.task_id, audit.source_path, audit.target_path,
        audit.result == AuditResult::Success ? "SUCCESS" : "FAILED");
    return Result<void>::success();
}

AuditLogRepository::AuditLogRepository(std::shared_ptr<Logger> logger) : logger_(std::move(logger)) {}

Result<std::vector<TransferAuditLog>> AuditLogRepository::findByTimeRange(const std::string& start, const std::string& end) {
    AuditLogQuery q; q.started_after = start; q.started_before = end;
    return logger_->queryAuditLogs(q);
}

Result<std::vector<TransferAuditLog>> AuditLogRepository::findBySourcePath(const std::string& path) {
    AuditLogQuery q; q.source_path_contains = path;
    return logger_->queryAuditLogs(q);
}

Result<std::vector<TransferAuditLog>> AuditLogRepository::findByResult(AuditResult result) {
    AuditLogQuery q; q.result = (result == AuditResult::Success) ? "SUCCESS" : "FAILED";
    return logger_->queryAuditLogs(q);
}

}
