#pragma once

#include <chrono>

namespace ht {

constexpr uint64_t kChunkSize = HT_CHUNK_SIZE;
constexpr uint64_t kBufferPoolSize = HT_BUFFER_POOL_SIZE;
constexpr uint32_t kMaxParallelism = HT_MAX_PARALLELISM;
constexpr uint32_t kDefaultParallelism = HT_DEFAULT_PARALLELISM;
constexpr std::chrono::seconds kDefaultObservationWindow{5};
constexpr std::chrono::seconds kMaxRetryDelay{300};
constexpr std::chrono::seconds kBaseRetryDelay{5};
constexpr uint32_t kMaxChunkRetries = 3;
constexpr uint32_t kResumeMagic = 0x4854524D;
constexpr uint16_t kResumeVersion = 1;
constexpr std::chrono::seconds kProgressUpdateInterval{5};
constexpr std::chrono::minutes kMaxStabilityWait{10};

}