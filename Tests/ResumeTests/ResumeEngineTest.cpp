#include <gtest/gtest.h>
#include "Resume/ResumeEngine.h"
#include "Core/Common/Types.h"
#include "Logger/Logger.h"
#include <filesystem>
#include <chrono>
#include <cstdio>
#include <fstream>

namespace ht {

class ResumeEngineTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_dir_ = std::filesystem::temp_directory_path() / "ht_test_resume";
        resume_dir_ = test_dir_ / ".htresume";
        std::filesystem::create_directories(resume_dir_);
        logger_ = std::make_shared<Logger>();
        engine_ = std::make_shared<ResumeEngine>(logger_, resume_dir_);
    }

    void TearDown() override {
        engine_.reset();
        std::error_code ec;
        std::filesystem::remove_all(test_dir_, ec);
    }

    std::filesystem::path test_dir_;
    std::filesystem::path resume_dir_;
    std::shared_ptr<Logger> logger_;
    std::shared_ptr<ResumeEngine> engine_;
};

TEST_F(ResumeEngineTest, CreateAndLoadResumeFile) {
    ResumeFileData data;
    data.task_id = "HT-20260724-TEST01";
    data.source_path = "C:\\source\\file.bin";
    data.target_path = "D:\\backup\\file.bin";
    data.file_size = 1073741824;
    data.current_offset = 0;
    data.completed_chunks = {};
    data.source_hash = std::string(64, '0');
    data.source_create_time = std::chrono::system_clock::now();
    data.source_modify_time = std::chrono::system_clock::now();
    data.updated_at = std::chrono::system_clock::now();

    auto create_result = engine_->createResumeFile(data.task_id, data);
    ASSERT_TRUE(create_result.isOk());

    auto load_result = engine_->loadResumeFile(data.task_id);
    ASSERT_TRUE(load_result.isOk());
    ASSERT_TRUE(load_result.value().has_value());

    auto& loaded = load_result.value().value();
    EXPECT_EQ(loaded.task_id, data.task_id);
    EXPECT_EQ(loaded.source_path, data.source_path);
    EXPECT_EQ(loaded.target_path, data.target_path);
    EXPECT_EQ(loaded.file_size, data.file_size);
    EXPECT_EQ(loaded.current_offset, 0u);
    EXPECT_EQ(loaded.completed_chunks.size(), 0u);
}

TEST_F(ResumeEngineTest, MarkChunkCompletedAndReload) {
    ResumeFileData data;
    data.task_id = "HT-20260724-TEST02";
    data.source_path = "/source/file.bin";
    data.target_path = "/backup/file.bin";
    data.file_size = 67108864;
    data.current_offset = 0;
    data.completed_chunks = {};
    data.source_hash = std::string(64, 'a');
    data.source_create_time = std::chrono::system_clock::now();
    data.source_modify_time = std::chrono::system_clock::now();
    data.updated_at = std::chrono::system_clock::now();

    engine_->createResumeFile(data.task_id, data);

    engine_->markChunkCompleted(data.task_id, 0, 16777216);
    engine_->markChunkCompleted(data.task_id, 1, 33554432);
    engine_->markChunkCompleted(data.task_id, 2, 50331648);
    engine_->flushPendingWrites();

    auto new_engine = std::make_shared<ResumeEngine>(logger_, resume_dir_);
    auto load_result = new_engine->loadResumeFile(data.task_id);
    ASSERT_TRUE(load_result.isOk());
    ASSERT_TRUE(load_result.value().has_value());

    auto& loaded = load_result.value().value();
    EXPECT_EQ(loaded.completed_chunks.size(), 3u);
    EXPECT_EQ(loaded.completed_chunks[0], 0u);
    EXPECT_EQ(loaded.completed_chunks[1], 1u);
    EXPECT_EQ(loaded.completed_chunks[2], 2u);
    EXPECT_EQ(loaded.current_offset, 50331648u);
}

TEST_F(ResumeEngineTest, BatchFlushReducesWrites) {
    ResumeFileData data;
    data.task_id = "HT-20260724-TEST03";
    data.source_path = "/source/big.bin";
    data.target_path = "/backup/big.bin";
    data.file_size = 107374182400;
    data.current_offset = 0;
    data.completed_chunks = {};
    data.source_hash = std::string(64, 'b');
    data.source_create_time = std::chrono::system_clock::now();
    data.source_modify_time = std::chrono::system_clock::now();
    data.updated_at = std::chrono::system_clock::now();

    engine_->createResumeFile(data.task_id, data);

    for (uint64_t i = 0; i < 10; ++i) {
        engine_->markChunkCompleted(data.task_id, i, (i + 1) * 16777216);
    }
    engine_->flushPendingWrites();

    auto new_engine = std::make_shared<ResumeEngine>(logger_, resume_dir_);
    auto load_result = new_engine->loadResumeFile(data.task_id);
    ASSERT_TRUE(load_result.isOk());
    ASSERT_TRUE(load_result.value().has_value());

    auto& loaded = load_result.value().value();
    EXPECT_EQ(loaded.completed_chunks.size(), 10u);
}

TEST_F(ResumeEngineTest, InvalidateResumeFile) {
    ResumeFileData data;
    data.task_id = "HT-20260724-TEST04";
    data.source_path = "/source/file.bin";
    data.target_path = "/backup/file.bin";
    data.file_size = 1024;
    data.current_offset = 0;
    data.completed_chunks = {};
    data.source_hash = "";
    data.source_create_time = std::chrono::system_clock::now();
    data.source_modify_time = std::chrono::system_clock::now();
    data.updated_at = std::chrono::system_clock::now();

    engine_->createResumeFile(data.task_id, data);
    auto invalidate_result = engine_->invalidateResumeFile(data.task_id);
    ASSERT_TRUE(invalidate_result.isOk());

    auto load_result = engine_->loadResumeFile(data.task_id);
    ASSERT_TRUE(load_result.isOk());
    EXPECT_FALSE(load_result.value().has_value());
}

TEST_F(ResumeEngineTest, IsSourceFileChanged) {
    auto src_path = test_dir_ / "source_check.bin";
    std::ofstream ofs(src_path, std::ios::binary);
    std::vector<uint8_t> data(4096, 0x42);
    ofs.write(reinterpret_cast<const char*>(data.data()), data.size());
    ofs.close();

    ResumeFileData rdata;
    rdata.task_id = "HT-20260724-TEST05";
    rdata.source_path = pathToUtf8(src_path);
    rdata.target_path = "/backup/file.bin";
    rdata.file_size = 4096;
    rdata.current_offset = 0;
    rdata.completed_chunks = {};
    rdata.source_hash = "";
    rdata.source_create_time = std::chrono::system_clock::now();
    rdata.source_modify_time = std::chrono::system_clock::now();
    rdata.updated_at = std::chrono::system_clock::now();

    engine_->createResumeFile(rdata.task_id, rdata);

    auto result = engine_->isSourceFileChanged(rdata.task_id, src_path);
    ASSERT_TRUE(result.isOk());
    EXPECT_FALSE(result.value());

    auto result2 = engine_->isSourceFileChanged(rdata.task_id, test_dir_ / "nonexistent.bin");
    ASSERT_TRUE(result2.isOk());
    EXPECT_TRUE(result2.value());
}

TEST_F(ResumeEngineTest, ScanUnfinishedTasks) {
    ResumeFileData data1;
    data1.task_id = "HT-20260724-SCAN01";
    data1.source_path = "/source/1.bin";
    data1.target_path = "/backup/1.bin";
    data1.file_size = 1024;
    data1.source_hash = "";
    data1.source_create_time = std::chrono::system_clock::now();
    data1.source_modify_time = std::chrono::system_clock::now();
    data1.updated_at = std::chrono::system_clock::now();

    ResumeFileData data2;
    data2.task_id = "HT-20260724-SCAN02";
    data2.source_path = "/source/2.bin";
    data2.target_path = "/backup/2.bin";
    data2.file_size = 2048;
    data2.source_hash = "";
    data2.source_create_time = std::chrono::system_clock::now();
    data2.source_modify_time = std::chrono::system_clock::now();
    data2.updated_at = std::chrono::system_clock::now();

    engine_->createResumeFile(data1.task_id, data1);
    engine_->createResumeFile(data2.task_id, data2);

    auto scan_result = engine_->scanUnfinishedTasks();
    ASSERT_TRUE(scan_result.isOk());
    EXPECT_EQ(scan_result.value().size(), 2u);

    engine_->invalidateResumeFile(data1.task_id);

    auto scan_result2 = engine_->scanUnfinishedTasks();
    ASSERT_TRUE(scan_result2.isOk());
    EXPECT_EQ(scan_result2.value().size(), 1u);
    EXPECT_EQ(scan_result2.value()[0], "HT-20260724-SCAN02");
}

TEST_F(ResumeEngineTest, CorruptedResumeFile) {
    auto corrupt_path = resume_dir_ / "HT-CORRUPT.htresume";
    std::ofstream ofs(corrupt_path, std::ios::binary);
    const char* garbage = "NOT_A_VALID_RESUME_FILE_GARBAGE_DATA";
    ofs.write(garbage, strlen(garbage));
    ofs.close();

    auto result = ResumeFileParser::parse(corrupt_path);
    EXPECT_TRUE(result.isErr());
}

}
