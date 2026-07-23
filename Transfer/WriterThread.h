#pragma once

#include <string>
#include <thread>
#include <atomic>
#include <memory>
#include <chrono>
#include <functional>
#include "Transfer/ConcurrentQueue.h"
#include "Core/IDataSink.h"
#include "Core/Common/Result.h"
#include "Core/Domain/TransferTask.h"
#include "Logger/ILogger.h"

namespace ht {

struct TaskControl;
class IResumeEngine;

using ProgressCallback = std::function<void(const std::string& task_id,
                                             uint64_t transferred_bytes,
                                             uint64_t total_bytes,
                                             double speed_mbps,
                                             double avg_speed_mbps)>;
using ChunkCompletedCallback = std::function<void(const std::string& task_id,
                                                   uint64_t chunk_index,
                                                   uint64_t offset)>;

class WriterThread {
public:
    WriterThread(IDataSink* sink,
                 ConcurrentQueue& queue,
                 std::shared_ptr<IResumeEngine> resume_engine,
                 const std::string& task_id,
                 uint64_t total_bytes,
                 std::shared_ptr<ILogger> logger,
                 std::shared_ptr<TaskControl> ctrl,
                 ProgressCallback progress_cb,
                 ChunkCompletedCallback chunk_cb);

    void start();
    void join();
    bool hasError() const { return error_occurred_.load(); }
    uint64_t totalTransferred() const { return total_transferred_.load(); }

    WriterThread(const WriterThread&) = delete;
    WriterThread& operator=(const WriterThread&) = delete;

private:
    void writerLoop();
    bool writeChunk(DataChunk& chunk);
    void updateProgress(uint64_t chunk_size);

    IDataSink* sink_;
    ConcurrentQueue& queue_;
    std::shared_ptr<IResumeEngine> resume_engine_;
    std::string task_id_;
    uint64_t total_bytes_;
    std::shared_ptr<ILogger> logger_;
    std::shared_ptr<TaskControl> ctrl_;
    ProgressCallback progress_callback_;
    ChunkCompletedCallback chunk_completed_callback_;

    std::atomic<uint64_t> total_transferred_{0};
    std::atomic<bool> error_occurred_{false};
    std::chrono::steady_clock::time_point start_time_;
    std::thread thread_;
};

}