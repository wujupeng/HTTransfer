#include <gtest/gtest.h>
#include "Transfer/TransferEngine.h"
#include "Transfer/ConcurrentQueue.h"
#include "Transfer/ReaderPool.h"
#include "Transfer/WriterThread.h"
#include "Core/LocalFileSource.h"
#include "Core/LocalFileSink.h"
#include "Core/Domain/ChunkManifest.h"
#include "Core/Common/Types.h"
#include "Core/Common/Constants.h"
#include "Resume/ResumeEngine.h"
#include "Logger/Logger.h"
#include <filesystem>
#include <fstream>
#include <vector>
#include <thread>
#include <chrono>

namespace ht {

class ReaderWriterTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_dir_ = std::filesystem::temp_directory_path() / "ht_test_rw";
        std::filesystem::create_directories(test_dir_);
    }

    void TearDown() override {
        std::error_code ec;
        std::filesystem::remove_all(test_dir_, ec);
    }

    std::filesystem::path test_dir_;
};

static std::vector<uint8_t> createTestFile(const std::filesystem::path& path, size_t size) {
    std::vector<uint8_t> data(size);
    for (size_t i = 0; i < size; ++i) {
        data[i] = static_cast<uint8_t>(i % 256);
    }
    std::ofstream ofs(path, std::ios::binary);
    ofs.write(reinterpret_cast<const char*>(data.data()), size);
    ofs.close();
    return data;
}

TEST_F(ReaderWriterTest, SingleChunkTransfer) {
    auto src_path = test_dir_ / "source.bin";
    auto dst_path = test_dir_ / "dest.bin";
    size_t file_size = 4096;

    auto src_data = createTestFile(src_path, file_size);

    auto logger = std::make_shared<Logger>();
    auto ctrl = std::make_shared<TaskControl>();

    ChunkManifest manifest;
    manifest.total_chunks = 1;
    ChunkInfo chunk;
    chunk.chunk_index = 0;
    chunk.offset = 0;
    chunk.size = file_size;
    chunk.status = ChunkStatus::Pending;
    manifest.chunks.push_back(chunk);

    ConcurrentQueue queue(4);
    queue.setActiveReaders(1);

    ReaderPool reader_pool(pathToUtf8(src_path), manifest, queue, logger, 1, ctrl);
    LocalFileSink sink;
    auto sink_result = sink.Open(pathToUtf8(dst_path), file_size);
    ASSERT_TRUE(sink_result.isOk());

    auto resume_engine = std::make_shared<ResumeEngine>(logger, test_dir_ / ".htresume");

    WriterThread writer_thread(&sink, queue, resume_engine,
        "test-task", file_size, logger, ctrl,
        nullptr, nullptr);

    reader_pool.start();
    writer_thread.start();

    reader_pool.join();
    writer_thread.join();

    sink.Flush();
    sink.Close();

    EXPECT_FALSE(writer_thread.hasError());

    std::ifstream ifs(dst_path, std::ios::binary);
    ASSERT_TRUE(ifs.is_open());
    std::vector<uint8_t> dst_data(file_size);
    ifs.read(reinterpret_cast<char*>(dst_data.data()), file_size);
    ifs.close();

    EXPECT_EQ(src_data, dst_data);
}

TEST_F(ReaderWriterTest, MultiChunkTransfer) {
    auto src_path = test_dir_ / "source_multi.bin";
    auto dst_path = test_dir_ / "dest_multi.bin";
    size_t file_size = kChunkSize + kChunkSize / 2;

    auto src_data = createTestFile(src_path, file_size);

    auto logger = std::make_shared<Logger>();
    auto ctrl = std::make_shared<TaskControl>();

    ChunkManifest manifest;
    manifest.total_chunks = 2;
    ChunkInfo chunk0;
    chunk0.chunk_index = 0;
    chunk0.offset = 0;
    chunk0.size = kChunkSize;
    chunk0.status = ChunkStatus::Pending;
    manifest.chunks.push_back(chunk0);

    ChunkInfo chunk1;
    chunk1.chunk_index = 1;
    chunk1.offset = kChunkSize;
    chunk1.size = file_size - kChunkSize;
    chunk1.status = ChunkStatus::Pending;
    manifest.chunks.push_back(chunk1);

    ConcurrentQueue queue(4);
    queue.setActiveReaders(2);

    ReaderPool reader_pool(pathToUtf8(src_path), manifest, queue, logger, 2, ctrl);
    LocalFileSink sink;
    auto sink_result = sink.Open(pathToUtf8(dst_path), file_size);
    ASSERT_TRUE(sink_result.isOk());

    auto resume_engine = std::make_shared<ResumeEngine>(logger, test_dir_ / ".htresume");

    WriterThread writer_thread(&sink, queue, resume_engine,
        "test-multi", file_size, logger, ctrl,
        nullptr, nullptr);

    reader_pool.start();
    writer_thread.start();

    reader_pool.join();
    writer_thread.join();

    sink.Flush();
    sink.Close();

    EXPECT_FALSE(writer_thread.hasError());

    std::ifstream ifs(dst_path, std::ios::binary);
    ASSERT_TRUE(ifs.is_open());
    std::vector<uint8_t> dst_data(file_size);
    ifs.read(reinterpret_cast<char*>(dst_data.data()), file_size);
    ifs.close();

    EXPECT_EQ(src_data, dst_data);
}

TEST_F(ReaderWriterTest, CancelStopsTransfer) {
    auto src_path = test_dir_ / "source_cancel.bin";
    auto dst_path = test_dir_ / "dest_cancel.bin";
    size_t file_size = kChunkSize * 4;

    createTestFile(src_path, file_size);

    auto logger = std::make_shared<Logger>();
    auto ctrl = std::make_shared<TaskControl>();

    ChunkManifest manifest;
    manifest.total_chunks = 4;
    for (uint64_t i = 0; i < 4; ++i) {
        ChunkInfo c;
        c.chunk_index = i;
        c.offset = i * kChunkSize;
        c.size = kChunkSize;
        c.status = ChunkStatus::Pending;
        manifest.chunks.push_back(c);
    }

    ConcurrentQueue queue(4);
    queue.setActiveReaders(2);

    ReaderPool reader_pool(pathToUtf8(src_path), manifest, queue, logger, 2, ctrl);
    LocalFileSink sink;
    auto sink_result = sink.Open(pathToUtf8(dst_path), file_size);
    ASSERT_TRUE(sink_result.isOk());

    auto resume_engine = std::make_shared<ResumeEngine>(logger, test_dir_ / ".htresume");

    WriterThread writer_thread(&sink, queue, resume_engine,
        "test-cancel", file_size, logger, ctrl,
        nullptr, nullptr);

    reader_pool.start();
    writer_thread.start();

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    ctrl->cancelled.store(true);

    reader_pool.join();
    writer_thread.join();

    sink.Flush();
    sink.Close();
}

TEST_F(ReaderWriterTest, WriterErrorPropagation) {
    auto src_path = test_dir_ / "source_werr.bin";
    size_t file_size = 4096;

    createTestFile(src_path, file_size);

    auto logger = std::make_shared<Logger>();
    auto ctrl = std::make_shared<TaskControl>();

    ChunkManifest manifest;
    manifest.total_chunks = 2;
    for (uint64_t i = 0; i < 2; ++i) {
        ChunkInfo c;
        c.chunk_index = i;
        c.offset = i * 2048;
        c.size = 2048;
        c.status = ChunkStatus::Pending;
        manifest.chunks.push_back(c);
    }

    ConcurrentQueue queue(4);
    queue.setActiveReaders(1);

    queue.signalWriterError();

    ReaderPool reader_pool(pathToUtf8(src_path), manifest, queue, logger, 1, ctrl);

    reader_pool.start();
    reader_pool.join();
}

}