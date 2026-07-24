#include "ReaderPool.h"
#include "Core/LocalFileSource.h"
#include "Core/Common/Constants.h"
#include "Transfer/TransferEngine.h"

namespace ht {

ReaderPool::ReaderPool(const std::string& source_path,
                       const ChunkManifest& manifest,
                       ConcurrentQueue& queue,
                       std::shared_ptr<ILogger> logger,
                       uint32_t reader_count,
                       std::shared_ptr<TaskControl> ctrl)
    : source_path_(source_path),
      manifest_(manifest),
      queue_(queue),
      logger_(std::move(logger)),
      reader_count_(reader_count),
      ctrl_(std::move(ctrl)) {}

void ReaderPool::start() {
    threads_.reserve(reader_count_);
    for (uint32_t i = 0; i < reader_count_; ++i) {
        threads_.emplace_back(&ReaderPool::readerLoop, this, i);
    }
}

void ReaderPool::join() {
    for (auto& t : threads_) {
        if (t.joinable()) t.join();
    }
}

void ReaderPool::readerLoop(uint32_t reader_id) {
    try {
        LocalFileSource local_src;
        auto src_result = local_src.Open(source_path_);
        if (src_result.isErr()) {
            if (logger_) logger_->log(ILogger::Level::Error, "SYSTEM",
                std::format("Reader {} failed to open source: {}", reader_id, src_result.errorMessage()));
            queue_.decrementActiveReaders();
            return;
        }

        while (true) {
            if (ctrl_->cancelled.load() || queue_.isWriterError()) break;

            uint64_t idx = next_chunk_index_.fetch_add(1);
            if (idx >= manifest_.chunks.size()) break;

            const auto& chunk = manifest_.chunks[idx];
            if (chunk.status == ChunkStatus::Verified) {
                continue;
            }

            bool chunk_ok = false;
            for (uint32_t retry = 0; retry < kMaxChunkRetries && !chunk_ok; ++retry) {
                if (ctrl_->cancelled.load() || queue_.isWriterError()) break;

                auto buffer = std::make_unique<uint8_t[]>(chunk.size);
                auto read_result = local_src.Read(chunk.offset, buffer.get(), static_cast<size_t>(chunk.size));
                if (read_result.isErr() || read_result.value() == 0) {
                    if (logger_ && retry == 0) logger_->log(ILogger::Level::Warning, "SYSTEM",
                        std::format("Reader {} chunk {} read failed (attempt {}/{})",
                            reader_id, idx, retry + 1, kMaxChunkRetries));
                    if (retry + 1 < kMaxChunkRetries) {
                        std::this_thread::sleep_for(std::chrono::milliseconds(100 * (retry + 1)));
                    }
                    continue;
                }

                size_t bytes_read = read_result.value();
                DataChunk data_chunk(idx, chunk.offset, bytes_read, std::move(buffer));

                if (!queue_.push(std::move(data_chunk))) {
                    break;
                }
                chunk_ok = true;
            }

            if (!chunk_ok && !queue_.isWriterError() && !ctrl_->cancelled.load()) {
                if (logger_) logger_->log(ILogger::Level::Error, "SYSTEM",
                    std::format("Reader {} chunk {} failed after {} retries", reader_id, idx, kMaxChunkRetries));
                queue_.signalWriterError();
                break;
            }
        }

        local_src.Close();
    } catch (const std::exception& e) {
        if (logger_) logger_->log(ILogger::Level::Critical, "SYSTEM",
            std::format("Reader {} exception: {}", reader_id, e.what()));
    } catch (...) {
        if (logger_) logger_->log(ILogger::Level::Critical, "SYSTEM",
            std::format("Reader {} unknown exception", reader_id));
    }

    queue_.decrementActiveReaders();
}

}