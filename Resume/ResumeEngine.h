#pragma once

#include <string>
#include <vector>
#include <optional>
#include <filesystem>
#include <memory>
#include "Core/Common/Result.h"
#include "Core/Domain/ResumeFile.h"
#include "Logger/ILogger.h"

namespace ht {

class IResumeEngine {
public:
    virtual ~IResumeEngine() = default;
    virtual Result<void> createResumeFile(const std::string& task_id, const ResumeFileData& data) = 0;
    virtual Result<std::optional<ResumeFileData>> loadResumeFile(const std::string& task_id) = 0;
    virtual Result<void> updateResumeFile(const std::string& task_id, const ResumeFileData& data) = 0;
    virtual Result<void> markChunkCompleted(const std::string& task_id, uint64_t chunk_index, uint64_t offset) = 0;
    virtual Result<bool> isSourceFileChanged(const std::string& task_id, const std::filesystem::path& source_path) = 0;
    virtual Result<void> invalidateResumeFile(const std::string& task_id) = 0;
    virtual Result<std::vector<std::string>> scanUnfinishedTasks() = 0;
};

class ResumeFileParser {
public:
    static Result<ResumeFileData> parse(const std::filesystem::path& path);
};

class ResumeFileWriter {
public:
    static Result<void> write(const std::filesystem::path& path, const ResumeFileData& data);
};

class ResumeEngine : public IResumeEngine {
public:
    explicit ResumeEngine(std::shared_ptr<ILogger> logger,
                          const std::filesystem::path& resume_dir = ".htresume");

    Result<void> createResumeFile(const std::string& task_id, const ResumeFileData& data) override;
    Result<std::optional<ResumeFileData>> loadResumeFile(const std::string& task_id) override;
    Result<void> updateResumeFile(const std::string& task_id, const ResumeFileData& data) override;
    Result<void> markChunkCompleted(const std::string& task_id, uint64_t chunk_index, uint64_t offset) override;
    Result<bool> isSourceFileChanged(const std::string& task_id, const std::filesystem::path& source_path) override;
    Result<void> invalidateResumeFile(const std::string& task_id) override;
    Result<std::vector<std::string>> scanUnfinishedTasks() override;

private:
    std::filesystem::path getResumePath(const std::string& task_id) const;
    bool atomicWrite(const std::filesystem::path& target_path, const std::vector<uint8_t>& data);

    std::shared_ptr<ILogger> logger_;
    std::filesystem::path resume_dir_;
    std::unordered_map<std::string, ResumeFileData> cache_;
};

}