#pragma once

#include <string>
#include <filesystem>

namespace ht {

class IPlatformPath {
public:
    virtual ~IPlatformPath() = default;

    virtual std::string normalize(const std::string& path) const = 0;
    virtual bool isAbsolute(const std::string& path) const = 0;
    virtual std::string join(const std::string& base, const std::string& relative) const = 0;
    virtual std::string basename(const std::string& path) const = 0;
    virtual std::string dirname(const std::string& path) const = 0;
    virtual bool isSmb(const std::string& path) const = 0;
    virtual std::string toNative(const std::string& path) const = 0;
};

}