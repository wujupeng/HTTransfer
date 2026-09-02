#pragma once

#include "PAL/IPlatformPath.h"
#include <algorithm>
#include <cstdlib>

#ifdef __linux__
#include <sys/stat.h>
#include <mntent.h>
#endif

namespace ht {

class PosixPath : public IPlatformPath {
public:
    std::string normalize(const std::string& path) const override {
        std::string result = path;
        std::replace(result.begin(), result.end(), '\\', '/');
        return result;
    }

    bool isAbsolute(const std::string& path) const override {
        return !path.empty() && path[0] == '/';
    }

    std::string join(const std::string& base, const std::string& relative) const override {
        if (base.empty()) return relative;
        if (base.back() == '/') return base + relative;
        return base + "/" + relative;
    }

    std::string basename(const std::string& path) const override {
        auto pos = path.find_last_of('/');
        return (pos == std::string::npos) ? path : path.substr(pos + 1);
    }

    std::string dirname(const std::string& path) const override {
        auto pos = path.find_last_of('/');
        return (pos == std::string::npos) ? "" : path.substr(0, pos);
    }

    bool isSmb(const std::string& path) const override {
        if (path.substr(0, 6) == "smb://") return true;
#ifdef __linux__
        FILE* fp = setmntent("/proc/mounts", "r");
        if (fp) {
            struct mntent* ent;
            while ((ent = getmntent(fp)) != nullptr) {
                if (std::string(ent->mnt_type) == "cifs" && path.find(ent->mnt_dir) == 0) {
                    endmntent(fp);
                    return true;
                }
            }
            endmntent(fp);
        }
#endif
        return false;
    }

    std::string toNative(const std::string& path) const override {
        return normalize(path);
    }
};

}