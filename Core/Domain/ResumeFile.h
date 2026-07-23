#pragma once

#include <string>
#include <vector>
#include <chrono>
#include <cstdint>
#include "Core/Common/Types.h"
#include "Core/Common/Constants.h"

namespace ht {

struct ResumeFileData {
    std::string task_id;
    uint64_t file_size = 0;
    uint64_t current_offset = 0;
    std::vector<uint64_t> completed_chunks;
    std::string source_hash;
    std::chrono::system_clock::time_point source_create_time;
    std::chrono::system_clock::time_point source_modify_time;
    std::chrono::system_clock::time_point updated_at;
};

struct ResumeFileHeader {
    uint32_t magic = kResumeMagic;
    uint16_t version = kResumeVersion;
    uint16_t flags = 0;
};

}