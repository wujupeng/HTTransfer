#pragma once

#include <string>
#include <filesystem>
#include <memory>
#include <chrono>
#include <functional>
#include <unordered_map>
#include <mutex>
#include "Core/Common/Result.h"
#include "Core/Common/Constants.h"
#include "Core/Domain/ChunkManifest.h"
#include "Core/BufferPool.h"
#include "Core/IOCDispatcher.h"
#include "Verify/VerifyEngine.h"

namespace ht {

enum class FileStability {
    Stable,
    Unstable,
    Timeout
};

struct FileEntry {
    std::filesystem::path source_path;
    std::filesystem::path relative_path;
    uint64_t file_size = 0;
};

class IFileEngine {
public:
    virtual ~IFileEngine() = default;
    virtual Result<std::vector<FileEntry>> scanDirectory(const std::filesystem::path& dir_path) = 0;
    virtual Result<ChunkManifest> createChunkManifest(const std::string& task_id, const std::filesystem::path& file_path) = 0;
    virtual Result<void> readChunkAsync(const std::filesystem::path& file_path,
                                         const ChunkInfo& chunk, BufferSegment buffer,
                                         std::function<void(Result<size_t>)> callback) = 0;
    virtual Result<void> writeChunkAsync(const std::filesystem::path& file_path,
                                          const ChunkInfo& chunk, const BufferSegment& buffer,
                                          std::function<void(Result<size_t>)> callback) = 0;
    virtual Result<FileStability> checkStability(const std::filesystem::path& file_path,
                                                  std::chrono::seconds observation_window) = 0;
    virtual Result<void> preallocateFile(const std::filesystem::path& file_path, uint64_t file_size) = 0;
    virtual Result<std::string> computeFileHash(const std::filesystem::path& file_path) = 0;
    virtual std::filesystem::path resolveTargetPath(const std::filesystem::path& source_path,
                                                     const std::filesystem::path& target_base) = 0;
    virtual void closeFileHandles(const std::filesystem::path& file_path) = 0;
};

class FileEngine : public IFileEngine {
public:
    explicit FileEngine(std::shared_ptr<BufferPool> buffer_pool,
                        std::shared_ptr<IOCDispatcher> iocp,
                        std::shared_ptr<VerifyEngine> verify_engine);

    Result<std::vector<FileEntry>> scanDirectory(const std::filesystem::path& dir_path) override;
    Result<ChunkManifest> createChunkManifest(const std::string& task_id, const std::filesystem::path& file_path) override;
    Result<void> readChunkAsync(const std::filesystem::path& file_path,
                                 const ChunkInfo& chunk, BufferSegment buffer,
                                 std::function<void(Result<size_t>)> callback) override;
    Result<void> writeChunkAsync(const std::filesystem::path& file_path,
                                  const ChunkInfo& chunk, const BufferSegment& buffer,
                                  std::function<void(Result<size_t>)> callback) override;
    Result<FileStability> checkStability(const std::filesystem::path& file_path,
                                          std::chrono::seconds observation_window) override;
    Result<void> preallocateFile(const std::filesystem::path& file_path, uint64_t file_size) override;
    Result<std::string> computeFileHash(const std::filesystem::path& file_path) override;
    std::filesystem::path resolveTargetPath(const std::filesystem::path& source_path,
                                             const std::filesystem::path& target_base) override;
    void closeFileHandles(const std::filesystem::path& file_path) override;

private:
    std::shared_ptr<BufferPool> buffer_pool_;
    std::shared_ptr<IOCDispatcher> iocp_;
    std::shared_ptr<VerifyEngine> verify_engine_;

    std::mutex handles_mutex_;
    std::unordered_map<std::wstring, void*> read_handles_;
    std::unordered_map<std::wstring, void*> write_handles_;

    void* getReadHandle(const std::filesystem::path& file_path);
    void* getWriteHandle(const std::filesystem::path& file_path);
};

}
