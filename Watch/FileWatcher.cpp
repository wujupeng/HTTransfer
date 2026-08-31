#include "FileWatcher.h"
#include "Core/Common/Types.h"
#include <filesystem>
#include <format>

namespace ht {

FileWatcher::FileWatcher(const std::string& source_path,
                          std::shared_ptr<IFileEngine> file_engine,
                          std::shared_ptr<ILogger> logger)
    : source_path_(source_path),
      file_engine_(std::move(file_engine)),
      logger_(std::move(logger)),
      is_first_scan_(true) {}

FileSnapshot FileWatcher::buildSnapshot(const std::vector<FileEntry>& entries) {
    FileSnapshot snapshot;
    auto src_base = utf8ToPath(source_path_);
    for (const auto& entry : entries) {
        std::string rel_path = pathToUtf8(entry.relative_path);
        std::error_code ec;
        auto fsize = std::filesystem::file_size(entry.source_path, ec);
        if (ec) continue;
        auto mtime = std::filesystem::last_write_time(entry.source_path, ec);
        if (ec) continue;
        snapshot[rel_path] = FileMeta{fsize, mtime};
    }
    return snapshot;
}

std::vector<FileChangeEvent> FileWatcher::detectChanges(const FileSnapshot& current,
                                                          const FileSnapshot& last) {
    std::vector<FileChangeEvent> events;
    auto now = std::chrono::system_clock::now();

    for (const auto& [path, meta] : current) {
        auto it = last.find(path);
        if (it == last.end()) {
            events.push_back(FileChangeEvent{
                path, ChangeType::Created, now,
                meta.size, meta.modify_time, BackupStatus::Pending
            });
        } else if (it->second.modify_time != meta.modify_time || it->second.size != meta.size) {
            events.push_back(FileChangeEvent{
                path, ChangeType::Modified, now,
                meta.size, meta.modify_time, BackupStatus::Pending
            });
        }
    }
    return events;
}

std::vector<FileChangeEvent> FileWatcher::scanAndDetect() {
    auto src_path = utf8ToPath(source_path_);
    auto scan_result = file_engine_->scanDirectory(src_path);
    if (scan_result.isErr()) {
        if (logger_) logger_->log(ILogger::Level::Error, "WATCH",
            std::format("FileWatcher scan failed: {}", scan_result.errorMessage()));
        return {};
    }

    auto current_snapshot = buildSnapshot(scan_result.value());

    if (is_first_scan_) {
        last_snapshot_ = current_snapshot;
        is_first_scan_ = false;
        if (logger_) logger_->log(ILogger::Level::Info, "WATCH",
            std::format("First scan baseline: {} files", last_snapshot_.size()));
        return {};
    }

    auto events = detectChanges(current_snapshot, last_snapshot_);
    last_snapshot_ = current_snapshot;
    return events;
}

const FileSnapshot& FileWatcher::getSnapshot() const {
    return last_snapshot_;
}

void FileWatcher::resetSnapshot() {
    last_snapshot_.clear();
    is_first_scan_ = true;
}

}