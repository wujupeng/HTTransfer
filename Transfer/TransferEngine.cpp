#include "TransferEngine.h"
#include "Core/LocalFileSource.h"
#include "Core/LocalFileSink.h"
#include "Core/Common/Types.h"
#include "Logger/ILogger.h"
#include "Transfer/ReaderPool.h"
#include "Transfer/WriterThread.h"
#include <algorithm>
#include <format>
#include <fstream>

#ifdef _WIN32
#include <windows.h>
#endif

namespace ht {

RetryController::RetryController(std::chrono::seconds base_delay, std::chrono::seconds max_delay)
    : base_delay_(base_delay), max_delay_(max_delay) {}

std::chrono::seconds RetryController::nextDelay() {
    auto delay = std::min(
        std::chrono::seconds(static_cast<long long>(base_delay_.count() * (1LL << retry_count_))),
        max_delay_);
    retry_count_++;
    return delay;
}

void RetryController::reset() { retry_count_ = 0; }

WorkerPool::WorkerPool(uint32_t worker_count) : worker_count_(worker_count) {
    running_ = true;
    workers_.reserve(worker_count);
    for (uint32_t i = 0; i < worker_count; ++i) {
        workers_.emplace_back(&WorkerPool::workerLoop, this);
    }
}

WorkerPool::~WorkerPool() { shutdown(); }

void WorkerPool::submit(WorkItem item) {
    {
        std::lock_guard lock(mutex_);
        queue_.push(std::move(item));
    }
    cv_.notify_one();
}

void WorkerPool::waitAll() {
    std::unique_lock lock(mutex_);
    done_cv_.wait(lock, [this] { return queue_.empty() && active_count_ == 0; });
}

void WorkerPool::shutdown() {
    if (!running_) return;
    running_ = false;
    cv_.notify_all();
    for (auto& t : workers_) {
        if (t.joinable()) t.join();
    }
    workers_.clear();
}

void WorkerPool::workerLoop() {
    while (running_) {
        WorkItem item;
        {
            std::unique_lock lock(mutex_);
            cv_.wait(lock, [this] { return !queue_.empty() || !running_; });
            if (!running_ && queue_.empty()) return;
            if (queue_.empty()) continue;
            item = std::move(queue_.front());
            queue_.pop();
            active_count_++;
        }
        item();
        {
            std::lock_guard lock(mutex_);
            active_count_--;
        }
        done_cv_.notify_all();
    }
}

TransferEngine::TransferEngine(std::shared_ptr<ILogger> logger,
                               std::shared_ptr<BufferPool> buffer_pool,
                               std::shared_ptr<IResumeEngine> resume_engine)
    : logger_(std::move(logger)),
      buffer_pool_(std::move(buffer_pool)),
      resume_engine_(std::move(resume_engine)),
      worker_pool_(std::make_unique<WorkerPool>(kDefaultParallelism)) {
#ifndef NDEBUG
    magic_ = 0xDEADBEEF;
#endif
}

TransferEngine::~TransferEngine() {
#ifndef NDEBUG
    magic_ = 0x0;
#endif
}

void TransferEngine::validateMagic() const {
#ifndef NDEBUG
    assert(magic_ == 0xDEADBEEF && "TransferEngine use-after-free detected!");
#endif
}

bool TransferEngine::isSMB(const std::string& path) {
    return path.size() >= 2 && path[0] == '\\' && path[1] == '\\';
}

std::shared_ptr<TaskControl> TransferEngine::getTaskControl(const std::string& task_id) {
    std::lock_guard lock(task_control_mutex_);
    auto it = task_controls_.find(task_id);
    if (it != task_controls_.end()) return it->second;
    auto ctrl = std::make_shared<TaskControl>();
    task_controls_[task_id] = ctrl;
    return ctrl;
}

void TransferEngine::registerAdapter(ProtocolType protocol, std::unique_ptr<ITransferAdapter> adapter) {
    adapters_[protocol] = std::move(adapter);
}

Result<void> TransferEngine::startTransfer(const TransferTask& task, const ChunkManifest& manifest) {
    try {
    validateMagic();
    auto ctrl = getTaskControl(task.task_id);
    ctrl->paused = false;
    ctrl->cancelled = false;

    if (logger_) logger_->log(ILogger::Level::Info, task.task_id,
        std::format("Transfer: Reader->Queue->Writer mode (parallelism={})", parallelism_));

    LocalFileSource source;
    auto src_result = source.Open(task.source_path);
    if (src_result.isErr()) {
        return Result<void>::failure(src_result.errorCode(), src_result.errorMessage());
    }
    source.Close();

    LocalFileSink sink;
    auto sink_result = sink.Open(task.target_path);
    if (sink_result.isErr()) {
        return Result<void>::failure(sink_result.errorCode(), sink_result.errorMessage());
    }
    sink.Close();

    return startTransferReaderWriter(task, manifest, nullptr, nullptr, ctrl);

    } catch (const std::exception& e) {
        return Result<void>::failure(ErrorCode::IOError, std::string("Transfer exception: ") + e.what());
    } catch (...) {
        return Result<void>::failure(ErrorCode::IOError, "Transfer unknown exception");
    }
}

Result<void> TransferEngine::startTransferSingleThread(const TransferTask& task,
                                                        const ChunkManifest& manifest,
                                                        IDataSource* source, IDataSink* sink,
                                                        std::shared_ptr<TaskControl> ctrl) {
    uint64_t total_transferred = 0;
    auto start_time = std::chrono::steady_clock::now();

    std::vector<uint8_t> local_buffer(kChunkSize);
    uint8_t* buffer_ptr = local_buffer.data();

    BufferSegment pool_buffer{};
    bool use_pool = (buffer_pool_ != nullptr);
    if (use_pool) {
        auto pool_result = buffer_pool_->acquire();
        if (pool_result.isOk()) {
            pool_buffer = std::move(pool_result.value());
            buffer_ptr = pool_buffer.data;
        } else {
            use_pool = false;
        }
    }

    for (const auto& chunk : manifest.chunks) {
        if (ctrl->cancelled.load()) break;
        while (ctrl->paused.load() && !ctrl->cancelled.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        if (ctrl->cancelled.load()) break;

        if (chunk.status == ChunkStatus::Verified) {
            total_transferred += chunk.size;
            continue;
        }

        bool chunk_ok = false;
        for (uint32_t retry = 0; retry < kMaxChunkRetries && !chunk_ok; ++retry) {
            auto read_result = source->Read(chunk.offset, buffer_ptr, static_cast<size_t>(chunk.size));
            if (read_result.isErr() || read_result.value() == 0) {
                if (logger_) logger_->log(ILogger::Level::Warning, task.task_id,
                    std::format("Chunk {} read failed (attempt {}/{})",
                        chunk.chunk_index, retry + 1, kMaxChunkRetries));
                if (retry + 1 < kMaxChunkRetries) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(100 * (retry + 1)));
                }
                continue;
            }

            auto write_result = sink->Write(chunk.offset, buffer_ptr, read_result.value());
            if (write_result.isErr()) {
                if (logger_) logger_->log(ILogger::Level::Warning, task.task_id,
                    std::format("Chunk {} write failed (attempt {}/{})",
                        chunk.chunk_index, retry + 1, kMaxChunkRetries));
                if (retry + 1 < kMaxChunkRetries) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(100 * (retry + 1)));
                }
                continue;
            }
            chunk_ok = true;
        }

        if (!chunk_ok) {
            if (logger_) logger_->log(ILogger::Level::Error, task.task_id,
                std::format("Chunk {} failed after {} retries", chunk.chunk_index, kMaxChunkRetries));
        }

        total_transferred += chunk.size;

        if (progress_callback_) {
            auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start_time).count();
            double speed = elapsed_ms > 0 ? (total_transferred / 1048576.0) / (elapsed_ms / 1000.0) : 0;
            progress_callback_(task.task_id, total_transferred, task.total_bytes, speed, speed);
        }
    }

    if (use_pool && pool_buffer.data) {
        buffer_pool_->release(std::move(pool_buffer));
    }

    sink->Flush();
    source->Close();
    sink->Close();

    return Result<void>::success();
}



Result<void> TransferEngine::startTransferReaderWriter(const TransferTask& task,
                                                        const ChunkManifest& manifest,
                                                        IDataSource* source, IDataSink* sink,
                                                        std::shared_ptr<TaskControl> ctrl) {
    validateMagic();

    uint32_t reader_count = isSMB(task.target_path) ? 1 : parallelism_;
    reader_count = std::min(reader_count, static_cast<uint32_t>(manifest.chunks.size()));
    if (reader_count == 0) reader_count = 1;

    if (logger_) logger_->log(ILogger::Level::Info, task.task_id,
        std::format("startTransferReaderWriter: reader_count={}, isSMB={}",
            reader_count, isSMB(task.target_path)));

    ConcurrentQueue queue(reader_count * 4);
    queue.setActiveReaders(reader_count);

    LocalFileSink local_sink;
    auto sink_result = local_sink.Open(task.target_path);
    if (sink_result.isErr()) {
        return Result<void>::failure(sink_result.errorCode(), sink_result.errorMessage());
    }

    ReaderPool reader_pool(task.source_path, manifest, queue, logger_, reader_count, ctrl);

    WriterThread writer_thread(&local_sink, queue, resume_engine_,
        task.task_id, task.total_bytes, logger_, ctrl,
        progress_callback_, chunk_completed_callback_);

    reader_pool.start();
    writer_thread.start();

    reader_pool.join();
    writer_thread.join();

    local_sink.Flush();
    local_sink.Close();

    if (writer_thread.hasError()) {
        return Result<void>::failure(ErrorCode::IOError, "Writer thread encountered errors");
    }

    return Result<void>::success();
}

Result<void> TransferEngine::pauseTransfer(const std::string& task_id) {
    auto ctrl = getTaskControl(task_id);
    ctrl->paused = true;
    return Result<void>::success();
}

Result<void> TransferEngine::resumeTransfer(const std::string& task_id) {
    auto ctrl = getTaskControl(task_id);
    ctrl->paused = false;
    return Result<void>::success();
}

Result<void> TransferEngine::cancelTransfer(const std::string& task_id) {
    auto ctrl = getTaskControl(task_id);
    ctrl->cancelled = true;
    return Result<void>::success();
}

void TransferEngine::setParallelism(uint32_t count) {
    auto safe_count = std::min(count, kMaxParallelism);
    if (safe_count == 0) safe_count = 1;
    parallelism_ = safe_count;
    if (logger_) logger_->log(ILogger::Level::Info, "SYSTEM",
        std::format("setParallelism: count={}", safe_count));
}

uint32_t TransferEngine::getParallelism() const { return parallelism_; }

void TransferEngine::setProgressCallback(ProgressCallback callback) {
    progress_callback_ = std::move(callback);
}

void TransferEngine::setChunkCompletedCallback(ChunkCompletedCallback callback) {
    chunk_completed_callback_ = std::move(callback);
}

Result<void> SMBAdapter::connect(const std::string& endpoint, const AuthInfo& auth) {
    endpoint_ = endpoint;
    connected_ = true;
    return Result<void>::success();
}

Result<void> SMBAdapter::disconnect() {
    connected_ = false;
    return Result<void>::success();
}

Result<void> SMBAdapter::sendChunk(const ChunkInfo& chunk, const BufferSegment& data) {
    return Result<void>::success();
}

Result<BufferSegment> SMBAdapter::receiveChunk(const ChunkInfo& chunk) {
    return Result<BufferSegment>::failure(ErrorCode::IOError, "Not implemented");
}

Result<void> SMBAdapter::seek(uint64_t offset) { return Result<void>::success(); }

Result<FileMetadata> SMBAdapter::getFileMetadata(const std::string& path) {
    FileMetadata meta;
#ifdef _WIN32
    auto fs_path = utf8ToPath(path);
    WIN32_FILE_ATTRIBUTE_DATA attrs{};
    if (GetFileAttributesExW(fs_path.wstring().c_str(), GetFileExInfoStandard, &attrs)) {
        meta.file_size = (static_cast<uint64_t>(attrs.nFileSizeHigh) << 32) | attrs.nFileSizeLow;
    }
#endif
    return Result<FileMetadata>::success(std::move(meta));
}

Result<void> HTTPAdapter::connect(const std::string& endpoint, const AuthInfo& auth) {
    endpoint_ = endpoint;
    connected_ = true;
    return Result<void>::success();
}

Result<void> HTTPAdapter::disconnect() {
    connected_ = false;
    return Result<void>::success();
}

Result<void> HTTPAdapter::sendChunk(const ChunkInfo& chunk, const BufferSegment& data) {
    return Result<void>::success();
}

Result<BufferSegment> HTTPAdapter::receiveChunk(const ChunkInfo& chunk) {
    return Result<BufferSegment>::failure(ErrorCode::IOError, "Not implemented");
}

Result<void> HTTPAdapter::seek(uint64_t offset) { return Result<void>::success(); }

Result<FileMetadata> HTTPAdapter::getFileMetadata(const std::string& path) {
    return Result<FileMetadata>::success(FileMetadata{});
}

Result<void> FTPAdapter::connect(const std::string& endpoint, const AuthInfo& auth) {
    endpoint_ = endpoint;
    connected_ = true;
    return Result<void>::success();
}

Result<void> FTPAdapter::disconnect() {
    connected_ = false;
    return Result<void>::success();
}

Result<void> FTPAdapter::sendChunk(const ChunkInfo& chunk, const BufferSegment& data) {
    return Result<void>::success();
}

Result<BufferSegment> FTPAdapter::receiveChunk(const ChunkInfo& chunk) {
    return Result<BufferSegment>::failure(ErrorCode::IOError, "Not implemented");
}

Result<void> FTPAdapter::seek(uint64_t offset) { return Result<void>::success(); }

Result<FileMetadata> FTPAdapter::getFileMetadata(const std::string& path) {
    return Result<FileMetadata>::success(FileMetadata{});
}

}
