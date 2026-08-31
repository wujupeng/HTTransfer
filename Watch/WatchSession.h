#pragma once

#include <string>
#include <memory>
#include <mutex>
#include <atomic>
#include <chrono>
#include "Watch/IWatchSession.h"
#include "Watch/WatchTypes.h"
#include "Watch/FileWatcher.h"
#include "Watch/BackupScheduler.h"
#include "Core/Domain/TransferPreset.h"

namespace ht {

class IFileEngine;
class ITaskManager;
class ILogger;

class WatchSession : public IWatchSession {
public:
    WatchSession(std::shared_ptr<ITaskManager> task_manager,
                 std::shared_ptr<IFileEngine> file_engine,
                 std::shared_ptr<ILogger> logger);
    ~WatchSession() override;

    Result<std::string> startWatch(const std::string& source_path,
                                    const std::string& target_path,
                                    int scan_interval_seconds) override;
    Result<void> stopWatch() override;
    WatchStatus getStatus() const override;
    WatchStatistics getStatistics() const override;
    void registerStatusCallback(WatchStatusCallback callback) override;

private:
    Result<void> validateConfig(const std::string& source, const std::string& target, int interval);
    std::string generateSessionId();
    void notifyStatusChange();

    std::shared_ptr<ITaskManager> task_manager_;
    std::shared_ptr<IFileEngine> file_engine_;
    std::shared_ptr<ILogger> logger_;

    std::unique_ptr<FileWatcher> file_watcher_;
    std::unique_ptr<BackupScheduler> scheduler_;

    std::mutex mutex_;
    std::string session_id_;
    std::string source_path_;
    std::string target_path_;
    int scan_interval_ = 0;
    std::atomic<WatchStatus> status_{WatchStatus::Idle};
    std::atomic<uint64_t> total_detected_{0};
    std::atomic<uint64_t> total_backed_up_{0};
    std::chrono::system_clock::time_point last_scan_time_;
    WatchStatusCallback status_callback_;
};

}