#pragma once

#include <cstdint>
#include <string>
#include <filesystem>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace ht {

using offset_t = uint64_t;

static_assert(sizeof(offset_t) == 8, "offset_t must be 8 bytes for large file support");

inline std::filesystem::path utf8ToPath(const std::string& utf8_str) {
#ifdef _WIN32
    if (utf8_str.empty()) return std::filesystem::path();
    int wlen = MultiByteToWideChar(CP_UTF8, 0, utf8_str.c_str(), -1, nullptr, 0);
    if (wlen <= 0) return std::filesystem::path();
    std::wstring wstr(wlen - 1, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8_str.c_str(), -1, wstr.data(), wlen);
    return std::filesystem::path(wstr);
#else
    return std::filesystem::u8path(utf8_str);
#endif
}

}