#pragma once

#include "PAL/IPlatformPath.h"
#include <algorithm>

namespace ht {

class Win32Path : public IPlatformPath {
public:
    std::string normalize(const std::string& path) const override {
        std::string result = path;
        std::replace(result.begin(), result.end(), '/', '\\');
        return result;
    }

    bool isAbsolute(const std::string& path) const override {
        if (path.size() >= 2 && path[1] == ':') return true;
        if (path.size() >= 2 && path[0] == '\\' && path[1] == '\\') return true;
        return false;
    }

    std::string join(const std::string& base, const std::string& relative) const override {
        if (base.empty()) return relative;
        if (base.back() == '\\') return base + relative;
        return base + "\\" + relative;
    }

    std::string basename(const std::string& path) const override {
        auto pos = path.find_last_of("\\/");
        return (pos == std::string::npos) ? path : path.substr(pos + 1);
    }

    std::string dirname(const std::string& path) const override {
        auto pos = path.find_last_of("\\/");
        return (pos == std::string::npos) ? "" : path.substr(0, pos);
    }

    bool isSmb(const std::string& path) const override {
        return path.size() >= 2 && path[0] == '\\' && path[1] == '\\';
    }

    std::string toNative(const std::string& path) const override {
        return normalize(path);
    }
};

}