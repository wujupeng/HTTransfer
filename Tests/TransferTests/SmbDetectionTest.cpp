#include <gtest/gtest.h>
#include "Transfer/TransferEngine.h"
#include "Core/LocalFileSource.h"
#include "Core/LocalFileSink.h"
#include "Core/Common/Types.h"
#include <filesystem>
#include <fstream>
#include <cstdio>

namespace ht {

TEST(SmbDetectionTest, UncPathIsSMB) {
    EXPECT_TRUE(TransferEngine::isSMB("\\\\NAS\\Share\\file.bak"));
    EXPECT_TRUE(TransferEngine::isSMB("\\\\server\\path"));
}

TEST(SmbDetectionTest, LocalPathIsNotSMB) {
    EXPECT_FALSE(TransferEngine::isSMB("E:\\Backup\\file.bak"));
    EXPECT_FALSE(TransferEngine::isSMB("C:\\Users\\test\\file.txt"));
    EXPECT_FALSE(TransferEngine::isSMB("/home/user/file.txt"));
    EXPECT_FALSE(TransferEngine::isSMB("file.txt"));
    EXPECT_FALSE(TransferEngine::isSMB(""));
    EXPECT_FALSE(TransferEngine::isSMB("X"));
}

}