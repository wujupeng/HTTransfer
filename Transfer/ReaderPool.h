#pragma once

#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <memory>
#include "Core/Domain/ChunkManifest.h"
#include "Transfer/ConcurrentQueue.h"
#include "Core/BufferPool.h"
#include "Logger/ILogger.h"

namespace ht {

struct TaskControl;

class ReaderPool {
public:
    ReaderPool(const std::string& source_path,
               const ChunkManifest& manifest,
               ConcurrentQueue& queue,
               std::shared_ptr<ILogger> logger,
               uint32_t reader_count,
               std::shared_ptr<TaskControl> ctrl);

    void start();
    void join();

    ReaderPool(const ReaderPool&) = delete;
    ReaderPool& operator=(const ReaderPool&) = delete;

private:
    void readerLoop(uint32_t reader_id);

    std::string source_path_;
    const ChunkManifest& manifest_;
    ConcurrentQueue& queue_;
    std::shared_ptr<ILogger> logger_;
    uint32_t reader_count_;
    std::shared_ptr<TaskControl> ctrl_;
    std::atomic<uint64_t> next_chunk_index_{0};
    std::vector<std::thread> threads_;
};

}