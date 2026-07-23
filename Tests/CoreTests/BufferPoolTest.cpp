#include <gtest/gtest.h>
#include "Core/BufferPool.h"
#include "Core/Common/Types.h"
#include "Core/Common/Result.h"
#include "Core/Domain/ChunkManifest.h"
#include "Core/Domain/TransferTask.h"

namespace ht {

TEST(TypesTest, OffsetTypeIs64Bit) {
    static_assert(sizeof(offset_t) == 8, "offset_t must be 8 bytes");
    EXPECT_EQ(sizeof(offset_t), 8u);
}

TEST(ResultTest, SuccessResult) {
    auto r = Result<int>::success(42);
    EXPECT_TRUE(r.isOk());
    EXPECT_FALSE(r.isErr());
    EXPECT_EQ(r.value(), 42);
}

TEST(ResultTest, FailureResult) {
    auto r = Result<int>::failure(ErrorCode::SourceError, "test error");
    EXPECT_FALSE(r.isOk());
    EXPECT_TRUE(r.isErr());
    EXPECT_EQ(r.errorMessage(), "test error");
}

TEST(ResultTest, VoidResult) {
    auto r = Result<void>::success();
    EXPECT_TRUE(r.isOk());
    auto f = Result<void>::failure(ErrorCode::IOError, "fail");
    EXPECT_TRUE(f.isErr());
}

TEST(ChunkManifestTest, CreateManifest200GB) {
    uint64_t file_size = 200ULL * 1024 * 1024 * 1024;
    auto manifest = createChunkManifest("test-task", file_size);
    EXPECT_EQ(manifest.chunk_size, kChunkSize);
    EXPECT_GT(manifest.total_chunks, 0u);
    uint64_t total = 0;
    for (const auto& c : manifest.chunks) total += c.size;
    EXPECT_EQ(total, file_size);
}

TEST(ChunkManifestTest, LastChunkSmaller) {
    uint64_t file_size = kChunkSize + 1;
    auto manifest = createChunkManifest("test-task", file_size);
    EXPECT_EQ(manifest.total_chunks, 2u);
    EXPECT_EQ(manifest.chunks[0].size, kChunkSize);
    EXPECT_EQ(manifest.chunks[1].size, 1u);
}

TEST(BufferPoolTest, AcquireAndRelease) {
    constexpr uint64_t test_seg_size = 4096;
    BufferPool pool(test_seg_size * 4, test_seg_size);
    EXPECT_EQ(pool.availableCount(), 4u);
    auto seg1 = pool.acquire();
    EXPECT_TRUE(seg1.isOk());
    EXPECT_EQ(pool.availableCount(), 3u);
    pool.release(std::move(seg1.value()));
    EXPECT_EQ(pool.availableCount(), 4u);
}

TEST(TransferTaskTest, GenerateTaskId) {
    auto id1 = generateTaskId();
    auto id2 = generateTaskId();
    EXPECT_NE(id1, id2);
    EXPECT_TRUE(id1.find("HT-") == 0);
}

TEST(TransferTaskTest, DetectProtocol) {
    EXPECT_EQ(detectProtocol("sftp://server/path"), ProtocolType::SFTP);
    EXPECT_EQ(detectProtocol("ftp://server/path"), ProtocolType::FTP);
    EXPECT_EQ(detectProtocol("http://server/path"), ProtocolType::HTTPS);
    EXPECT_EQ(detectProtocol("https://server/path"), ProtocolType::HTTPS);
    EXPECT_EQ(detectProtocol("\\\\server\\share"), ProtocolType::SMB);
    EXPECT_EQ(detectProtocol("D:\\path"), ProtocolType::SMB);
}

}
