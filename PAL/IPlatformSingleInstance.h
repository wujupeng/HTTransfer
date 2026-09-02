#pragma once

#include <functional>
#include "Core/Common/Result.h"

namespace ht {

class IPlatformSingleInstance {
public:
    virtual ~IPlatformSingleInstance() = default;

    virtual bool tryAcquireLock() = 0;
    virtual void releaseLock() = 0;
    virtual bool isAcquired() const = 0;
    virtual void sendRestoreNotice() = 0;
    virtual void startListening(std::function<void()> on_restore) = 0;
    virtual bool isSupported() const = 0;
};

}