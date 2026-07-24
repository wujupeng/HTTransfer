#include "ResumeEngine.h"
#include "Core/Common/Types.h"
#include <fstream>
#include <cstring>
#include <algorithm>
#include <format>

#ifdef _WIN32
#include <windows.h>
#endif

namespace ht {

static std::string pathStemToUtf8(const std::filesystem::path& p) {
#ifdef _WIN32
    auto wstr = p.stem().wstring();
    if (wstr.empty()) return "";
    int len = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (len <= 0) return "";
    std::string result(len - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, result.data(), len, nullptr, nullptr);
    return result;
#else
    return p.stem().string();
#endif
}

ResumeEngine::ResumeEngine(std::shared_ptr<ILogger> logger, const std::filesystem::path& resume_dir)
    : logger_(std::move(logger)), resume_dir_(resume_dir) {
    std::filesystem::create_directories(resume_dir_);
}

std::filesystem::path ResumeEngine::getResumePath(const std::string& task_id) const {
    return resume_dir_ / (task_id + ".htresume");
}

Result<void> ResumeEngine::createResumeFile(const std::string& task_id, const ResumeFileData& data) {
    cache_[task_id] = data;
    auto path = getResumePath(task_id);

    std::ofstream file(path, std::ios::binary);
    if (!file) {
        return Result<void>::failure(ErrorCode::IOError, "Cannot create resume file");
    }

    ResumeFileHeader header;
    file.write(reinterpret_cast<const char*>(&header.magic), sizeof(header.magic));
    file.write(reinterpret_cast<const char*>(&header.version), sizeof(header.version));
    file.write(reinterpret_cast<const char*>(&header.flags), sizeof(header.flags));

    uint16_t id_len = static_cast<uint16_t>(task_id.size());
    file.write(reinterpret_cast<const char*>(&id_len), sizeof(id_len));
    file.write(task_id.data(), id_len);

    uint16_t src_len = static_cast<uint16_t>(data.source_path.size());
    file.write(reinterpret_cast<const char*>(&src_len), sizeof(src_len));
    file.write(data.source_path.data(), src_len);

    uint16_t dst_len = static_cast<uint16_t>(data.target_path.size());
    file.write(reinterpret_cast<const char*>(&dst_len), sizeof(dst_len));
    file.write(data.target_path.data(), dst_len);

    file.write(reinterpret_cast<const char*>(&data.file_size), sizeof(data.file_size));
    file.write(reinterpret_cast<const char*>(&data.current_offset), sizeof(data.current_offset));

    uint64_t completed_count = data.completed_chunks.size();
    file.write(reinterpret_cast<const char*>(&completed_count), sizeof(completed_count));
    for (uint64_t idx : data.completed_chunks) {
        file.write(reinterpret_cast<const char*>(&idx), sizeof(idx));
    }

    file.write(data.source_hash.data(), 64);

    auto write_time = [](const std::chrono::system_clock::time_point& tp) {
        int64_t ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            tp.time_since_epoch()).count();
        return ms;
    };

    int64_t ct = write_time(data.source_create_time);
    int64_t mt = write_time(data.source_modify_time);
    int64_t ut = write_time(data.updated_at);
    file.write(reinterpret_cast<const char*>(&ct), sizeof(ct));
    file.write(reinterpret_cast<const char*>(&mt), sizeof(mt));
    file.write(reinterpret_cast<const char*>(&ut), sizeof(ut));

    file.flush();
    return Result<void>::success();
}

Result<std::optional<ResumeFileData>> ResumeEngine::loadResumeFile(const std::string& task_id) {
    {
        std::lock_guard lock(mutex_);
        auto it = cache_.find(task_id);
        if (it != cache_.end()) {
            return Result<std::optional<ResumeFileData>>::success(it->second);
        }
    }

    auto path = getResumePath(task_id);
    if (!std::filesystem::exists(path)) {
        return Result<std::optional<ResumeFileData>>::success(std::nullopt);
    }

    auto result = ResumeFileParser::parse(path);
    if (result.isOk()) {
        std::lock_guard lock(mutex_);
        cache_[task_id] = result.value();
        return Result<std::optional<ResumeFileData>>::success(result.value());
    }
    return Result<std::optional<ResumeFileData>>::failure(result.errorCode(), result.errorMessage());
}

Result<void> ResumeEngine::updateResumeFile(const std::string& task_id, const ResumeFileData& data) {
    std::lock_guard lock(mutex_);
    cache_[task_id] = data;
    return createResumeFile(task_id, data);
}

Result<void> ResumeEngine::markChunkCompleted(const std::string& task_id, uint64_t chunk_index, uint64_t offset) {
    std::lock_guard lock(mutex_);
    auto it = cache_.find(task_id);
    if (it == cache_.end()) {
        return Result<void>::failure(ErrorCode::IOError, "ResumeFile not found in cache");
    }

    auto& data = it->second;
    auto pos = std::lower_bound(data.completed_chunks.begin(), data.completed_chunks.end(), chunk_index);
    if (pos == data.completed_chunks.end() || *pos != chunk_index) {
        data.completed_chunks.insert(pos, chunk_index);
    }
    data.current_offset = offset;
    data.updated_at = std::chrono::system_clock::now();

    pending_writes_++;
    if (pending_writes_ >= kFlushInterval) {
        auto result = createResumeFile(task_id, data);
        pending_writes_ = 0;
        return result;
    }

    return Result<void>::success();
}

Result<void> ResumeEngine::flushPendingWrites() {
    std::lock_guard lock(mutex_);
    if (pending_writes_ == 0) return Result<void>::success();

    for (auto& [task_id, data] : cache_) {
        auto result = createResumeFile(task_id, data);
        if (result.isErr()) return result;
    }
    pending_writes_ = 0;
    return Result<void>::success();
}

bool ResumeEngine::atomicWrite(const std::filesystem::path& target_path, const std::vector<uint8_t>& data) {
    auto temp_path = target_path;
    temp_path += ".tmp";

    {
        std::ofstream file(temp_path, std::ios::binary);
        if (!file) return false;
        file.write(reinterpret_cast<const char*>(data.data()), data.size());
        if (!file) return false;
        file.flush();
    }

    std::error_code ec;
    std::filesystem::rename(temp_path, target_path, ec);
    if (ec) {
        std::filesystem::remove(temp_path, ec);
        return false;
    }
    return true;
}

Result<bool> ResumeEngine::isSourceFileChanged(const std::string& task_id, const std::filesystem::path& source_path) {
    std::lock_guard lock(mutex_);
    auto it = cache_.find(task_id);
    if (it == cache_.end()) return Result<bool>::success(true);

    std::error_code ec;
    if (!std::filesystem::exists(source_path, ec) || ec) return Result<bool>::success(true);

    auto fsize = std::filesystem::file_size(source_path, ec);
    if (ec) return Result<bool>::success(true);

    if (fsize != it->second.file_size) return Result<bool>::success(true);

    return Result<bool>::success(false);
}

Result<void> ResumeEngine::invalidateResumeFile(const std::string& task_id) {
    std::lock_guard lock(mutex_);
    cache_.erase(task_id);
    auto path = getResumePath(task_id);
    std::error_code ec;
    std::filesystem::remove(path, ec);
    return Result<void>::success();
}

Result<std::vector<std::string>> ResumeEngine::scanUnfinishedTasks() {
    std::vector<std::string> tasks;
    if (!std::filesystem::exists(resume_dir_)) {
        return Result<std::vector<std::string>>::success(std::move(tasks));
    }

    for (const auto& entry : std::filesystem::directory_iterator(resume_dir_)) {
        if (entry.path().extension() == ".htresume") {
            tasks.push_back(pathStemToUtf8(entry.path()));
        }
    }

    return Result<std::vector<std::string>>::success(std::move(tasks));
}

Result<ResumeFileData> ResumeFileParser::parse(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return Result<ResumeFileData>::failure(ErrorCode::IOError, "Cannot open resume file");
    }

    ResumeFileData data;
    ResumeFileHeader header;

    file.read(reinterpret_cast<char*>(&header.magic), sizeof(header.magic));
    file.read(reinterpret_cast<char*>(&header.version), sizeof(header.version));
    file.read(reinterpret_cast<char*>(&header.flags), sizeof(header.flags));

    if (header.magic != kResumeMagic) {
        return Result<ResumeFileData>::failure(ErrorCode::IOError, "Invalid resume file magic");
    }

    uint16_t id_len = 0;
    file.read(reinterpret_cast<char*>(&id_len), sizeof(id_len));
    if (id_len > 256) {
        return Result<ResumeFileData>::failure(ErrorCode::IOError, "Invalid resume file: task_id too long");
    }
    data.task_id.resize(id_len);
    file.read(data.task_id.data(), id_len);

    uint16_t src_len = 0;
    file.read(reinterpret_cast<char*>(&src_len), sizeof(src_len));
    if (src_len > 4096) {
        return Result<ResumeFileData>::failure(ErrorCode::IOError, "Invalid resume file: source_path too long (old format?)");
    }
    data.source_path.resize(src_len);
    file.read(data.source_path.data(), src_len);

    uint16_t dst_len = 0;
    file.read(reinterpret_cast<char*>(&dst_len), sizeof(dst_len));
    if (dst_len > 4096) {
        return Result<ResumeFileData>::failure(ErrorCode::IOError, "Invalid resume file: target_path too long (old format?)");
    }
    data.target_path.resize(dst_len);
    file.read(data.target_path.data(), dst_len);

    file.read(reinterpret_cast<char*>(&data.file_size), sizeof(data.file_size));
    file.read(reinterpret_cast<char*>(&data.current_offset), sizeof(data.current_offset));

    uint64_t completed_count = 0;
    file.read(reinterpret_cast<char*>(&completed_count), sizeof(completed_count));
    if (completed_count > 1000000) {
        return Result<ResumeFileData>::failure(ErrorCode::IOError, "Invalid resume file: too many completed chunks");
    }
    data.completed_chunks.resize(static_cast<size_t>(completed_count));
    for (uint64_t i = 0; i < completed_count; ++i) {
        file.read(reinterpret_cast<char*>(&data.completed_chunks[static_cast<size_t>(i)]), sizeof(uint64_t));
    }

    if (!file.good()) {
        return Result<ResumeFileData>::failure(ErrorCode::IOError, "Invalid resume file: read error");
    }

    char hash_buf[65] = {};
    file.read(hash_buf, 64);
    data.source_hash = hash_buf;

    auto read_time = [&file]() {
        int64_t ms = 0;
        file.read(reinterpret_cast<char*>(&ms), sizeof(ms));
        return std::chrono::system_clock::time_point{std::chrono::milliseconds{ms}};
    };

    data.source_create_time = read_time();
    data.source_modify_time = read_time();
    data.updated_at = read_time();

    return Result<ResumeFileData>::success(std::move(data));
}

Result<void> ResumeFileWriter::write(const std::filesystem::path& path, const ResumeFileData& data) {
    ResumeEngine engine(nullptr, path.parent_path());
    return engine.createResumeFile(data.task_id, data);
}

}