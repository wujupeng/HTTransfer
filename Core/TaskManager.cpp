#include "TaskManager.h"
#include "Core/Domain/TransferPreset.h"
#include "Core/Common/Types.h"
#include <algorithm>
#include <format>
#include <filesystem>

#ifdef _WIN32
#include <windows.h>
#endif

namespace ht {

static std::string pathToUtf8(const std::filesystem::path& p) {
#ifdef _WIN32
    auto wstr = p.wstring();
    if (wstr.empty()) return "";
    int len = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (len <= 0) return "";
    std::string result(len - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, result.data(), len, nullptr, nullptr);
    return result;
#else
    return p.string();
#endif
}

static const TaskStatus kValidTransitions[14][2] = {
    {TaskStatus::Created, TaskStatus::Queued},
    {TaskStatus::Queued, TaskStatus::Stabilizing},
    {TaskStatus::Stabilizing, TaskStatus::Transferring},
    {TaskStatus::Stabilizing, TaskStatus::Failed},
    {TaskStatus::Transferring, TaskStatus::Paused},
    {TaskStatus::Transferring, TaskStatus::Verifying},
    {TaskStatus::Transferring, TaskStatus::Completed},
    {TaskStatus::Transferring, TaskStatus::Failed},
    {TaskStatus::Transferring, TaskStatus::Cancelled},
    {TaskStatus::Paused, TaskStatus::Transferring},
    {TaskStatus::Paused, TaskStatus::Cancelled},
    {TaskStatus::Verifying, TaskStatus::Completed},
    {TaskStatus::Verifying, TaskStatus::Failed},
    {TaskStatus::Failed, TaskStatus::Queued},
};

TaskManager::TaskManager(std::shared_ptr<FileEngine> file_engine,
                         std::shared_ptr<VerifyEngine> verify_engine,
                         std::shared_ptr<ResumeEngine> resume_engine,
                         std::shared_ptr<TransferEngine> transfer_engine,
                         std::shared_ptr<SpeedController> speed_controller,
                         std::shared_ptr<ILogger> logger)
    : file_engine_(std::move(file_engine)),
      verify_engine_(std::move(verify_engine)),
      resume_engine_(std::move(resume_engine)),
      transfer_engine_(std::move(transfer_engine)),
      speed_controller_(std::move(speed_controller)),
      logger_(std::move(logger)) {
    transfer_engine_->setProgressCallback(
        [this](const std::string& task_id, uint64_t transferred, uint64_t total,
               double speed_mbps, double avg_speed_mbps) {
            onProgressUpdate(task_id, transferred, total, speed_mbps, avg_speed_mbps);
        });
    transfer_engine_->setChunkCompletedCallback(
        [this](const std::string& task_id, uint64_t chunk_index, uint64_t offset) {
            resume_engine_->markChunkCompleted(task_id, chunk_index, offset);
        });
}

TaskManager::~TaskManager() {
    shutting_down_ = true;
    for (auto& [id, thread] : task_threads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
}

Result<std::string> TaskManager::createTask(const std::string& source_path,
                                             const std::string& target_path,
                                             TransferPreset preset,
                                             uint32_t parallelism,
                                             uint64_t speed_limit) {
    if (logger_) {
        logger_->log(ILogger::Level::Info, "SYSTEM",
            std::format("createTask ENTRY: preset={}, parallelism={}, speed_limit={}",
                static_cast<uint32_t>(preset), parallelism, speed_limit));
    }
    std::lock_guard lock(mutex_);

    if (source_path.empty() || target_path.empty()) {
        return Result<std::string>::failure(ErrorCode::ConfigError, "Source and target paths must not be empty");
    }

    if (isDuplicateTask(source_path, target_path)) {
        return Result<std::string>::failure(ErrorCode::ConfigError,
            "Duplicate task: same source and target already active");
    }

    TransferTask task;
    task.task_id = generateTaskId();
    task.source_path = source_path;
    task.target_path = target_path;
    task.preset = preset;
    task.protocol = detectProtocol(target_path);
    task.status = TaskStatus::Created;
    task.created_at = std::chrono::system_clock::now();
    task.updated_at = task.created_at;

    auto preset_config = getPresetDefault(preset);
    task.parallelism = (parallelism > 0) ? parallelism : preset_config.parallelism;
    task.speed_limit = (speed_limit > 0) ? speed_limit : preset_config.speed_limit;

    active_tasks_[task.task_id] = task;

    if (logger_) {
        logger_->log(ILogger::Level::Info, task.task_id,
            std::format("Task created: parallelism={}, speed_limit={}", task.parallelism, task.speed_limit));
    }

    return Result<std::string>::success(task.task_id);
}

Result<void> TaskManager::startTask(const std::string& task_id) {
    std::lock_guard lock(mutex_);

    auto it = active_tasks_.find(task_id);
    if (it == active_tasks_.end()) {
        return Result<void>::failure(ErrorCode::ConfigError, "Task not found");
    }

    if (it->second.status != TaskStatus::Created) {
        return Result<void>::failure(ErrorCode::ConfigError, "Task must be in Created state to start");
    }

    if (!transitionState(it->second, TaskStatus::Queued)) {
        return Result<void>::failure(ErrorCode::ConfigError, "Invalid state transition");
    }

    auto thread_it = task_threads_.find(task_id);
    if (thread_it != task_threads_.end() && thread_it->second.joinable()) {
        thread_it->second.join();
    }

    task_threads_[task_id] = std::thread(&TaskManager::executeTask, this, task_id);

    return Result<void>::success();
}

void TaskManager::executeTask(const std::string& task_id) {
    try {
        executeTaskInner(task_id);
    } catch (const std::exception& e) {
        std::lock_guard lock(mutex_);
        auto it = active_tasks_.find(task_id);
        if (it != active_tasks_.end()) {
            transitionState(it->second, TaskStatus::Failed);
            it->second.error_code = "HT-E999";
            it->second.error_message = std::string("Unhandled exception: ") + e.what();
        }
        if (logger_) {
            logger_->log(ILogger::Level::Critical, task_id,
                std::string("Unhandled exception: ") + e.what());
        }
    } catch (...) {
        std::lock_guard lock(mutex_);
        auto it = active_tasks_.find(task_id);
        if (it != active_tasks_.end()) {
            transitionState(it->second, TaskStatus::Failed);
            it->second.error_code = "HT-E999";
            it->second.error_message = "Unknown exception";
        }
    }
}

void TaskManager::executeTaskInner(const std::string& task_id) {
    if (logger_) logger_->log(ILogger::Level::Info, task_id, "executeTaskInner: starting");

    TransferTask task;
    {
        std::lock_guard lock(mutex_);
        auto it = active_tasks_.find(task_id);
        if (it == active_tasks_.end()) return;
        task = it->second;
    }

    if (logger_) logger_->log(ILogger::Level::Info, task_id,
        std::format("executeTaskInner: parallelism={}, speed_limit={}, src={}, dst={}",
            task.parallelism, task.speed_limit, task.source_path, task.target_path));

    transfer_engine_->setParallelism(task.parallelism);

    if (logger_) logger_->log(ILogger::Level::Info, task_id, "executeTaskInner: setParallelism done");
    {
        std::lock_guard lock(mutex_);
        auto it = active_tasks_.find(task_id);
        if (it != active_tasks_.end()) {
            transitionState(it->second, TaskStatus::Stabilizing);
        }
    }

    if (logger_) logger_->log(ILogger::Level::Info, task_id, "Step 1: Checking stability");

    auto stability = file_engine_->checkStability(utf8ToPath(task.source_path), std::chrono::seconds(1));
    if (stability.isErr()) {
        std::lock_guard lock(mutex_);
        auto it = active_tasks_.find(task_id);
        if (it != active_tasks_.end()) {
            transitionState(it->second, TaskStatus::Failed);
            it->second.error_code = stability.errorCodeString();
            it->second.error_message = stability.errorMessage();
        }
        return;
    }

    if (stability.value() == FileStability::Timeout) {
        if (logger_) {
            logger_->log(ILogger::Level::Warning, task_id,
                "File still changing, proceeding anyway");
        }
    }

    if (logger_) logger_->log(ILogger::Level::Info, task_id, "Step 2: Computing source hash");

    std::string start_hash;
    std::error_code check_ec;
    bool source_is_dir = std::filesystem::is_directory(utf8ToPath(task.source_path), check_ec) && !check_ec;

    auto preset_config = getPresetDefault(task.preset);
    if (preset_config.enable_sha256 && !source_is_dir) {
        auto hash_result = verify_engine_->computeFileHash(utf8ToPath(task.source_path));
        if (hash_result.isErr()) {
            if (logger_) logger_->log(ILogger::Level::Warning, task_id,
                std::string("Source hash failed: ") + hash_result.errorMessage() + ", skipping");
        } else {
            start_hash = hash_result.value();
        }
    }

    {
        std::lock_guard lock(mutex_);
        auto it = active_tasks_.find(task_id);
        if (it != active_tasks_.end()) {
            transitionState(it->second, TaskStatus::Transferring);
        }
    }

    if (logger_) logger_->log(ILogger::Level::Info, task_id, "Step 3: Starting transfer");

    std::filesystem::path src_path = utf8ToPath(task.source_path);
    std::filesystem::path dst_path = utf8ToPath(task.target_path);
    uint64_t total_bytes = 0;

    std::error_code fs_ec;
    bool is_dir = std::filesystem::is_directory(src_path, fs_ec);
    if (fs_ec) {
        if (logger_) logger_->log(ILogger::Level::Warning, task_id,
            std::string("is_directory check failed: ") + fs_ec.message());
    }

    if (is_dir && !fs_ec) {
        if (logger_) logger_->log(ILogger::Level::Info, task_id, "Step 3: Directory mode - scanning source");
        auto scan_result = file_engine_->scanDirectory(src_path);
        if (scan_result.isErr()) {
            std::lock_guard lock(mutex_);
            auto it = active_tasks_.find(task_id);
            if (it != active_tasks_.end()) {
                transitionState(it->second, TaskStatus::Failed);
                it->second.error_code = scan_result.errorCodeString();
                it->second.error_message = scan_result.errorMessage();
            }
            return;
        }

        auto& entries = scan_result.value();
        if (logger_) logger_->log(ILogger::Level::Info, task_id,
            std::format("Step 3: Found {} files in directory", entries.size()));
        for (const auto& entry : entries) {
            total_bytes += entry.file_size;
        }

        {
            std::lock_guard lock(mutex_);
            auto it = active_tasks_.find(task_id);
            if (it != active_tasks_.end()) {
                it->second.total_bytes = total_bytes;
                task_progress_[task_id].total_bytes = total_bytes;
            }
        }

        auto dir_target = dst_path / src_path.filename();
        std::error_code dir_ec;
        std::filesystem::create_directories(dir_target, dir_ec);

        for (const auto& entry : entries) {
            if (shutting_down_) return;

            auto resolved_target = dir_target / entry.relative_path;
            auto parent_dir = resolved_target.parent_path();
            std::error_code ec;
            std::filesystem::create_directories(parent_dir, ec);

            if (logger_) logger_->log(ILogger::Level::Info, task_id,
                std::format("Step 3: Transferring file {} ({} bytes)",
                    pathToUtf8(entry.relative_path), entry.file_size));

            auto manifest_result = file_engine_->createChunkManifest(task_id, entry.source_path);
            if (manifest_result.isErr()) {
                if (logger_) {
                    logger_->log(ILogger::Level::Error, task_id, "Failed to create manifest");
                }
                continue;
            }

            auto& manifest = task_manifests_[task_id + "/" + pathToUtf8(entry.relative_path)] = manifest_result.value();
            manifest.file_hash = start_hash;

            file_engine_->preallocateFile(resolved_target, entry.file_size);

            TransferTask file_task;
            file_task.task_id = task_id;
            file_task.source_path = pathToUtf8(entry.source_path);
            file_task.target_path = pathToUtf8(resolved_target);
            file_task.preset = task.preset;
            file_task.parallelism = task.parallelism;
            file_task.speed_limit = task.speed_limit;
            file_task.total_bytes = entry.file_size;

            auto transfer_result = transfer_engine_->startTransfer(file_task, manifest);
            if (transfer_result.isErr()) {
                if (logger_) {
                    logger_->log(ILogger::Level::Error, task_id,
                        std::string("Transfer failed: ") + transfer_result.errorMessage());
                }
            } else {
                if (logger_) logger_->log(ILogger::Level::Info, task_id,
                    std::string("File transferred: ") + pathToUtf8(entry.relative_path));
            }
        }
    } else {
        if (logger_) logger_->log(ILogger::Level::Info, task_id, "Step 3a: Creating chunk manifest");
        auto manifest_result = file_engine_->createChunkManifest(task_id, utf8ToPath(task.source_path));
        if (manifest_result.isErr()) {
            std::lock_guard lock(mutex_);
            auto it = active_tasks_.find(task_id);
            if (it != active_tasks_.end()) {
                transitionState(it->second, TaskStatus::Failed);
                it->second.error_code = manifest_result.errorCodeString();
                it->second.error_message = manifest_result.errorMessage();
            }
            return;
        }

        auto& manifest = task_manifests_[task_id] = manifest_result.value();
        manifest.file_hash = start_hash;

        std::error_code ec;
        total_bytes = std::filesystem::file_size(utf8ToPath(task.source_path), ec);
        if (ec) {
            std::lock_guard lock(mutex_);
            auto it = active_tasks_.find(task_id);
            if (it != active_tasks_.end()) {
                transitionState(it->second, TaskStatus::Failed);
                it->second.error_code = "HT-E002";
                it->second.error_message = "Cannot get source file size";
            }
            return;
        }

        auto resolved_target = file_engine_->resolveTargetPath(src_path, dst_path);

        if (logger_) logger_->log(ILogger::Level::Info, task_id,
            std::string("Step 3b: Resolved target path"));

        {
            std::lock_guard lock(mutex_);
            auto it = active_tasks_.find(task_id);
            if (it != active_tasks_.end()) {
                it->second.total_bytes = total_bytes;
                it->second.target_path = pathToUtf8(resolved_target);
                task_progress_[task_id].total_bytes = total_bytes;
            }
        }

        ResumeFileData resume_data;
        resume_data.task_id = task_id;
        resume_data.file_size = total_bytes;
        resume_data.source_hash = start_hash;
        resume_data.source_create_time = task.created_at;
        resume_data.source_modify_time = std::chrono::system_clock::now();
        resume_data.updated_at = std::chrono::system_clock::now();
        resume_engine_->createResumeFile(task_id, resume_data);

        auto load_result = resume_engine_->loadResumeFile(task_id);
        if (load_result.isOk() && load_result.value().has_value()) {
            auto& resume = load_result.value().value();
            for (uint64_t idx : resume.completed_chunks) {
                if (idx < manifest.chunks.size()) {
                    manifest.chunks[idx].status = ChunkStatus::Verified;
                }
            }
        }

        file_engine_->preallocateFile(resolved_target, total_bytes);

        if (logger_) logger_->log(ILogger::Level::Info, task_id,
            std::string("Step 3c: Preallocated, starting transfer engine"));

        TransferTask file_task;
        file_task.task_id = task_id;
        file_task.source_path = task.source_path;
        file_task.target_path = pathToUtf8(resolved_target);
        file_task.preset = task.preset;
        file_task.parallelism = task.parallelism;
        file_task.speed_limit = task.speed_limit;
        file_task.total_bytes = total_bytes;

        auto transfer_result = transfer_engine_->startTransfer(file_task, manifest);
        if (transfer_result.isErr()) {
            std::lock_guard lock(mutex_);
            auto it = active_tasks_.find(task_id);
            if (it != active_tasks_.end()) {
                transitionState(it->second, TaskStatus::Failed);
                it->second.error_code = transfer_result.errorCodeString();
                it->second.error_message = transfer_result.errorMessage();
            }
            return;
        }
    }

    if (logger_) logger_->log(ILogger::Level::Info, task_id, "Step 4: Verifying target");

    std::string actual_target_path;
    bool enable_verify = true;
    {
        std::lock_guard lock(mutex_);
        auto it = active_tasks_.find(task_id);
        if (it != active_tasks_.end()) {
            actual_target_path = it->second.target_path;
            auto preset_config = getPresetDefault(it->second.preset);
            enable_verify = preset_config.enable_sha256;
        }
    }

    if (actual_target_path.empty()) {
        actual_target_path = task.target_path;
    }

    if (!enable_verify || source_is_dir) {
        if (logger_ && source_is_dir) logger_->log(ILogger::Level::Info, task_id,
            "Step 4: Skipping verification for directory transfer");
        std::lock_guard lock(mutex_);
        auto it = active_tasks_.find(task_id);
        if (it != active_tasks_.end()) {
            transitionState(it->second, TaskStatus::Completed);
            resume_engine_->invalidateResumeFile(task_id);

            TransferAuditLog audit;
            audit.task_id = task_id;
            audit.source_path = task.source_path;
            audit.target_path = actual_target_path;
            audit.result = AuditResult::Success;
            if (logger_) logger_->writeAuditLog(audit);
        }
        return;
    }

    {
        std::lock_guard lock(mutex_);
        auto it = active_tasks_.find(task_id);
        if (it != active_tasks_.end()) {
            transitionState(it->second, TaskStatus::Verifying);
        }
    }

    auto end_hash_result = verify_engine_->computeFileHash(utf8ToPath(actual_target_path));
    if (end_hash_result.isErr()) {
        std::lock_guard lock(mutex_);
        auto it = active_tasks_.find(task_id);
        if (it != active_tasks_.end()) {
            transitionState(it->second, TaskStatus::Failed);
            it->second.error_code = end_hash_result.errorCodeString();
            it->second.error_message = end_hash_result.errorMessage();
        }
        return;
    }

    auto report_result = verify_engine_->generateReport(
        task_id, start_hash, end_hash_result.value(),
        task_manifests_.count(task_id) ? task_manifests_[task_id].total_chunks : 0, 0);

    std::lock_guard lock(mutex_);
    auto it = active_tasks_.find(task_id);
    if (it == active_tasks_.end()) return;

    if (report_result.isOk() && report_result.value().hash_match) {
        transitionState(it->second, TaskStatus::Completed);
        resume_engine_->invalidateResumeFile(task_id);

        TransferAuditLog audit;
        audit.task_id = task_id;
        audit.source_path = task.source_path;
        audit.target_path = actual_target_path;
        audit.result = AuditResult::Success;
        audit.start_hash = start_hash;
        audit.end_hash = end_hash_result.value();
        if (logger_) logger_->writeAuditLog(audit);
    } else {
        transitionState(it->second, TaskStatus::Failed);
        it->second.error_code = "HT-E006";
        it->second.error_message = "SHA-256 verification failed";

        TransferAuditLog audit;
        audit.task_id = task_id;
        audit.source_path = task.source_path;
        audit.target_path = actual_target_path;
        audit.result = AuditResult::Failed;
        audit.failure_reason = "SHA-256 mismatch";
        if (logger_) logger_->writeAuditLog(audit);
    }
}

Result<void> TaskManager::pauseTask(const std::string& task_id) {
    std::lock_guard lock(mutex_);
    auto it = active_tasks_.find(task_id);
    if (it == active_tasks_.end()) {
        return Result<void>::failure(ErrorCode::ConfigError, "Task not found");
    }

    if (it->second.status != TaskStatus::Transferring) {
        return Result<void>::failure(ErrorCode::ConfigError, "Task must be in Transferring state to pause");
    }

    auto result = transfer_engine_->pauseTransfer(task_id);
    if (result.isOk()) {
        transitionState(it->second, TaskStatus::Paused);
        if (logger_) {
            logger_->log(ILogger::Level::Info, task_id, "Task paused");
        }
    }
    return result;
}

Result<void> TaskManager::resumeTask(const std::string& task_id) {
    std::lock_guard lock(mutex_);
    auto it = active_tasks_.find(task_id);
    if (it == active_tasks_.end()) {
        return Result<void>::failure(ErrorCode::ConfigError, "Task not found");
    }

    if (it->second.status != TaskStatus::Paused) {
        return Result<void>::failure(ErrorCode::ConfigError, "Task must be in Paused state to resume");
    }

    auto result = transfer_engine_->resumeTransfer(task_id);
    if (result.isOk()) {
        transitionState(it->second, TaskStatus::Transferring);
        if (logger_) {
            logger_->log(ILogger::Level::Info, task_id, "Task resumed");
        }
    }
    return result;
}

Result<void> TaskManager::cancelTask(const std::string& task_id) {
    std::lock_guard lock(mutex_);
    auto it = active_tasks_.find(task_id);
    if (it == active_tasks_.end()) {
        return Result<void>::failure(ErrorCode::ConfigError, "Task not found");
    }

    auto result = transfer_engine_->cancelTransfer(task_id);
    transitionState(it->second, TaskStatus::Cancelled);

    TransferAuditLog audit;
    audit.task_id = task_id;
    audit.source_path = it->second.source_path;
    audit.target_path = it->second.target_path;
    audit.result = AuditResult::Failed;
    audit.failure_reason = "Cancelled by user";
    if (logger_) logger_->writeAuditLog(audit);

    return result;
}

Result<TaskStatus> TaskManager::getTaskStatus(const std::string& task_id) const {
    std::lock_guard lock(mutex_);
    auto it = active_tasks_.find(task_id);
    if (it == active_tasks_.end()) {
        return Result<TaskStatus>::failure(ErrorCode::ConfigError, "Task not found");
    }
    return Result<TaskStatus>::success(it->second.status);
}

Result<TaskProgress> TaskManager::getTaskProgress(const std::string& task_id) const {
    std::lock_guard lock(mutex_);
    auto it = task_progress_.find(task_id);
    if (it == task_progress_.end()) {
        return Result<TaskProgress>::failure(ErrorCode::ConfigError, "Task progress not found");
    }
    return Result<TaskProgress>::success(it->second);
}

Result<std::vector<TaskSummary>> TaskManager::listTasks() const {
    std::lock_guard lock(mutex_);
    std::vector<TaskSummary> summaries;
    summaries.reserve(active_tasks_.size());
    for (const auto& [id, task] : active_tasks_) {
        TaskSummary s;
        s.task_id = task.task_id;
        s.source_path = task.source_path;
        s.target_path = task.target_path;
        s.status = task.status;
        s.progress_percent = task.total_bytes > 0
            ? (static_cast<double>(task.transferred_bytes) / task.total_bytes) * 100.0
            : 0.0;
        summaries.push_back(std::move(s));
    }
    return Result<std::vector<TaskSummary>>::success(std::move(summaries));
}

void TaskManager::recoverFromCrash() {
    auto result = resume_engine_->scanUnfinishedTasks();
    if (result.isErr()) return;

    for (const auto& task_id : result.value()) {
        auto load_result = resume_engine_->loadResumeFile(task_id);
        if (load_result.isErr() || !load_result.value().has_value()) {
            resume_engine_->invalidateResumeFile(task_id);
            continue;
        }

        if (logger_) {
            logger_->log(ILogger::Level::Info, task_id, "Recovering unfinished task from crash");
        }

        auto& resume_data = load_result.value().value();
        TransferTask task;
        task.task_id = task_id;
        task.status = TaskStatus::Created;
        task.preset = TransferPreset::Balanced;
        task.parallelism = kDefaultParallelism;
        task.total_bytes = resume_data.file_size;
        task.created_at = resume_data.source_create_time;
        task.updated_at = std::chrono::system_clock::now();

        active_tasks_[task_id] = task;
        task_progress_[task_id].total_bytes = resume_data.file_size;
        task_progress_[task_id].transferred_bytes = resume_data.current_offset;
    }
}

void TaskManager::registerCallback(TaskCallback callback) {
    callbacks_.push_back(std::move(callback));
}

bool TaskManager::transitionState(TransferTask& task, TaskStatus new_status) {
    for (const auto& [from, to] : kValidTransitions) {
        if (task.status == from && new_status == to) {
            task.status = new_status;
            task.updated_at = std::chrono::system_clock::now();
            notifyCallbacks(task.task_id, new_status);
            return true;
        }
    }
    return false;
}

bool TaskManager::isDuplicateTask(const std::string& source_path, const std::string& target_path) const {
    for (const auto& [id, task] : active_tasks_) {
        if (task.source_path == source_path && task.target_path == target_path &&
            task.status != TaskStatus::Completed && task.status != TaskStatus::Failed &&
            task.status != TaskStatus::Cancelled) {
            return true;
        }
    }
    return false;
}

void TaskManager::notifyCallbacks(const std::string& task_id, TaskStatus status) {
    for (auto& cb : callbacks_) {
        cb(task_id, status);
    }
}

void TaskManager::onProgressUpdate(const std::string& task_id, uint64_t transferred, uint64_t total,
                                    double speed_mbps, double avg_speed_mbps) {
    std::lock_guard lock(mutex_);
    auto& progress = task_progress_[task_id];
    progress.total_bytes = total;
    progress.transferred_bytes = transferred;
    progress.speed_mbps = speed_mbps;
    progress.average_speed_mbps = avg_speed_mbps;
    if (speed_mbps > 0) {
        uint64_t remaining = (total > transferred) ? (total - transferred) : 0;
        progress.estimated_remaining = std::chrono::seconds(
            static_cast<int64_t>(remaining / (speed_mbps * 1024.0 * 1024.0 / 8.0)));
    }

    auto it = active_tasks_.find(task_id);
    if (it != active_tasks_.end()) {
        it->second.transferred_bytes = transferred;
    }
}

}
