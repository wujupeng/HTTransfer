#pragma once

#include <string>

namespace ht {

class IPlugin {
public:
    virtual ~IPlugin() = default;
    virtual std::string name() const = 0;
    virtual std::string version() const = 0;
    virtual bool initialize() = 0;
    virtual void shutdown() = 0;
};

}