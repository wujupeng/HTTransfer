#include <gtest/gtest.h>
#include "Transfer/DataChunk.h"
#include <cstdint>
#include <memory>

namespace ht {

TEST(DataChunkTest, DefaultConstructor) {
    DataChunk chunk;
    EXPECT_EQ(chunk.chunk_index, 0u);
    EXPECT_EQ(chunk.offset, 0u);
    EXPECT_EQ(chunk.size, 0u);
    EXPECT_EQ(chunk.buffer, nullptr);
}

TEST(DataChunkTest, ParameterizedConstructor) {
    auto buf = std::make_unique<uint8_t[]>(16);
    memset(buf.get(), 0xAB, 16);
    DataChunk chunk(42, 1024, 16, std::move(buf));
    EXPECT_EQ(chunk.chunk_index, 42u);
    EXPECT_EQ(chunk.offset, 1024u);
    EXPECT_EQ(chunk.size, 16u);
    ASSERT_NE(chunk.buffer, nullptr);
    EXPECT_EQ(chunk.buffer[0], 0xAB);
    EXPECT_EQ(chunk.buffer[15], 0xAB);
}

TEST(DataChunkTest, MoveConstructor) {
    auto buf = std::make_unique<uint8_t[]>(8);
    buf[0] = 0x42;
    DataChunk original(1, 100, 8, std::move(buf));

    DataChunk moved(std::move(original));
    EXPECT_EQ(moved.chunk_index, 1u);
    EXPECT_EQ(moved.offset, 100u);
    EXPECT_EQ(moved.size, 8u);
    ASSERT_NE(moved.buffer, nullptr);
    EXPECT_EQ(moved.buffer[0], 0x42);
    EXPECT_EQ(original.buffer, nullptr);
}

TEST(DataChunkTest, MoveAssignment) {
    auto buf1 = std::make_unique<uint8_t[]>(4);
    buf1[0] = 0x11;
    DataChunk a(1, 10, 4, std::move(buf1));

    auto buf2 = std::make_unique<uint8_t[]>(8);
    buf2[0] = 0x22;
    DataChunk b(2, 20, 8, std::move(buf2));

    a = std::move(b);
    EXPECT_EQ(a.chunk_index, 2u);
    EXPECT_EQ(a.offset, 20u);
    EXPECT_EQ(a.size, 8u);
    ASSERT_NE(a.buffer, nullptr);
    EXPECT_EQ(a.buffer[0], 0x22);
    EXPECT_EQ(b.buffer, nullptr);
}

TEST(DataChunkTest, BufferOwnershipTransfer) {
    uint8_t* raw_ptr = nullptr;
    {
        auto buf = std::make_unique<uint8_t[]>(32);
        raw_ptr = buf.get();
        DataChunk chunk(0, 0, 32, std::move(buf));
        EXPECT_EQ(chunk.buffer.get(), raw_ptr);
        EXPECT_EQ(buf, nullptr);
    }
}

}