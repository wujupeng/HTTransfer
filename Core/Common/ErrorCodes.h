#pragma once

#include <string>
#include <array>

namespace ht {

enum class ErrorCode : uint16_t {
    SourceError = 1,
    TargetError = 2,
    StorageError = 3,
    NetworkError = 4,
    IOError = 5,
    VerifyError = 6,
    HashComputeError = 7,
    AuthError = 8,
    SourceInaccessible = 9,
    ResourceExhausted = 10,
    ConfigError = 11,
};

struct ErrorCodeInfo {
    ErrorCode code;
    const char* code_str;
    const char* description;
};

inline constexpr std::array<ErrorCodeInfo, 11> kErrorCodeTable = {{
    {ErrorCode::SourceError,       "HT-E001", "Source file is not accessible"},
    {ErrorCode::TargetError,       "HT-E002", "Target path is not writable"},
    {ErrorCode::StorageError,      "HT-E003", "Insufficient disk space on target"},
    {ErrorCode::NetworkError,      "HT-E004", "Network connection interrupted"},
    {ErrorCode::IOError,           "HT-E005", "I/O operation failed"},
    {ErrorCode::VerifyError,       "HT-E006", "SHA-256 verification failed"},
    {ErrorCode::HashComputeError,  "HT-E007", "Start hash computation failed"},
    {ErrorCode::AuthError,         "HT-E008", "FTP/SFTP authentication failed"},
    {ErrorCode::SourceInaccessible,"HT-E009", "Source file no longer accessible"},
    {ErrorCode::ResourceExhausted, "HT-E010", "Buffer pool exhausted"},
    {ErrorCode::ConfigError,       "HT-E011", "Configuration error"},
}};

inline const char* errorCodeToString(ErrorCode code) {
    for (const auto& info : kErrorCodeTable) {
        if (info.code == code) return info.code_str;
    }
    return "HT-E000";
}

inline const char* errorCodeDescription(ErrorCode code) {
    for (const auto& info : kErrorCodeTable) {
        if (info.code == code) return info.description;
    }
    return "Unknown error";
}

}