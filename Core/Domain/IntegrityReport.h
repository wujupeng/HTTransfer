#pragma once

#include <string>
#include <chrono>
#include <cstdint>

namespace ht {

enum class VerifyResult : uint8_t {
    Success,
    Failed
};

struct IntegrityReport {
    std::string task_id;
    std::string start_hash;
    std::string end_hash;
    bool hash_match = false;
    VerifyResult result = VerifyResult::Failed;
    uint64_t verified_chunks = 0;
    uint64_t failed_chunks = 0;
    std::chrono::system_clock::time_point verified_at;
};

}