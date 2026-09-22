#include "StoragePersistency.h"
#include <gtest/gtest.h>
#include <algorithm>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

class TestStoragePersistency : public ::testing::Test
{
protected:
    fs::path testDir;

    void SetUp() override
    {
        testDir = fs::temp_directory_path() / ("persistency_test_" + std::to_string(std::rand()));
        fs::remove_all(testDir);
        fs::create_directories(testDir);
    }

    void TearDown() override
    {
        std::error_code ec;
        fs::remove_all(testDir, ec);
    }

    void createFile(const std::string& path, const std::string& content = "test")
    {
        fs::path fullPath = testDir / path;
        fs::create_directories(fullPath.parent_path());
        std::ofstream file(fullPath, std::ios::binary);
        file.write(content.data(), static_cast<std::streamsize>(content.size()));
        file.close();
    }
};

TEST_F(TestStoragePersistency, constructorScansExistingFiles)
{
    createFile("file1.txt", "hello");
    createFile("file2.txt", "world");
    createFile("sub/file3.txt", "data");

    StoragePersistency persistency(testDir);

    auto keys = persistency.keys();
    auto entries = persistency.entries();

    EXPECT_EQ(keys.size(), 3);
    EXPECT_EQ(entries.size(), 3);

    std::sort(keys.begin(), keys.end());
    EXPECT_EQ(keys[0], "file1.txt");
    EXPECT_EQ(keys[1], "file2.txt");
    EXPECT_EQ(keys[2], "sub/file3.txt");
}

TEST_F(TestStoragePersistency, constructorIgnoresTempFiles)
{
    createFile("data.txt", "real");
    createFile("data.tmp", "temp");

    StoragePersistency persistency(testDir);

    auto keys = persistency.keys();
    EXPECT_EQ(keys.size(), 1);
    EXPECT_EQ(keys[0], "data.txt");
}

TEST_F(TestStoragePersistency, objectSizeReturnsCorrectSize)
{
    createFile("file.txt", "12345");

    StoragePersistency persistency(testDir);

    auto size = persistency.objectSize("file.txt");
    ASSERT_TRUE(size.has_value());
    EXPECT_EQ(*size, 5);
}

TEST_F(TestStoragePersistency, objectSizeReturnsNulloptForMissing)
{
    StoragePersistency persistency(testDir);

    auto size = persistency.objectSize("does_not_exist.txt");
    EXPECT_FALSE(size.has_value());
}

TEST_F(TestStoragePersistency, totalBytesSumAllFiles)
{
    createFile("a.txt", "123");
    createFile("b.txt", "12345");
    createFile("sub/c.txt", "1");

    StoragePersistency persistency(testDir);

    EXPECT_EQ(persistency.totalBytes(), 9);
}

TEST_F(TestStoragePersistency, upsertAddsNewFile)
{
    StoragePersistency persistency(testDir);

    EXPECT_FALSE(persistency.contains("new.txt"));
    EXPECT_EQ(persistency.totalBytes(), 0);

    persistency.upsert("new.txt", 100);

    EXPECT_TRUE(persistency.contains("new.txt"));
    EXPECT_EQ(persistency.totalBytes(), 100);

    auto size = persistency.objectSize("new.txt");
    ASSERT_TRUE(size.has_value());
    EXPECT_EQ(*size, 100);
}

TEST_F(TestStoragePersistency, upsertUpdatesExistingFile)
{
    createFile("file.txt", "old");
    StoragePersistency persistency(testDir);

    auto size1 = persistency.objectSize("file.txt");
    ASSERT_TRUE(size1.has_value());
    size_t oldSize = *size1;

    persistency.upsert("file.txt", 999);

    auto size2 = persistency.objectSize("file.txt");
    ASSERT_TRUE(size2.has_value());
    EXPECT_EQ(*size2, 999);
    EXPECT_EQ(persistency.totalBytes(), 999);
}

TEST_F(TestStoragePersistency, eraseRemovesFileAndDecreasesTotal)
{
    createFile("file.txt", "data");
    StoragePersistency persistency(testDir);

    EXPECT_TRUE(persistency.contains("file.txt"));
    size_t initialTotal = persistency.totalBytes();

    bool erased = persistency.erase("file.txt");

    EXPECT_TRUE(erased);
    EXPECT_FALSE(persistency.contains("file.txt"));
    EXPECT_EQ(persistency.totalBytes(), 0);
}

TEST_F(TestStoragePersistency, eraseReturnsFalseForMissing)
{
    StoragePersistency persistency(testDir);

    bool erased = persistency.erase("does_not_exist.txt");

    EXPECT_FALSE(erased);
}

TEST_F(TestStoragePersistency, refreshRebuildsIndex)
{
    StoragePersistency persistency(testDir);

    EXPECT_EQ(persistency.keys().size(), 0);

    createFile("new1.txt", "a");
    createFile("new2.txt", "b");

    EXPECT_EQ(persistency.keys().size(), 0);

    persistency.refresh();

    auto keys = persistency.keys();
    EXPECT_EQ(keys.size(), 2);

    std::sort(keys.begin(), keys.end());
    EXPECT_EQ(keys[0], "new1.txt");
    EXPECT_EQ(keys[1], "new2.txt");
}

TEST_F(TestStoragePersistency, containsWorksCorrectly)
{
    createFile("exists.txt", "data");
    StoragePersistency persistency(testDir);

    EXPECT_TRUE(persistency.contains("exists.txt"));
    EXPECT_FALSE(persistency.contains("does_not.txt"));
}

TEST_F(TestStoragePersistency, entriesReturnsAllPathsAndSizes)
{
    createFile("a.txt", "123");
    createFile("b.txt", "12345");

    StoragePersistency persistency(testDir);

    auto entries = persistency.entries();
    EXPECT_EQ(entries.size(), 2);

    size_t total = 0;
    for (const auto& [path, size] : entries)
    {
        total += size;
    }
    EXPECT_EQ(total, 8);
}
