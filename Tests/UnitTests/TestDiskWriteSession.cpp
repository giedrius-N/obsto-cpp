#include "DiskWriteSession.h"
#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <algorithm>

namespace fs = std::filesystem;

class TestDiskWriteSession : public ::testing::Test
{
protected:
    fs::path testDir;
    fs::path tempPath;
    fs::path finalPath;

    void SetUp() override
    {
        testDir = fs::temp_directory_path() / ("obsto_test_" + std::to_string(std::rand()));
        fs::remove_all(testDir);
        fs::create_directories(testDir);

        tempPath = testDir / "object.tmp";
        finalPath = testDir / "object.bin";
    }

    void TearDown() override
    {
        std::error_code ec;
        fs::remove_all(testDir, ec);
    }

    std::string readFile(const fs::path& path)
    {
        std::ifstream file(path, std::ios::binary);
        std::ostringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    }
};

TEST_F(TestDiskWriteSession, writeChunkThenFinishProducesCorrectFile)
{
    DiskWriteSession session(tempPath, finalPath);

    EXPECT_TRUE(session.writeChunk("hello "));
    EXPECT_TRUE(session.writeChunk("world"));

    PutResult result = session.finish();

    EXPECT_EQ(result, PutResult::Ok);
    EXPECT_TRUE(fs::exists(finalPath));
    EXPECT_FALSE(fs::exists(tempPath));
    EXPECT_EQ(readFile(finalPath), "hello world");
}

TEST_F(TestDiskWriteSession, multipleSmallChunksAssembleCorrectly)
{
    DiskWriteSession session(tempPath, finalPath);

    for (int i = 0; i < 100; i++)
    {
        EXPECT_TRUE(session.writeChunk("x"));
    }
    session.finish();

    EXPECT_EQ(readFile(finalPath).size(), 100);
    EXPECT_EQ(readFile(finalPath), std::string(100, 'x'));
}

TEST_F(TestDiskWriteSession, emptyChunksAreHandledGracefully)
{
    DiskWriteSession session(tempPath, finalPath);

    EXPECT_TRUE(session.writeChunk(""));
    EXPECT_TRUE(session.writeChunk("data"));
    EXPECT_TRUE(session.writeChunk(""));

    session.finish();
    EXPECT_EQ(readFile(finalPath), "data");
}

TEST_F(TestDiskWriteSession, binaryDataWithEmbeddedNullBytesIsPreserved)
{
    DiskWriteSession session(tempPath, finalPath);

    std::string chunk("ab\0cd", 5);
    EXPECT_TRUE(session.writeChunk(chunk));
    session.finish();

    std::string result = readFile(finalPath);
    EXPECT_EQ(result.size(), 5);
    EXPECT_EQ(result, chunk);
}

TEST_F(TestDiskWriteSession, abortRemovesTempFileAndNeverCreatesFinalFile)
{
    DiskWriteSession session(tempPath, finalPath);

    session.writeChunk("partial data that should never be visible");
    session.abort();

    EXPECT_FALSE(fs::exists(tempPath));
    EXPECT_FALSE(fs::exists(finalPath));
}

TEST_F(TestDiskWriteSession, finishAtomicallyReplacesAnyExistingFinalFile)
{
    {
        std::ofstream old(finalPath, std::ios::binary);
        old << "old content";
    }
    ASSERT_TRUE(fs::exists(finalPath));

    DiskWriteSession session(tempPath, finalPath);
    session.writeChunk("new content");
    PutResult result = session.finish();

    EXPECT_EQ(result, PutResult::Ok);
    EXPECT_EQ(readFile(finalPath), "new content");
}

TEST_F(TestDiskWriteSession, concurrentGetDuringWriteNeverSeesPartialData)
{
    DiskWriteSession session(tempPath, finalPath);

    session.writeChunk("first chunk ");
    EXPECT_FALSE(fs::exists(finalPath));

    session.writeChunk("second chunk");
    EXPECT_FALSE(fs::exists(finalPath)); // still not visible before finish()

    session.finish();
    EXPECT_TRUE(fs::exists(finalPath));
    EXPECT_EQ(readFile(finalPath), "first chunk second chunk");
}

TEST_F(TestDiskWriteSession, largeDataAcrossManyChunksAssemblesCorrectly)
{
    DiskWriteSession session(tempPath, finalPath);

    std::string expected;
    for (int i = 0; i < 1000; i++)
    {
        std::string chunk(1024, static_cast<char>('a' + (i % 26)));
        expected += chunk;
        ASSERT_TRUE(session.writeChunk(chunk));
    }

    session.finish();
    EXPECT_EQ(readFile(finalPath), expected);
    EXPECT_EQ(readFile(finalPath).size(), 1000 * 1024);
}