#pragma once

#include <vector>
#include "Watch/WatchTypes.h"

namespace ht {

class IFileWatcher {
public:
    virtual ~IFileWatcher() = default;
    virtual std::vector<FileChangeEvent> scanAndDetect() = 0;
    virtual const FileSnapshot& getSnapshot() const = 0;
    virtual void resetSnapshot() = 0;
};

}