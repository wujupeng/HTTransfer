#pragma once

#include <cstdint>
#include <string>
#include <memory>
#include <filesystem>
#include "Core/Common/Result.h"
#include "Core/Domain/ChunkManifest.h"
#include "Core/Domain/IntegrityReport.h"
#include "Verify/CRC32Calculator.h"
#include "Verify/SHA256Calculator.h"
#include "Verify/Blake3Calculator.h"

namespace ht {

enum class HashAlgorithm : uint8_t {
    SHA256,
    BLAKE3
};

class IVerifyEngine {
public:
    virtual ~IVerifyEngine() = default;
    virtual uint32_t computeCRC32(const uint8_t* data, size_t length) = 0;
    virtual Result<bool> verifyChunk(const ChunkInfo& chunk, uint32_t computed_crc32) = 0;
    virtual Result<IntegrityReport> generateReport(const std::string& task_id,
                                                    const std::string& start_hash,
                                                    const std::string& end_hash,
                                                    uint64_t verified_chunks,
                                                    uint64_t failed_chunks) = 0;
    virtual Result<std::string> computeFileHash(const std::filesystem::path& file_path) = 0;
    virtual void setHashAlgorithm(HashAlgorithm algo) = 0;
    virtual HashAlgorithm getHashAlgorithm() const = 0;
};

class VerifyEngine : public IVerifyEngine {
public:
    VerifyEngine() = default;

    uint32_t computeCRC32(const uint8_t* data, size_t length) override;
    Result<bool> verifyChunk(const ChunkInfo& chunk, uint32_t computed_crc32) override;
    Result<IntegrityReport> generateReport(const std::string& task_id,
                                            const std::string& start_hash,
                                            const std::string& end_hash,
                                            uint64_t verified_chunks,
                                            uint64_t failed_chunks) override;
    Result<std::string> computeFileHash(const std::filesystem::path& file_path) override;
    void setHashAlgorithm(HashAlgorithm algo) override;
    HashAlgorithm getHashAlgorithm() const override;

private:
    HashAlgorithm hash_algo_ = HashAlgorithm::SHA256;
};

}