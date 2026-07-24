#pragma once

#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <unordered_map>
#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include "Core/Common/Result.h"
#include "Core/Common/Constants.h"
#include "Core/Domain/TransferTask.h"
#include "Core/SpeedController.h"
#include "Core/Domain/ChunkManifest.h"
#include "Core/BufferPool.h"
#include "Core/IDataSource.h"
#include "Core/IDataSink.h"
#include "Resume/ResumeEngine.h"
#include "Logger/ILogger.h"

namespace ht {

struct FileMetadata {
    uint64_t file_size = 0;
    std::chrono::system_clock::time_point modify_time;
    std::chrono::system_clock::time_point create_time;
};

struct AuthInfo {
    std::string username;
    std::string password;
    std::string key_path;
};

class ITransferAdapter {
public:
    virtual ~ITransferAdapter() = default;
    virtual Result<void> connect(const std::string& endpoint, const AuthInfo& auth) = 0;
    virtual Result<void> disconnect() = 0;
    virtual Result<void> sendChunk(const ChunkInfo& chunk, const BufferSegment& data) = 0;
    virtual Result<BufferSegment> receiveChunk(const ChunkInfo& chunk) = 0;
    virtual Result<void> seek(uint64_t offset) = 0;
    virtual Result<FileMetadata> getFileMetadata(const std::string& path) = 0;
    virtual ProtocolType getProtocolType() const = 0;
    virtual bool isConnected() const = 0;
};

class ITransferEngine {
public:
    virtual ~ITransferEngine() = default;
    virtual void registerAdapter(ProtocolType protocol, std::unique_ptr<ITransferAdapter> adapter) = 0;
    virtual Result<void> startTransfer(const TransferTask& task, const ChunkManifest& manifest) = 0;
    virtual Result<void> pauseTransfer(const std::string& task_id) = 0;
    virtual Result<void> resumeTransfer(const std::string& task_id) = 0;
    virtual Result<void> cancelTransfer(const std::string& task_id) = 0;
    virtual void setParallelism(uint32_t count) = 0;
    virtual uint32_t getParallelism() const = 0;

    using ProgressCallback = std::function<void(const std::string& task_id,
                                                 uint64_t transferred_bytes,
                                                 uint64_t total_bytes,
                                                 double speed_mbps,
                                                 double avg_speed_mbps)>;
    using ChunkCompletedCallback = std::function<void(const std::string& task_id,
                                                       uint64_t chunk_index,
                                                       uint64_t offset)>;
    virtual void setProgressCallback(ProgressCallback callback) = 0;
    virtual void setChunkCompletedCallback(ChunkCompletedCallback callback) = 0;
};

class RetryController {
public:
    RetryController(std::chrono::seconds base_delay = kBaseRetryDelay,
                    std::chrono::seconds max_delay = kMaxRetryDelay);

    std::chrono::seconds nextDelay();
    void reset();
    uint32_t retryCount() const { return retry_count_; }

private:
    std::chrono::seconds base_delay_;
    std::chrono::seconds max_delay_;
    uint32_t retry_count_ = 0;
};

class WorkerPool {
public:
    explicit WorkerPool(uint32_t worker_count = kDefaultParallelism);
    ~WorkerPool();

    using WorkItem = std::function<void()>;
    void submit(WorkItem item);
    void waitAll();
    void shutdown();

    uint32_t workerCount() const { return worker_count_; }

private:
    void workerLoop();

    std::vector<std::thread> workers_;
    std::queue<WorkItem> queue_;
    std::mutex mutex_;
    std::condition_variable cv_;
    std::condition_variable done_cv_;
    std::atomic<bool> running_{false};
    std::atomic<uint32_t> active_count_{0};
    uint32_t worker_count_;
};

struct TaskControl {
    std::atomic<bool> paused{false};
    std::atomic<bool> cancelled{false};
};

class TransferEngine : public ITransferEngine {
public:
    explicit TransferEngine(std::shared_ptr<ILogger> logger,
                            std::shared_ptr<BufferPool> buffer_pool = nullptr,
                            std::shared_ptr<IResumeEngine> resume_engine = nullptr);

    ~TransferEngine() override;

    void registerAdapter(ProtocolType protocol, std::unique_ptr<ITransferAdapter> adapter) override;
    Result<void> startTransfer(const TransferTask& task, const ChunkManifest& manifest) override;
    Result<void> pauseTransfer(const std::string& task_id) override;
    Result<void> resumeTransfer(const std::string& task_id) override;
    Result<void> cancelTransfer(const std::string& task_id) override;
    void setParallelism(uint32_t count) override;
    uint32_t getParallelism() const override;
    void setProgressCallback(ProgressCallback callback) override;
    void setChunkCompletedCallback(ChunkCompletedCallback callback) override;
    void setSpeedController(std::shared_ptr<ISpeedController> controller);

    static bool isSMB(const std::string& path);

private:
    Result<void> startTransferSingleThread(const TransferTask& task, const ChunkManifest& manifest,
                                            IDataSource* source, IDataSink* sink,
                                            std::shared_ptr<TaskControl> ctrl);
    Result<void> startTransferReaderWriter(const TransferTask& task, const ChunkManifest& manifest,
                                            IDataSource* source, IDataSink* sink,
                                            std::shared_ptr<TaskControl> ctrl);
    std::shared_ptr<TaskControl> getTaskControl(const std::string& task_id);

    void validateMagic() const;

    std::shared_ptr<ILogger> logger_;
    std::shared_ptr<BufferPool> buffer_pool_;
    std::shared_ptr<IResumeEngine> resume_engine_;
    std::unordered_map<ProtocolType, std::unique_ptr<ITransferAdapter>> adapters_;
    std::unique_ptr<WorkerPool> worker_pool_;
    ProgressCallback progress_callback_;
    ChunkCompletedCallback chunk_completed_callback_;
    std::shared_ptr<ISpeedController> speed_controller_;
    uint32_t parallelism_ = kDefaultParallelism;

    std::mutex task_control_mutex_;
    std::unordered_map<std::string, std::shared_ptr<TaskControl>> task_controls_;

#ifndef NDEBUG
    uint32_t magic_ = 0;
#endif
};

class SMBAdapter : public ITransferAdapter {
public:
    Result<void> connect(const std::string& endpoint, const AuthInfo& auth) override;
    Result<void> disconnect() override;
    Result<void> sendChunk(const ChunkInfo& chunk, const BufferSegment& data) override;
    Result<BufferSegment> receiveChunk(const ChunkInfo& chunk) override;
    Result<void> seek(uint64_t offset) override;
    Result<FileMetadata> getFileMetadata(const std::string& path) override;
    ProtocolType getProtocolType() const override { return ProtocolType::SMB; }
    bool isConnected() const override { return connected_; }

private:
    bool connected_ = false;
    std::string endpoint_;
};

class HTTPAdapter : public ITransferAdapter {
public:
    Result<void> connect(const std::string& endpoint, const AuthInfo& auth) override;
    Result<void> disconnect() override;
    Result<void> sendChunk(const ChunkInfo& chunk, const BufferSegment& data) override;
    Result<BufferSegment> receiveChunk(const ChunkInfo& chunk) override;
    Result<void> seek(uint64_t offset) override;
    Result<FileMetadata> getFileMetadata(const std::string& path) override;
    ProtocolType getProtocolType() const override { return ProtocolType::HTTPS; }
    bool isConnected() const override { return connected_; }

private:
    bool connected_ = false;
    std::string endpoint_;
};

class FTPAdapter : public ITransferAdapter {
public:
    Result<void> connect(const std::string& endpoint, const AuthInfo& auth) override;
    Result<void> disconnect() override;
    Result<void> sendChunk(const ChunkInfo& chunk, const BufferSegment& data) override;
    Result<BufferSegment> receiveChunk(const ChunkInfo& chunk) override;
    Result<void> seek(uint64_t offset) override;
    Result<FileMetadata> getFileMetadata(const std::string& path) override;
    ProtocolType getProtocolType() const override { return ProtocolType::SFTP; }
    bool isConnected() const override { return connected_; }

private:
    bool connected_ = false;
    std::string endpoint_;
};

}
