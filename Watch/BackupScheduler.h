#pragma once

#include <string>
#include <atomic>
#include <memory>
#include <functional>
#include <chrono>
#include <QObject>
#include <QTimer>
#include "Watch/IFileWatcher.h"
#include "Watch/WatchTypes.h"
#include "Core/Domain/TransferPreset.h"
#include "Core/Common/Result.h"
#include "Logger/ILogger.h"

namespace ht {

class ITaskManager;

class BackupScheduler : public QObject {
    Q_OBJECT

public:
    BackupScheduler(std::shared_ptr<IFileWatcher> file_watcher,
                    std::shared_ptr<ITaskManager> task_manager,
                    std::shared_ptr<ILogger> logger,
                    QObject* parent = nullptr);

    Result<void> start(const std::string& source_path,
                       const std::string& target_path,
                       int scan_interval_seconds,
                       TransferPreset preset);
    void stop();
    bool isRunning() const;

    void setOnFileChangedCallback(std::function<void(const std::vector<FileChangeEvent>&)> cb) {
        on_file_changed_callback_ = std::move(cb);
    }
    void setOnBackupCompletedCallback(std::function<void(uint64_t)> cb) {
        on_backup_completed_callback_ = std::move(cb);
    }

private slots:
    void onScanTimeout();

private:
    void executeBackup(const std::vector<FileChangeEvent>& events);

    QTimer timer_;
    int scan_interval_seconds_ = 0;
    std::shared_ptr<IFileWatcher> file_watcher_;
    std::shared_ptr<ITaskManager> task_manager_;
    std::shared_ptr<ILogger> logger_;
    std::string source_path_;
    std::string target_path_;
    TransferPreset preset_ = TransferPreset::Balanced;
    std::atomic<bool> backup_in_progress_{false};
    std::function<void(const std::vector<FileChangeEvent>&)> on_file_changed_callback_;
    std::function<void(uint64_t)> on_backup_completed_callback_;
};

}