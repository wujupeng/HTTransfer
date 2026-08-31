#pragma once

#include <string>
#include <memory>
#include "Watch/IFileWatcher.h"
#include "Watch/WatchTypes.h"
#include "Core/FileEngine.h"
#include "Logger/ILogger.h"

namespace ht {

class FileWatcher : public IFileWatcher {
public:
    FileWatcher(const std::string& source_path,
                std::shared_ptr<IFileEngine> file_engine,
                std::shared_ptr<ILogger> logger);

    std::vector<FileChangeEvent> scanAndDetect() override;
    const FileSnapshot& getSnapshot() const override;
    void resetSnapshot() override;

    FileWatcher(const FileWatcher&) = delete;
    FileWatcher& operator=(const FileWatcher&) = delete;

private:
    FileSnapshot buildSnapshot(const std::vector<FileEntry>& entries);
    std::vector<FileChangeEvent> detectChanges(const FileSnapshot& current,
                                                const FileSnapshot& last);

    std::string source_path_;
    FileSnapshot last_snapshot_;
    std::shared_ptr<IFileEngine> file_engine_;
    std::shared_ptr<ILogger> logger_;
    bool is_first_scan_;
};

}