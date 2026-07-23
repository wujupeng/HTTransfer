#include "VerifyEngine.h"
#include <fstream>
#include <openssl/evp.h>

namespace ht {

uint32_t CRC32Calculator::table_[256] = {};
bool CRC32Calculator::table_initialized_ = false;

void CRC32Calculator::initTable() {
    if (table_initialized_) return;
    for (uint32_t i = 0; i < 256; ++i) {
        uint32_t crc = i;
        for (int j = 0; j < 8; ++j) {
            crc = (crc >> 1) ^ (crc & 1 ? 0xEDB88320 : 0);
        }
        table_[i] = crc;
    }
    table_initialized_ = true;
}

CRC32Calculator::CRC32Calculator() : crc_(0xFFFFFFFF) { initTable(); }

void CRC32Calculator::update(const uint8_t* data, size_t length) {
    for (size_t i = 0; i < length; ++i) {
        crc_ = (crc_ >> 8) ^ table_[(crc_ ^ data[i]) & 0xFF];
    }
}

uint32_t CRC32Calculator::finalize() {
    uint32_t result = crc_ ^ 0xFFFFFFFF;
    crc_ = 0xFFFFFFFF;
    return result;
}

uint32_t CRC32Calculator::compute(const uint8_t* data, size_t length) {
    CRC32Calculator calc;
    calc.update(data, length);
    return calc.finalize();
}


struct SHA256Calculator::Context {
    EVP_MD_CTX* md_ctx = nullptr;
};

SHA256Calculator::SHA256Calculator() : ctx_(std::make_unique<Context>()) {
    ctx_->md_ctx = EVP_MD_CTX_new();
    EVP_DigestInit_ex(ctx_->md_ctx, EVP_sha256(), nullptr);
}

SHA256Calculator::~SHA256Calculator() {
    if (ctx_ && ctx_->md_ctx) {
        EVP_MD_CTX_free(ctx_->md_ctx);
    }
}

void SHA256Calculator::update(const uint8_t* data, size_t length) {
    EVP_DigestUpdate(ctx_->md_ctx, data, length);
}

std::string SHA256Calculator::finalize() {
    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int hash_len = 0;
    EVP_DigestFinal_ex(ctx_->md_ctx, hash, &hash_len);

    std::string result;
    result.reserve(hash_len * 2);
    static const char hex[] = "0123456789abcdef";
    for (unsigned int i = 0; i < hash_len; ++i) {
        result += hex[hash[i] >> 4];
        result += hex[hash[i] & 0x0F];
    }

    EVP_DigestInit_ex(ctx_->md_ctx, EVP_sha256(), nullptr);
    return result;
}

std::string SHA256Calculator::compute(const uint8_t* data, size_t length) {
    SHA256Calculator calc;
    calc.update(data, length);
    return calc.finalize();
}


uint32_t VerifyEngine::computeCRC32(const uint8_t* data, size_t length) {
    return CRC32Calculator::compute(data, length);
}

Result<bool> VerifyEngine::verifyChunk(const ChunkInfo& chunk, uint32_t computed_crc32) {
    return Result<bool>::success(chunk.crc32 == computed_crc32);
}

Result<IntegrityReport> VerifyEngine::generateReport(const std::string& task_id,
                                                       const std::string& start_hash,
                                                       const std::string& end_hash,
                                                       uint64_t verified_chunks,
                                                       uint64_t failed_chunks) {
    IntegrityReport report;
    report.task_id = task_id;
    report.start_hash = start_hash;
    report.end_hash = end_hash;
    report.hash_match = (start_hash == end_hash);
    report.result = report.hash_match ? VerifyResult::Success : VerifyResult::Failed;
    report.verified_chunks = verified_chunks;
    report.failed_chunks = failed_chunks;
    report.verified_at = std::chrono::system_clock::now();
    return Result<IntegrityReport>::success(std::move(report));
}

Result<std::string> VerifyEngine::computeFileHash(const std::filesystem::path& file_path) {
    std::ifstream file(file_path, std::ios::binary);
    if (!file) {
        return Result<std::string>::failure(ErrorCode::SourceError, "Cannot open file for hashing");
    }

    SHA256Calculator sha256;
    std::vector<uint8_t> buffer(1024 * 1024);
    while (file) {
        file.read(reinterpret_cast<char*>(buffer.data()), buffer.size());
        auto count = static_cast<size_t>(file.gcount());
        if (count > 0) {
            sha256.update(buffer.data(), count);
        }
    }

    return Result<std::string>::success(sha256.finalize());
}

}