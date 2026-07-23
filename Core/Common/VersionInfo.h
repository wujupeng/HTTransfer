#pragma once

#include "version.h"
#include <string>
#include <cstdint>

namespace ht {

struct VersionInfo {
    static constexpr uint32_t major = HT_VERSION_MAJOR;
    static constexpr uint32_t minor = HT_VERSION_MINOR;
    static constexpr uint32_t patch = HT_VERSION_PATCH;
    static constexpr const char* prerelease = HT_VERSION_PRERELEASE;
    static constexpr const char* version_string = HT_VERSION_STRING;
    static constexpr const char* version_full = HT_VERSION_FULL;
};

}