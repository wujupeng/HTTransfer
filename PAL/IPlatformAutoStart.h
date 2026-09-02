#pragma once

#include "Core/Common/Result.h"

namespace ht {

class IPlatformAutoStart {
public:
    virtual ~IPlatformAutoStart() = default;

    virtual Result<void> enable() = 0;
    virtual Result<void> disable() = 0;
    virtual bool isEnabled() const = 0;
    virtual bool isSupported() const = 0;
};

}