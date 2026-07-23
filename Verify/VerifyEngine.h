#pragma once

#include <cstdint>
#include <string>
#include <memory>
#include "Core/Common/Result.h"
#include "Core/Domain/ChunkManifest.h"
#include "Core/Domain/IntegrityReport.h"

namespace ht {

class CRC32Calculator {
public:
    CRC32Calculator();

    void update(const uint8_t* data, size_t length);
    uint32_t finalize();

    static uint32_t compute(const uint8_t* data, size_t length);

private:
    uint32_t crc_;
    static uint32_t table_[256];
    static bool table_initialized_;
    static void initTable();
};

class SHA256Calculator {
public:
    SHA256Calculator();
    ~SHA256Calculator();

    SHA256Calculator(const SHA256Calculator&) = delete;
    SHA256Calculator& operator=(const SHA256Calculator&) = delete;

    void update(const uint8_t* data, size_t length);
    std::string finalize();

    static std::string compute(const uint8_t* data, size_t length);

private:
    struct Context;
    std::unique_ptr<Context> ctx_;
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
};

}