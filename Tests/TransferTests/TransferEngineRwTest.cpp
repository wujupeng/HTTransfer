#include <gtest/gtest.h>
#include "Transfer/TransferEngine.h"
#include "Core/LocalFileSource.h"
#include "Core/LocalFileSink.h"
#include "Core/Domain/ChunkManifest.h"
#include "Core/Domain/TransferTask.h"
#include "Core/Common/Types.h"
#include "Core/Common/Constants.h"
#include "Core/FileEngine.h"
#include "Core/BufferPool.h"
#include "Core/IOCDispatcher.h"
#include "Resume/ResumeEngine.h"
#include "Logger/Logger.h"
#include <filesystem>
#include <fstream>
#include <vector>

namespace ht {

class TransferEngineRwTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_dir_ = std::filesystem::temp_directory_path() / "ht_test_engine_rw";
        std::filesystem::create_directories(test_dir_);
        logger_ = std::make_shared<Logger>();
        buffer_pool_ = std::make_shared<BufferPool>();
        resume_engine_ = std::make_shared<ResumeEngine>(logger_, test_dir_ / ".htresume");
        transfer_engine_ = std::make_shared<TransferEngine>(logger_, buffer_pool_, resume_engine_);
    }

    void TearDown() override {
        transfer_engine_.reset();
        std::error_code ec;
        std::filesystem::remove_all(test_dir_, ec);
    }

    std::vector<uint8_t> createTestFile(const std::filesystem::path& path, size_t size) {
        std::vector<uint8_t> data(size);
        for (size_t i = 0; i < size; ++i) {
            data[i] = static_cast<uint8_t>(i % 256);
        }
        std::ofstream ofs(path, std::ios::binary);
        ofs.write(reinterpret_cast<const char*>(data.data()), size);
        ofs.close();
        return data;
    }

    std::filesystem::path test_dir_;
    std::shared_ptr<Logger> logger_;
    std::shared_ptr<BufferPool> buffer_pool_;
    std::shared_ptr<ResumeEngine> resume_engine_;
    std::shared_ptr<TransferEngine> transfer_engine_;
};

TEST_F(TransferEngineRwTest, SmallFileTransfer) {
    auto src_path = test_dir_ / "small_src.bin";
    auto dst_path = test_dir_ / "small_dst.bin";
    size_t file_size = 8192;

    auto src_data = createTestFile(src_path, file_size);

    TransferTask task;
    task.task_id = "test-small";
    task.source_path = pathToUtf8(src_path);
    task.target_path = pathToUtf8(dst_path);
    task.total_bytes = file_size;

    auto iocp = std::make_shared<IOCDispatcher>();
    auto verify_engine = std::make_shared<VerifyEngine>();
    auto file_engine = std::make_shared<FileEngine>(buffer_pool_, iocp, verify_engine);

    auto manifest_result = file_engine->createChunkManifest(task.task_id, src_path);
    ASSERT_TRUE(manifest_result.isOk());

    file_engine->preallocateFile(dst_path, file_size);

    auto result = transfer_engine_->startTransfer(task, manifest_result.value());
    ASSERT_TRUE(result.isOk()) << "Transfer failed: " << result.errorMessage();

    std::ifstream ifs(dst_path, std::ios::binary);
    ASSERT_TRUE(ifs.is_open());
    std::vector<uint8_t> dst_data(file_size);
    ifs.read(reinterpret_cast<char*>(dst_data.data()), file_size);
    ifs.close();

    EXPECT_EQ(src_data, dst_data);
}

TEST_F(TransferEngineRwTest, MultiChunkFileTransfer) {
    auto src_path = test_dir_ / "multi_src.bin";
    auto dst_path = test_dir_ / "multi_dst.bin";
    size_t file_size = kChunkSize + kChunkSize / 2;

    auto src_data = createTestFile(src_path, file_size);

    TransferTask task;
    task.task_id = "test-multi";
    task.source_path = pathToUtf8(src_path);
    task.target_path = pathToUtf8(dst_path);
    task.total_bytes = file_size;

    auto iocp = std::make_shared<IOCDispatcher>();
    auto verify_engine = std::make_shared<VerifyEngine>();
    auto file_engine = std::make_shared<FileEngine>(buffer_pool_, iocp, verify_engine);

    auto manifest_result = file_engine->createChunkManifest(task.task_id, src_path);
    ASSERT_TRUE(manifest_result.isOk());

    file_engine->preallocateFile(dst_path, file_size);

    auto result = transfer_engine_->startTransfer(task, manifest_result.value());
    ASSERT_TRUE(result.isOk()) << "Transfer failed: " << result.errorMessage();

    std::ifstream ifs(dst_path, std::ios::binary);
    ASSERT_TRUE(ifs.is_open());
    std::vector<uint8_t> dst_data(file_size);
    ifs.read(reinterpret_cast<char*>(dst_data.data()), file_size);
    ifs.close();

    EXPECT_EQ(src_data, dst_data);
}

TEST_F(TransferEngineRwTest, NonexistentSourceFails) {
    auto src_path = test_dir_ / "nonexistent.bin";
    auto dst_path = test_dir_ / "dest.bin";

    TransferTask task;
    task.task_id = "test-fail";
    task.source_path = pathToUtf8(src_path);
    task.target_path = pathToUtf8(dst_path);
    task.total_bytes = 0;

    ChunkManifest manifest;
    manifest.total_chunks = 0;

    auto result = transfer_engine_->startTransfer(task, manifest);
    EXPECT_TRUE(result.isErr());
}

TEST_F(TransferEngineRwTest, ParallelismSetting) {
    transfer_engine_->setParallelism(4);
    EXPECT_EQ(transfer_engine_->getParallelism(), 4u);

    transfer_engine_->setParallelism(0);
    EXPECT_EQ(transfer_engine_->getParallelism(), 1u);

    transfer_engine_->setParallelism(kMaxParallelism + 10);
    EXPECT_EQ(transfer_engine_->getParallelism(), kMaxParallelism);
}

}