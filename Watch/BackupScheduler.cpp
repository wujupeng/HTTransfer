#include "BackupScheduler.h"
#include "Core/TaskManager.h"
#include "Core/Common/Types.h"
#include <filesystem>
#include <thread>
#include <format>

namespace ht {

BackupScheduler::BackupScheduler(std::shared_ptr<IFileWatcher> file_watcher,
                                   std::shared_ptr<ITaskManager> task_manager,
                                   std::shared_ptr<ILogger> logger,
                                   QObject* parent)
    : QObject(parent),
      file_watcher_(std::move(file_watcher)),
      task_manager_(std::move(task_manager)),
      logger_(std::move(logger)) {
    connect(&timer_, &QTimer::timeout, this, &BackupScheduler::onScanTimeout);
}

Result<void> BackupScheduler::start(const std::string& source_path,
                                     const std::string& target_path,
                                     int scan_interval_seconds,
                                     TransferPreset preset) {
    source_path_ = source_path;
    target_path_ = target_path;
    scan_interval_seconds_ = scan_interval_seconds;
    preset_ = preset;
    backup_in_progress_.store(false);

    timer_.setInterval(scan_interval_seconds * 1000);
    timer_.start();

    if (logger_) logger_->log(ILogger::Level::Info, "WATCH",
        std::format("BackupScheduler started: interval={}s", scan_interval_seconds));
    return Result<void>::success();
}

void BackupScheduler::stop() {
    timer_.stop();
    backup_in_progress_.store(false);
    if (logger_) logger_->log(ILogger::Level::Info, "WATCH", "BackupScheduler stopped");
}

bool BackupScheduler::isRunning() const {
    return timer_.isActive();
}

void BackupScheduler::onScanTimeout() {
    if (backup_in_progress_.load()) {
        if (logger_) logger_->log(ILogger::Level::Debug, "WATCH",
            "Backup in progress, skipping scan cycle");
        return;
    }

    backup_in_progress_.store(true);

    std::thread([this]() {
        auto events = file_watcher_->scanAndDetect();

        if (on_file_changed_callback_) {
            on_file_changed_callback_(events);
        }

        if (!events.empty()) {
            executeBackup(events);
        } else {
            backup_in_progress_.store(false);
        }
    }).detach();
}

void BackupScheduler::executeBackup(const std::vector<FileChangeEvent>& events) {
    uint64_t success_count = 0;
    auto src_base = utf8ToPath(source_path_);
    auto dst_base = utf8ToPath(target_path_);

    for (const auto& event : events) {
        auto src_file = src_base / utf8ToPath(event.file_path);
        auto dst_file = dst_base / utf8ToPath(event.file_path);

        auto parent_dir = dst_file.parent_path();
        std::error_code ec;
        std::filesystem::create_directories(parent_dir, ec);

        auto create_result = task_manager_->createTask(
            pathToUtf8(src_file), pathToUtf8(dst_file), preset_, 4, 0);
        if (create_result.isErr()) {
            if (logger_) logger_->log(ILogger::Level::Error, "WATCH",
                std::format("createTask failed for {}: {}", event.file_path, create_result.errorMessage()));
            continue;
        }

        auto start_result = task_manager_->startTask(create_result.value());
        if (start_result.isErr()) {
            if (logger_) logger_->log(ILogger::Level::Error, "WATCH",
                std::format("startTask failed for {}: {}", event.file_path, start_result.errorMessage()));
            continue;
        }
        success_count++;
    }

    backup_in_progress_.store(false);

    if (on_backup_completed_callback_) {
        on_backup_completed_callback_(success_count);
    }
}

}