#include "DiskReadSession.h"
#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <algorithm>

namespace fs = std::filesystem;

class TestDiskReadSession : public ::testing::Test
{
protected:
    fs::path testDir;
    fs::path filePath;

    void SetUp() override
    {
        testDir = fs::temp_directory_path() / ("obsto_read_test_" + std::to_string(std::rand()));
        fs::remove_all(testDir);
        fs::create_directories(testDir);

        filePath = testDir / "object.bin";
    }

    void TearDown() override
    {
        std::error_code ec;
        fs::remove_all(testDir, ec);
    }

    void createTestFile(const std::string& content)
    {
        std::ofstream file(filePath, std::ios::binary);
        file.write(content.data(), static_cast<std::streamsize>(content.size()));
        file.close();
    }

    std::string readAllViaSession(DiskReadSession& session)
    {
        std::string result;
        while (true)
        {
            auto chunk = session.readChunk(1024);
            if (!chunk.has_value())
                break;
            result += *chunk;
        }
        return result;
    }
};

TEST_F(TestDiskReadSession, readsEntireFileInOneChunk)
{
    std::string expected = "Hello, World!";
    createTestFile(expected);

    DiskReadSession session(filePath);
    auto chunk = session.readChunk(1024);

    ASSERT_TRUE(chunk.has_value());
    EXPECT_EQ(*chunk, expected);
    EXPECT_EQ(chunk->size(), expected.size());

    auto next = session.readChunk(1024);
    EXPECT_FALSE(next.has_value());
}

TEST_F(TestDiskReadSession, readsFileInMultipleChunks)
{
    std::string expected(4096, 'X');
    createTestFile(expected);

    DiskReadSession session(filePath);
    std::string result;

    while (true)
    {
        auto chunk = session.readChunk(1024);
        if (!chunk.has_value())
            break;
        result += *chunk;
    }

    EXPECT_EQ(result, expected);
    EXPECT_EQ(result.size(), 4096);
}

TEST_F(TestDiskReadSession, readChunkRespectsMaxBytesLimit)
{
    std::string expected = "This is a test string";
    createTestFile(expected);

    DiskReadSession session(filePath);
    
    auto chunk1 = session.readChunk(5);
    ASSERT_TRUE(chunk1.has_value());
    EXPECT_EQ(*chunk1, "This ");
    EXPECT_EQ(chunk1->size(), 5);

    auto chunk2 = session.readChunk(2);
    ASSERT_TRUE(chunk2.has_value());
    EXPECT_EQ(*chunk2, "is");
    EXPECT_EQ(chunk2->size(), 2);
}

TEST_F(TestDiskReadSession, returnsNulloptWhenFileIsEmpty)
{
    createTestFile("");

    DiskReadSession session(filePath);
    auto chunk = session.readChunk(1024);

    EXPECT_FALSE(chunk.has_value());
}

TEST_F(TestDiskReadSession, returnsNulloptWhenFileDoesNotExist)
{
    DiskReadSession session(filePath);
    auto chunk = session.readChunk(1024);

    EXPECT_FALSE(chunk.has_value());
}

TEST_F(TestDiskReadSession, readsBinaryDataWithNullBytes)
{
    std::string expected("ab\0cd\0ef", 8);
    createTestFile(expected);

    DiskReadSession session(filePath);
    auto chunk = session.readChunk(1024);

    ASSERT_TRUE(chunk.has_value());
    EXPECT_EQ(chunk->size(), 8);
    EXPECT_EQ(*chunk, expected);
}

TEST_F(TestDiskReadSession, readsLargeFileAcrossMultipleCalls)
{
    const size_t fileSize = 1024 * 1024;
    std::string expected(fileSize, 'A');
    createTestFile(expected);

    DiskReadSession session(filePath);
    std::string result;
    size_t totalBytesRead = 0;

    while (true)
    {
        auto chunk = session.readChunk(64 * 1024);
        if (!chunk.has_value())
            break;
        totalBytesRead += chunk->size();
        result += *chunk;
    }

    EXPECT_EQ(totalBytesRead, fileSize);
    EXPECT_EQ(result, expected);
    EXPECT_EQ(result.size(), fileSize);
}

TEST_F(TestDiskReadSession, readsFileWithExactChunkSize)
{
    std::string expected(1024, 'B');
    createTestFile(expected);

    DiskReadSession session(filePath);
    auto chunk = session.readChunk(1024);

    ASSERT_TRUE(chunk.has_value());
    EXPECT_EQ(chunk->size(), 1024);
    EXPECT_EQ(*chunk, expected);

    auto next = session.readChunk(1024);
    EXPECT_FALSE(next.has_value());
}

TEST_F(TestDiskReadSession, readsFileWithUnevenLastChunk)
{
    std::string expected(1000, 'C');
    createTestFile(expected);

    DiskReadSession session(filePath);
    std::string result;

    auto chunk1 = session.readChunk(256);
    ASSERT_TRUE(chunk1.has_value());
    EXPECT_EQ(chunk1->size(), 256);
    result += *chunk1;

    auto chunk2 = session.readChunk(256);
    ASSERT_TRUE(chunk2.has_value());
    EXPECT_EQ(chunk2->size(), 256);
    result += *chunk2;

    auto chunk3 = session.readChunk(256);
    ASSERT_TRUE(chunk3.has_value());
    EXPECT_EQ(chunk3->size(), 256);
    result += *chunk3;

    auto chunk4 = session.readChunk(256);
    ASSERT_TRUE(chunk4.has_value());
    EXPECT_EQ(chunk4->size(), 232);
    result += *chunk4;

    auto chunk5 = session.readChunk(256);
    EXPECT_FALSE(chunk5.has_value());

    EXPECT_EQ(result, expected);
    EXPECT_EQ(result.size(), 1000);
}

TEST_F(TestDiskReadSession, multipleSessionsOnSameFileReadIndependently)
{
    std::string expected = "Independent readers";
    createTestFile(expected);

    DiskReadSession session1(filePath);
    DiskReadSession session2(filePath);

    auto chunk1 = session1.readChunk(5);
    auto chunk2 = session2.readChunk(5);

    ASSERT_TRUE(chunk1.has_value());
    ASSERT_TRUE(chunk2.has_value());
    EXPECT_EQ(*chunk1, "Indep");
    EXPECT_EQ(*chunk2, "Indep");

    auto chunk1b = session1.readChunk(5);
    ASSERT_TRUE(chunk1b.has_value());
    EXPECT_EQ(*chunk1b, "enden");
}

TEST_F(TestDiskReadSession, doesNotThrowWhenReadingPastEOF)
{
    std::string expected = "Small file";
    createTestFile(expected);

    DiskReadSession session(filePath);
    
    auto chunk1 = session.readChunk(1024);
    ASSERT_TRUE(chunk1.has_value());

    auto chunk2 = session.readChunk(1024);
    EXPECT_FALSE(chunk2.has_value());
    
    auto chunk3 = session.readChunk(1024);
    EXPECT_FALSE(chunk3.has_value());
}

TEST_F(TestDiskReadSession, zeroByteReadChunkHandlesGracefully)
{
    std::string expected = "Test content";
    createTestFile(expected);

    DiskReadSession session(filePath);
    
    auto chunk = session.readChunk(0);
    EXPECT_FALSE(chunk.has_value());
}

TEST_F(TestDiskReadSession, sequentialReadsPreserveDataIntegrity)
{
    std::string expected = "The quick brown fox jumps over the lazy dog";
    createTestFile(expected);

    DiskReadSession session(filePath);
    std::string result;

    auto chunk1 = session.readChunk(10);
    ASSERT_TRUE(chunk1.has_value());
    result += *chunk1;

    auto chunk2 = session.readChunk(10);
    ASSERT_TRUE(chunk2.has_value());
    result += *chunk2;

    auto chunk3 = session.readChunk(10);
    ASSERT_TRUE(chunk3.has_value());
    result += *chunk3;

    auto chunk4 = session.readChunk(10);
    ASSERT_TRUE(chunk4.has_value());
    result += *chunk4;

    auto chunk5 = session.readChunk(10);
    ASSERT_TRUE(chunk5.has_value());
    result += *chunk5;

    EXPECT_EQ(result, expected);
}

TEST_F(TestDiskReadSession, readChunkAfterCloseReturnsNullopt)
{
    std::string expected = "Test data";
    createTestFile(expected);

    DiskReadSession session(filePath);
    
    auto chunk1 = session.readChunk(1024);
    ASSERT_TRUE(chunk1.has_value());
    EXPECT_EQ(*chunk1, expected);
}
