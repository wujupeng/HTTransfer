#include "WriterThread.h"
#include "Core/Common/Constants.h"
#include "Resume/ResumeEngine.h"
#include "Transfer/TransferEngine.h"

namespace ht {

WriterThread::WriterThread(IDataSink* sink,
                           ConcurrentQueue& queue,
                           std::shared_ptr<IResumeEngine> resume_engine,
                           const std::string& task_id,
                           uint64_t total_bytes,
                           std::shared_ptr<ILogger> logger,
                           std::shared_ptr<TaskControl> ctrl,
                           ProgressCallback progress_cb,
                           ChunkCompletedCallback chunk_cb)
    : sink_(sink),
      queue_(queue),
      resume_engine_(std::move(resume_engine)),
      task_id_(task_id),
      total_bytes_(total_bytes),
      logger_(std::move(logger)),
      ctrl_(std::move(ctrl)),
      progress_callback_(std::move(progress_cb)),
      chunk_completed_callback_(std::move(chunk_cb)) {}

void WriterThread::start() {
    start_time_ = std::chrono::steady_clock::now();
    thread_ = std::thread(&WriterThread::writerLoop, this);
}

void WriterThread::join() {
    if (thread_.joinable()) thread_.join();
}

void WriterThread::writerLoop() {
    try {
        while (true) {
            if (ctrl_->cancelled.load()) break;

            while (ctrl_->paused.load() && !ctrl_->cancelled.load()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            if (ctrl_->cancelled.load()) break;

            DataChunk chunk;
            if (!queue_.pop(chunk)) break;

            if (!writeChunk(chunk)) {
                error_occurred_.store(true);
                queue_.signalWriterError();
                break;
            }

            try {
                if (chunk_completed_callback_) {
                    chunk_completed_callback_(task_id_, chunk.chunk_index, chunk.offset + chunk.size);
                }
            } catch (...) {}

            try {
                if (resume_engine_) {
                    resume_engine_->markChunkCompleted(task_id_, chunk.chunk_index, chunk.offset + chunk.size);
                }
            } catch (...) {}

            updateProgress(chunk.size);
        }

        try {
            if (resume_engine_) {
                resume_engine_->flushPendingWrites();
            }
        } catch (...) {}
    } catch (const std::exception& e) {
        if (logger_) logger_->log(ILogger::Level::Critical, task_id_,
            std::format("Writer exception: {}", e.what()));
        error_occurred_.store(true);
        queue_.signalWriterError();
    } catch (...) {
        if (logger_) logger_->log(ILogger::Level::Critical, task_id_, "Writer unknown exception");
        error_occurred_.store(true);
        queue_.signalWriterError();
    }
}

bool WriterThread::writeChunk(DataChunk& chunk) {
    for (uint32_t retry = 0; retry < kMaxChunkRetries; ++retry) {
        if (ctrl_->cancelled.load()) return false;

        auto write_result = sink_->Write(chunk.offset, chunk.buffer.get(), chunk.size);
        if (write_result.isOk()) {
            return true;
        }

        if (logger_) logger_->log(ILogger::Level::Warning, task_id_,
            std::format("Writer chunk {} write failed (attempt {}/{})",
                chunk.chunk_index, retry + 1, kMaxChunkRetries));

        if (retry + 1 < kMaxChunkRetries) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100 * (retry + 1)));
        }
    }

    if (logger_) logger_->log(ILogger::Level::Error, task_id_,
        std::format("Writer chunk {} failed after {} retries", chunk.chunk_index, kMaxChunkRetries));
    return false;
}

void WriterThread::updateProgress(uint64_t chunk_size) {
    uint64_t transferred = total_transferred_.fetch_add(chunk_size) + chunk_size;

    try {
        if (progress_callback_ && transferred % (4 * kChunkSize) < chunk_size) {
            auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start_time_).count();
            double speed = elapsed_ms > 0 ? (transferred / 1048576.0) / (elapsed_ms / 1000.0) : 0;
            progress_callback_(task_id_, transferred, total_bytes_, speed, speed);
        }
    } catch (...) {}
}

}