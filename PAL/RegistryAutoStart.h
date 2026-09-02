#pragma once

#include "PAL/IPlatformAutoStart.h"
#include "Core/Common/Result.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace ht {

class RegistryAutoStart : public IPlatformAutoStart {
public:
    Result<void> enable() override {
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
    }

    Result<void> disable() override {
        HKEY hKey;
        if (RegOpenKeyExW(HKEY_CURRENT_USER,
            L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
            0, KEY_SET_VALUE, &hKey) != ERROR_SUCCESS) {
            return Result<void>::failure(ErrorCode::ConfigError, "Cannot open registry Run key");
        }
        RegDeleteValueW(hKey, L"HTTransfer");
        RegCloseKey(hKey);
        return Result<void>::success();
    }

    bool isEnabled() const override {
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
    }

    bool isSupported() const override { return true; }
};

}

#endif