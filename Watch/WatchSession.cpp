#include "WatchSession.h"
#include "FileWatcher.h"
#include "BackupScheduler.h"
#include "Core/Common/Types.h"
#include "Core/Common/ErrorCodes.h"
#include <filesystem>
#include <format>
#include <chrono>
#include <iomanip>
#include <sstream>

namespace ht {

WatchSession::WatchSession(std::shared_ptr<ITaskManager> task_manager,
                             std::shared_ptr<IFileEngine> file_engine,
                             std::shared_ptr<ILogger> logger)
    : task_manager_(std::move(task_manager)),
      file_engine_(std::move(file_engine)),
      logger_(std::move(logger)) {}

WatchSession::~WatchSession() {
    stopWatch();
}

Result<void> WatchSession::validateConfig(const std::string& source,
                                            const std::string& target,
                                            int interval) {
    if (source.empty()) {
        return Result<void>::failure(ErrorCode::ConfigError, "Source path must not be empty");
    }
    if (target.empty()) {
        return Result<void>::failure(ErrorCode::ConfigError, "Target path must not be empty");
    }
    if (source == target) {
        return Result<void>::failure(ErrorCode::ConfigError, "Source and target paths must not be the same");
    }
    if (interval < 1 || interval > 3600) {
        return Result<void>::failure(ErrorCode::ConfigError, "Scan interval must be between 1 and 3600 seconds");
    }
    auto src_path = utf8ToPath(source);
    std::error_code ec;
    if (!std::filesystem::exists(src_path, ec) || ec) {
        return Result<void>::failure(ErrorCode::SourceError, "Source path does not exist or is not accessible");
    }
    return Result<void>::success();
}

std::string WatchSession::generateSessionId() {
    auto now = std::chrono::system_clock::now();
    auto time_t_val = std::chrono::system_clock::to_time_t(now);
    std::tm tm_buf{};
#ifdef _WIN32
    localtime_s(&tm_buf, &time_t_val);
#else
    localtime_r(&time_t_val, &tm_buf);
#endif
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count() % 1000;

    std::ostringstream oss;
    oss << "WS-"
        << std::put_time(&tm_buf, "%Y%m%d")
        << "-" << std::setfill('0') << std::setw(6) << ms;
    return oss.str();
}

Result<std::string> WatchSession::startWatch(const std::string& source_path,
                                                const std::string& target_path,
                                                int scan_interval_seconds) {
    std::lock_guard lock(mutex_);

    if (status_.load() == WatchStatus::Running) {
        return Result<std::string>::failure(ErrorCode::ConfigError, "Watch session is already running");
    }

    auto validate_result = validateConfig(source_path, target_path, scan_interval_seconds);
    if (validate_result.isErr()) {
        return Result<std::string>::failure(validate_result.errorCode(), validate_result.errorMessage());
    }

    session_id_ = generateSessionId();
    source_path_ = source_path;
    target_path_ = target_path;
    scan_interval_ = scan_interval_seconds;
    total_detected_.store(0);
    total_backed_up_.store(0);
    last_scan_time_ = std::chrono::system_clock::now();

    file_watcher_ = std::make_unique<FileWatcher>(source_path_, file_engine_, logger_);

    scheduler_ = std::make_unique<BackupScheduler>(
        std::shared_ptr<IFileWatcher>(file_watcher_.get(), [](auto*){}),
        task_manager_, logger_);

    scheduler_->setOnFileChangedCallback([this](const std::vector<FileChangeEvent>& events) {
        total_detected_.fetch_add(events.size());
        last_scan_time_ = std::chrono::system_clock::now();
    });

    scheduler_->setOnBackupCompletedCallback([this](uint64_t success_count) {
        total_backed_up_.fetch_add(success_count);
    });

    auto start_result = scheduler_->start(source_path_, target_path_, scan_interval_, TransferPreset::Balanced);
    if (start_result.isErr()) {
        status_.store(WatchStatus::Error);
        return Result<std::string>::failure(start_result.errorCode(), start_result.errorMessage());
    }

    status_.store(WatchStatus::Running);
    notifyStatusChange();

    if (logger_) logger_->log(ILogger::Level::Info, session_id_,
        std::format("WatchSession started: src={}, dst={}, interval={}s",
            source_path_, target_path_, scan_interval_));

    return Result<std::string>::success(session_id_);
}

Result<void> WatchSession::stopWatch() {
    std::lock_guard lock(mutex_);

    if (status_.load() != WatchStatus::Running) {
        return Result<void>::success();
    }

    if (scheduler_) {
        scheduler_->stop();
    }

    status_.store(WatchStatus::Stopped);
    notifyStatusChange();

    if (logger_) logger_->log(ILogger::Level::Info, session_id_, "WatchSession stopped");

    return Result<void>::success();
}

WatchStatus WatchSession::getStatus() const {
    return status_.load();
}

WatchStatistics WatchSession::getStatistics() const {
    WatchStatistics stats;
    stats.status = status_.load();
    stats.scan_interval = scan_interval_;
    stats.total_detected = total_detected_.load();
    stats.total_backed_up = total_backed_up_.load();
    stats.last_scan_time = last_scan_time_;
    return stats;
}

void WatchSession::registerStatusCallback(WatchStatusCallback callback) {
    std::lock_guard lock(mutex_);
    status_callback_ = std::move(callback);
}

void WatchSession::notifyStatusChange() {
    if (status_callback_) {
        status_callback_(status_.load(), getStatistics());
    }
}

}