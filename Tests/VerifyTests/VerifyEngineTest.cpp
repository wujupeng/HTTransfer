#include <gtest/gtest.h>
#include "Verify/VerifyEngine.h"
#include "Core/Common/Types.h"
#include <filesystem>
#include <fstream>
#include <vector>

namespace ht {

class VerifyEngineTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_dir_ = std::filesystem::temp_directory_path() / "ht_test_verify";
        std::filesystem::create_directories(test_dir_);
    }

    void TearDown() override {
        std::error_code ec;
        std::filesystem::remove_all(test_dir_, ec);
    }

    std::filesystem::path test_dir_;
};

TEST_F(VerifyEngineTest, CRC32KnownValue) {
    const char* data = "123456789";
    uint32_t crc = CRC32Calculator::compute(reinterpret_cast<const uint8_t*>(data), 9);
    EXPECT_EQ(crc, 0xCBF43926u);
}

TEST_F(VerifyEngineTest, CRC32Streaming) {
    CRC32Calculator calc;
    const char* part1 = "1234";
    const char* part2 = "56789";
    calc.update(reinterpret_cast<const uint8_t*>(part1), 4);
    calc.update(reinterpret_cast<const uint8_t*>(part2), 5);
    uint32_t crc = calc.finalize();
    EXPECT_EQ(crc, 0xCBF43926u);
}

TEST_F(VerifyEngineTest, SHA256KnownValue) {
    const char* data = "";
    auto hash = SHA256Calculator::compute(reinterpret_cast<const uint8_t*>(data), 0);
    EXPECT_EQ(hash, "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
}

TEST_F(VerifyEngineTest, SHA256Streaming) {
    SHA256Calculator calc;
    const char* part1 = "abc";
    calc.update(reinterpret_cast<const uint8_t*>(part1), 3);
    auto hash = calc.finalize();
    EXPECT_EQ(hash, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
}

TEST_F(VerifyEngineTest, Blake3KnownValue) {
    const char* data = "";
    auto hash = Blake3Calculator::compute(reinterpret_cast<const uint8_t*>(data), 0);
    EXPECT_EQ(hash.length(), 64u);
    EXPECT_EQ(hash.substr(0, 16), "af1349b9f5f9a1a6");
}

TEST_F(VerifyEngineTest, Blake3Streaming) {
    Blake3Calculator calc;
    const char* data = "abc";
    calc.update(reinterpret_cast<const uint8_t*>(data), 3);
    auto hash = calc.finalize();
    EXPECT_EQ(hash.length(), 64u);
}

TEST_F(VerifyEngineTest, ComputeFileHashSHA256) {
    auto file_path = test_dir_ / "test_hash.bin";
    std::ofstream ofs(file_path, std::ios::binary);
    const char* data = "hello world";
    ofs.write(data, 11);
    ofs.close();

    VerifyEngine engine;
    auto result = engine.computeFileHash(file_path);
    ASSERT_TRUE(result.isOk());
    EXPECT_EQ(result.value().length(), 64u);
}

TEST_F(VerifyEngineTest, ComputeFileHashBLAKE3) {
    auto file_path = test_dir_ / "test_blake3.bin";
    std::ofstream ofs(file_path, std::ios::binary);
    const char* data = "hello world";
    ofs.write(data, 11);
    ofs.close();

    VerifyEngine engine;
    engine.setHashAlgorithm(HashAlgorithm::BLAKE3);
    auto result = engine.computeFileHash(file_path);
    ASSERT_TRUE(result.isOk());
    EXPECT_EQ(result.value().length(), 64u);
}

TEST_F(VerifyEngineTest, HashAlgorithmSwitching) {
    VerifyEngine engine;
    EXPECT_EQ(engine.getHashAlgorithm(), HashAlgorithm::SHA256);

    engine.setHashAlgorithm(HashAlgorithm::BLAKE3);
    EXPECT_EQ(engine.getHashAlgorithm(), HashAlgorithm::BLAKE3);

    engine.setHashAlgorithm(HashAlgorithm::SHA256);
    EXPECT_EQ(engine.getHashAlgorithm(), HashAlgorithm::SHA256);
}

TEST_F(VerifyEngineTest, VerifyChunkMatch) {
    ChunkInfo chunk;
    chunk.crc32 = 0xCBF43926u;

    VerifyEngine engine;
    auto result = engine.verifyChunk(chunk, 0xCBF43926u);
    ASSERT_TRUE(result.isOk());
    EXPECT_TRUE(result.value());
}

TEST_F(VerifyEngineTest, VerifyChunkMismatch) {
    ChunkInfo chunk;
    chunk.crc32 = 0xCBF43926u;

    VerifyEngine engine;
    auto result = engine.verifyChunk(chunk, 0xDEADBEEFu);
    ASSERT_TRUE(result.isOk());
    EXPECT_FALSE(result.value());
}

TEST_F(VerifyEngineTest, GenerateReportMatch) {
    VerifyEngine engine;
    auto report = engine.generateReport("task1", "hash123", "hash123", 10, 0);
    ASSERT_TRUE(report.isOk());
    EXPECT_TRUE(report.value().hash_match);
    EXPECT_EQ(report.value().result, VerifyResult::Success);
}

TEST_F(VerifyEngineTest, GenerateReportMismatch) {
    VerifyEngine engine;
    auto report = engine.generateReport("task1", "hash123", "hash456", 8, 2);
    ASSERT_TRUE(report.isOk());
    EXPECT_FALSE(report.value().hash_match);
    EXPECT_EQ(report.value().result, VerifyResult::Failed);
    EXPECT_EQ(report.value().failed_chunks, 2u);
}

}
