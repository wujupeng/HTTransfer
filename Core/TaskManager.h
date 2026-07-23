#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <functional>
#include <mutex>
#include <thread>
#include <atomic>
#include "Core/Common/Result.h"
#include "Core/Domain/TransferTask.h"
#include "Core/Domain/ChunkManifest.h"
#include "Core/Domain/IntegrityReport.h"
#include "Core/FileEngine.h"
#include "Core/SpeedController.h"
#include "Verify/VerifyEngine.h"
#include "Resume/ResumeEngine.h"
#include "Transfer/TransferEngine.h"
#include "Logger/ILogger.h"

namespace ht {

struct TaskProgress {
    uint64_t total_bytes = 0;
    uint64_t transferred_bytes = 0;
    double speed_mbps = 0.0;
    double average_speed_mbps = 0.0;
    std::chrono::seconds estimated_remaining{};
};

struct TaskSummary {
    std::string task_id;
    std::string source_path;
    std::string target_path;
    TaskStatus status = TaskStatus::Created;
    double progress_percent = 0.0;
};

class ITaskManager {
public:
    virtual ~ITaskManager() = default;
    virtual Result<std::string> createTask(const std::string& source_path,
                                            const std::string& target_path,
                                            TransferPreset preset,
                                            uint32_t parallelism = 4,
                                            uint64_t speed_limit = 0) = 0;
    virtual Result<void> startTask(const std::string& task_id) = 0;
    virtual Result<void> pauseTask(const std::string& task_id) = 0;
    virtual Result<void> resumeTask(const std::string& task_id) = 0;
    virtual Result<void> cancelTask(const std::string& task_id) = 0;
    virtual Result<TaskStatus> getTaskStatus(const std::string& task_id) const = 0;
    virtual Result<TaskProgress> getTaskProgress(const std::string& task_id) const = 0;
    virtual Result<std::vector<TaskSummary>> listTasks() const = 0;
    virtual Result<std::vector<TaskSummary>> listRecoverableTasks() const = 0;
    virtual void recoverFromCrash() = 0;

    using TaskCallback = std::function<void(const std::string& task_id, TaskStatus status)>;
    virtual void registerCallback(TaskCallback callback) = 0;
};

class TaskManager : public ITaskManager {
public:
    explicit TaskManager(std::shared_ptr<FileEngine> file_engine,
                         std::shared_ptr<VerifyEngine> verify_engine,
                         std::shared_ptr<ResumeEngine> resume_engine,
                         std::shared_ptr<TransferEngine> transfer_engine,
                         std::shared_ptr<SpeedController> speed_controller,
                         std::shared_ptr<ILogger> logger);

    ~TaskManager();

    Result<std::string> createTask(const std::string& source_path,
                                    const std::string& target_path,
                                    TransferPreset preset,
                                    uint32_t parallelism = 4,
                                    uint64_t speed_limit = 0) override;
    Result<void> startTask(const std::string& task_id) override;
    Result<void> pauseTask(const std::string& task_id) override;
    Result<void> resumeTask(const std::string& task_id) override;
    Result<void> cancelTask(const std::string& task_id) override;
    Result<TaskStatus> getTaskStatus(const std::string& task_id) const override;
    Result<TaskProgress> getTaskProgress(const std::string& task_id) const override;
    Result<std::vector<TaskSummary>> listTasks() const override;
    Result<std::vector<TaskSummary>> listRecoverableTasks() const override;
    void recoverFromCrash() override;
    void registerCallback(TaskCallback callback) override;

private:
    bool transitionState(TransferTask& task, TaskStatus new_status);
    bool isDuplicateTask(const std::string& source_path, const std::string& target_path) const;
    void notifyCallbacks(const std::string& task_id, TaskStatus status);
    void executeTask(const std::string& task_id);
    void executeTaskInner(const std::string& task_id);
    void onProgressUpdate(const std::string& task_id, uint64_t transferred, uint64_t total,
                          double speed_mbps, double avg_speed_mbps);

    std::shared_ptr<FileEngine> file_engine_;
    std::shared_ptr<VerifyEngine> verify_engine_;
    std::shared_ptr<ResumeEngine> resume_engine_;
    std::shared_ptr<TransferEngine> transfer_engine_;
    std::shared_ptr<SpeedController> speed_controller_;
    std::shared_ptr<ILogger> logger_;

    std::unordered_map<std::string, TransferTask> active_tasks_;
    std::unordered_map<std::string, ChunkManifest> task_manifests_;
    std::unordered_map<std::string, TaskProgress> task_progress_;
    std::unordered_map<std::string, std::thread> task_threads_;
    std::vector<TaskCallback> callbacks_;
    mutable std::mutex mutex_;
    std::atomic<bool> shutting_down_{false};
};

}
