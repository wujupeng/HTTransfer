#pragma once

#include <string>
#include "Core/Common/Result.h"

namespace ht {

class AutoStartManager {
public:
    Result<void> enable() {
#ifdef _WIN32
        HKEY hKey;
        if (RegOpenKeyExW(HKEY_CURRENT_USER,
            L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
            0, KEY_SET_VALUE, &hKey) != ERROR_SUCCESS) {
            return Result<void>::failure(ErrorCode::ConfigError, "Cannot open registry Run key");
        }

        wchar_t exe_path[MAX_PATH];
        if (GetModuleFileNameW(nullptr, exe_path, MAX_PATH) == 0) {
            RegCloseKey(hKey);
            return Result<void>::failure(ErrorCode::ConfigError, "Cannot get module path");
        }

        std::wstring value = std::wstring(exe_path) + L" --minimized";
        if (RegSetValueExW(hKey, L"HTTransfer", 0, REG_SZ,
            reinterpret_cast<const BYTE*>(value.c_str()),
            static_cast<DWORD>((value.size() + 1) * sizeof(wchar_t))) != ERROR_SUCCESS) {
            RegCloseKey(hKey);
            return Result<void>::failure(ErrorCode::ConfigError, "Cannot set registry value");
        }

        RegCloseKey(hKey);
        return Result<void>::success();
#else
        return Result<void>::failure(ErrorCode::ConfigError, "AutoStart not supported on this platform");
#endif
    }

    Result<void> disable() {
#ifdef _WIN32
        HKEY hKey;
        if (RegOpenKeyExW(HKEY_CURRENT_USER,
            L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
            0, KEY_SET_VALUE, &hKey) != ERROR_SUCCESS) {
            return Result<void>::failure(ErrorCode::ConfigError, "Cannot open registry Run key");
        }

        RegDeleteValueW(hKey, L"HTTransfer");
        RegCloseKey(hKey);
        return Result<void>::success();
#else
        return Result<void>::success();
#endif
    }

    bool isEnabled() const {
#ifdef _WIN32
        HKEY hKey;
        if (RegOpenKeyExW(HKEY_CURRENT_USER,
            L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
            0, KEY_READ, &hKey) != ERROR_SUCCESS) {
            return false;
        }

        DWORD data_size = 0;
        bool exists = RegQueryValueExW(hKey, L"HTTransfer", nullptr, nullptr, nullptr, &data_size) == ERROR_SUCCESS;
        RegCloseKey(hKey);
        return exists;
#else
        return false;
#endif
    }
};

}