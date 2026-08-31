#pragma once

#include <string>
#include "Core/Common/Result.h"
#include "Watch/WatchTypes.h"

namespace ht {

class IWatchSession {
public:
    virtual ~IWatchSession() = default;
    virtual Result<std::string> startWatch(const std::string& source_path,
                                            const std::string& target_path,
                                            int scan_interval_seconds) = 0;
    virtual Result<void> stopWatch() = 0;
    virtual WatchStatus getStatus() const = 0;
    virtual WatchStatistics getStatistics() const = 0;
    virtual void registerStatusCallback(WatchStatusCallback callback) = 0;
};

}