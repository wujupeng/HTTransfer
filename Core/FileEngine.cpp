#include "FileEngine.h"
#include "Core/Domain/TransferTask.h"
#include <fstream>
#include <thread>

#ifdef _WIN32
#include <windows.h>
#endif

namespace ht {

FileEngine::FileEngine(std::shared_ptr<BufferPool> buffer_pool,
                       std::shared_ptr<IOCDispatcher> iocp,
                       std::shared_ptr<VerifyEngine> verify_engine)
    : buffer_pool_(std::move(buffer_pool)),
      iocp_(std::move(iocp)),
      verify_engine_(std::move(verify_engine)) {}

Result<std::vector<FileEntry>> FileEngine::scanDirectory(const std::filesystem::path& dir_path) {
    std::error_code ec;
    if (!std::filesystem::exists(dir_path, ec) || ec) {
        return Result<std::vector<FileEntry>>::failure(ErrorCode::SourceError, "Source directory does not exist");
    }

    if (!std::filesystem::is_directory(dir_path, ec) || ec) {
        return Result<std::vector<FileEntry>>::failure(ErrorCode::SourceError, "Path is not a directory");
    }

    std::vector<FileEntry> entries;

    for (const auto& entry : std::filesystem::recursive_directory_iterator(dir_path, std::filesystem::directory_options::skip_permission_denied, ec)) {
        if (ec) continue;
        if (!entry.is_regular_file()) continue;

        FileEntry fe;
        fe.source_path = entry.path();
        fe.relative_path = std::filesystem::relative(entry.path(), dir_path, ec);
        if (ec) {
            fe.relative_path = entry.path().filename();
        }
        fe.file_size = entry.file_size();
        entries.push_back(std::move(fe));
    }

    if (entries.empty()) {
        return Result<std::vector<FileEntry>>::failure(ErrorCode::SourceError, "No files found in directory");
    }

    return Result<std::vector<FileEntry>>::success(std::move(entries));
}

Result<ChunkManifest> FileEngine::createChunkManifest(const std::string& task_id, const std::filesystem::path& file_path) {
    std::error_code ec;
    if (!std::filesystem::exists(file_path, ec) || ec) {
        return Result<ChunkManifest>::failure(ErrorCode::SourceError, "Source file does not exist");
    }

    uint64_t file_size = std::filesystem::file_size(file_path, ec);
    if (ec) {
        return Result<ChunkManifest>::failure(ErrorCode::SourceError, "Cannot get file size");
    }
    auto manifest = ht::createChunkManifest(task_id, file_size);
    return Result<ChunkManifest>::success(std::move(manifest));
}

void* FileEngine::getReadHandle(const std::filesystem::path& file_path) {
    auto key = file_path.wstring();
    std::lock_guard<std::mutex> lock(handles_mutex_);
    auto it = read_handles_.find(key);
    if (it != read_handles_.end()) return it->second;

#ifdef _WIN32
    HANDLE hFile = CreateFileW(key.c_str(), GENERIC_READ,
        FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) return nullptr;
    read_handles_[key] = hFile;
    return hFile;
#else
    return nullptr;
#endif
}

void* FileEngine::getWriteHandle(const std::filesystem::path& file_path) {
    auto key = file_path.wstring();
    std::lock_guard<std::mutex> lock(handles_mutex_);
    auto it = write_handles_.find(key);
    if (it != write_handles_.end()) return it->second;

#ifdef _WIN32
    HANDLE hFile = CreateFileW(key.c_str(), GENERIC_WRITE,
        0, nullptr, OPEN_ALWAYS, FILE_FLAG_OVERLAPPED, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) return nullptr;
    write_handles_[key] = hFile;
    return hFile;
#else
    return nullptr;
#endif
}

void FileEngine::closeFileHandles(const std::filesystem::path& file_path) {
    auto key = file_path.wstring();
    std::lock_guard<std::mutex> lock(handles_mutex_);
#ifdef _WIN32
    auto rit = read_handles_.find(key);
    if (rit != read_handles_.end()) {
        CloseHandle(static_cast<HANDLE>(rit->second));
        read_handles_.erase(rit);
    }
    auto wit = write_handles_.find(key);
    if (wit != write_handles_.end()) {
        CloseHandle(static_cast<HANDLE>(wit->second));
        write_handles_.erase(wit);
    }
#endif
}

Result<void> FileEngine::readChunkAsync(const std::filesystem::path& file_path,
                                         const ChunkInfo& chunk, BufferSegment buffer,
                                         std::function<void(Result<size_t>)> callback) {
#ifdef _WIN32
    HANDLE hFile = static_cast<HANDLE>(getReadHandle(file_path));
    if (!hFile) {
        return Result<void>::failure(ErrorCode::SourceError, "Cannot open source file");
    }

    auto result = iocp_->submitRead(hFile, chunk.offset, static_cast<uint32_t>(chunk.size),
                                     buffer.data, 0);
    return result;
#else
    std::ifstream file(file_path, std::ios::binary);
    if (!file) {
        return Result<void>::failure(ErrorCode::SourceError, "Cannot open source file");
    }
    file.seekg(chunk.offset);
    file.read(reinterpret_cast<char*>(buffer.data), chunk.size);
    buffer.size = static_cast<uint64_t>(file.gcount());
    if (callback) callback(Result<size_t>::success(static_cast<size_t>(buffer.size)));
    return Result<void>::success();
#endif
}

Result<void> FileEngine::writeChunkAsync(const std::filesystem::path& file_path,
                                          const ChunkInfo& chunk, const BufferSegment& buffer,
                                          std::function<void(Result<size_t>)> callback) {
#ifdef _WIN32
    HANDLE hFile = static_cast<HANDLE>(getWriteHandle(file_path));
    if (!hFile) {
        return Result<void>::failure(ErrorCode::TargetError, "Cannot open target file");
    }

    auto result = iocp_->submitWrite(hFile, chunk.offset, static_cast<uint32_t>(buffer.size),
                                      buffer.data, 0);
    return result;
#else
    std::ofstream file(file_path, std::ios::binary | std::ios::in);
    if (!file) {
        return Result<void>::failure(ErrorCode::TargetError, "Cannot open target file");
    }
    file.seekp(chunk.offset);
    file.write(reinterpret_cast<const char*>(buffer.data), buffer.size);
    if (callback) callback(Result<size_t>::success(static_cast<size_t>(buffer.size)));
    return Result<void>::success();
#endif
}

Result<FileStability> FileEngine::checkStability(const std::filesystem::path& file_path,
                                                  std::chrono::seconds observation_window) {
    std::error_code ec;
    if (!std::filesystem::exists(file_path, ec) || ec) {
        return Result<FileStability>::failure(ErrorCode::SourceError, "File does not exist");
    }

    auto max_wait = std::chrono::steady_clock::now() + kMaxStabilityWait;

    uint64_t prev_size = std::filesystem::file_size(file_path, ec);
    if (ec) return Result<FileStability>::success(FileStability::Stable);
    auto prev_mtime = std::filesystem::last_write_time(file_path, ec);
    if (ec) return Result<FileStability>::success(FileStability::Stable);

    while (true) {
        std::this_thread::sleep_for(observation_window);

        if (!std::filesystem::exists(file_path, ec) || ec) {
            return Result<FileStability>::failure(ErrorCode::SourceInaccessible, "File disappeared");
        }

        uint64_t cur_size = std::filesystem::file_size(file_path, ec);
        if (ec) return Result<FileStability>::success(FileStability::Stable);
        auto cur_mtime = std::filesystem::last_write_time(file_path, ec);
        if (ec) return Result<FileStability>::success(FileStability::Stable);

        if (cur_size == prev_size && cur_mtime == prev_mtime) {
            return Result<FileStability>::success(FileStability::Stable);
        }

        prev_size = cur_size;
        prev_mtime = cur_mtime;

        if (std::chrono::steady_clock::now() > max_wait) {
            return Result<FileStability>::success(FileStability::Timeout);
        }
    }
}

Result<void> FileEngine::preallocateFile(const std::filesystem::path& file_path, uint64_t file_size) {
#ifdef _WIN32
    ULARGE_INTEGER free_bytes;
    if (GetDiskFreeSpaceExW(file_path.parent_path().wstring().c_str(), &free_bytes, nullptr, nullptr)) {
        if (free_bytes.QuadPart < file_size) {
            return Result<void>::failure(ErrorCode::StorageError, "Insufficient disk space on target");
        }
    }

    HANDLE hFile = CreateFileW(file_path.wstring().c_str(), GENERIC_WRITE,
        0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) {
        return Result<void>::failure(ErrorCode::TargetError, "Cannot create target file");
    }

    LARGE_INTEGER li;
    li.QuadPart = static_cast<LONGLONG>(file_size);
    if (!SetFilePointerEx(hFile, li, nullptr, FILE_BEGIN)) {
        CloseHandle(hFile);
        return Result<void>::failure(ErrorCode::IOError, "SetFilePointerEx failed");
    }
    if (!SetEndOfFile(hFile)) {
        DWORD err = GetLastError();
        CloseHandle(hFile);
        if (err == ERROR_DISK_FULL) {
            return Result<void>::failure(ErrorCode::StorageError, "Insufficient disk space on target");
        }
        return Result<void>::failure(ErrorCode::IOError, "SetEndOfFile failed");
    }

    CloseHandle(hFile);
#else
    auto space = std::filesystem::space(file_path.parent_path());
    if (space.available < file_size) {
        return Result<void>::failure(ErrorCode::StorageError, "Insufficient disk space on target");
    }

    std::ofstream file(file_path, std::ios::binary);
    if (!file) return Result<void>::failure(ErrorCode::TargetError, "Cannot create target file");
    file.seekp(file_size - 1);
    file.put('\0');
#endif
    return Result<void>::success();
}

Result<std::string> FileEngine::computeFileHash(const std::filesystem::path& file_path) {
    return verify_engine_->computeFileHash(file_path);
}

std::filesystem::path FileEngine::resolveTargetPath(const std::filesystem::path& source_path,
                                                     const std::filesystem::path& target_base) {
    std::error_code ec;
    if ((std::filesystem::is_directory(target_base, ec) && !ec) ||
        (std::filesystem::exists(target_base, ec) && !ec)) {
        if (std::filesystem::is_directory(target_base, ec) && !ec) {
            return target_base / source_path.filename();
        }
    }
    return target_base;
}

}
