#include "DiskObjectStorage.h"
#include <gtest/gtest.h>
#include <filesystem>
#include <string>
#include <vector>
#include <algorithm>

namespace fs = std::filesystem;

class TestDiskObjectStorage : public ::testing::Test
{
protected:
    fs::path testRootDir;
    std::unique_ptr<DiskObjectStorage> storage;

    void SetUp() override
    {
        testRootDir = fs::temp_directory_path() / ("obsto_test_" + std::to_string(std::rand()));
        fs::create_directories(testRootDir);
        storage = std::make_unique<DiskObjectStorage>(testRootDir);
    }

    void TearDown() override
    {
        std::error_code ec;
        fs::remove_all(testRootDir, ec);
    }
};

TEST_F(TestDiskObjectStorage, putAndGetSuccess)
{
    std::string key = "folder/file.txt";
    std::string content = "hello world";

    EXPECT_EQ(storage->put(key, content), PutResult::Ok);
    auto result = storage->get(key);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), content);
}

TEST_F(TestDiskObjectStorage, putOverwriteExistingKey)
{
    std::string key = "data.bin";
    EXPECT_EQ(storage->put(key, "initial"), PutResult::Ok);
    EXPECT_EQ(storage->put(key, "updated"), PutResult::Ok);

    auto result = storage->get(key);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), "updated");
}

TEST_F(TestDiskObjectStorage, getNonExistentKeyReturnsNullopt)
{
    EXPECT_FALSE(storage->get("missing_key.txt").has_value());
}

TEST_F(TestDiskObjectStorage, existsCheck)
{
    std::string key = "check_me.txt";
    EXPECT_FALSE(storage->exists(key));

    EXPECT_EQ(storage->put(key, "payload"), PutResult::Ok);
    EXPECT_TRUE(storage->exists(key));
}

TEST_F(TestDiskObjectStorage, setNxInsertsWhenNotPresent)
{
    std::string key = "unique_key.txt";
    EXPECT_EQ(storage->setnx(key, "first_insert"), SetNxResult::Inserted);

    auto result = storage->get(key);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), "first_insert");
}

TEST_F(TestDiskObjectStorage, setNxFailsWhenAlreadyExists)
{
    std::string key = "existing_key.txt";
    EXPECT_EQ(storage->put(key, "original"), PutResult::Ok);

    EXPECT_EQ(storage->setnx(key, "new_data"), SetNxResult::AlreadyExists);

    auto result = storage->get(key);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), "original");
}

TEST_F(TestDiskObjectStorage, keysListing)
{
    storage->put("a.txt", "1");
    storage->put("sub/b.txt", "2");

    auto keys = storage->keys();
    EXPECT_EQ(keys.size(), 2);

    // Normalize slashes for comparison across platforms
    std::vector<std::string> normalizedKeys;
    for (const auto& k : keys) 
    {
        normalizedKeys.push_back(fs::path(k).generic_string());
    }

    EXPECT_NE(std::find(normalizedKeys.begin(), normalizedKeys.end(), "a.txt"), normalizedKeys.end());
    EXPECT_NE(std::find(normalizedKeys.begin(), normalizedKeys.end(), "sub/b.txt"), normalizedKeys.end());
}

TEST_F(TestDiskObjectStorage, rejectsPathTraversal)
{
    std::vector<std::string> maliciousKeys = {
        "../etc/passwd",
        "foo/../../bar",
        "sub/dir/..",
        ".."
    };

    for (const auto& key : maliciousKeys)
    {
        EXPECT_EQ(storage->put(key, "data"), PutResult::InvalidPath);
        EXPECT_FALSE(storage->get(key).has_value());
        EXPECT_FALSE(storage->exists(key));
        EXPECT_FALSE(storage->del(key));
        EXPECT_EQ(storage->setnx(key, "data"), SetNxResult::InvalidPath);
    }
}

TEST_F(TestDiskObjectStorage, rejectsEmptyPath)
{
    EXPECT_EQ(storage->put("", "data"), PutResult::InvalidPath);
    EXPECT_FALSE(storage->get("").has_value());
    EXPECT_FALSE(storage->exists(""));
    EXPECT_FALSE(storage->del(""));
    EXPECT_EQ(storage->setnx("", "data"), SetNxResult::InvalidPath);
}

TEST_F(TestDiskObjectStorage, absolutePathInjectionAttempt)
{
    fs::path outsideFile = fs::temp_directory_path() / "obsto_outside_target.txt";

    PutResult res = storage->put(outsideFile.string(), "hacked");

    EXPECT_EQ(res, PutResult::InvalidPath);
    EXPECT_FALSE(fs::exists(outsideFile));

    std::error_code ec;
    fs::remove(outsideFile, ec);
}

TEST_F(TestDiskObjectStorage, createsNestedSubdirectoriesAutomatically)
{
    std::string key = "a/b/c/d/e/deep_file.txt";
    EXPECT_EQ(storage->put(key, "nested"), PutResult::Ok);
    EXPECT_TRUE(storage->exists(key));

    auto res = storage->get(key);
    ASSERT_TRUE(res.has_value());
    EXPECT_EQ(res.value(), "nested");
}

TEST_F(TestDiskObjectStorage, delRemovesKey)
{
    std::string key = "to_delete.txt";
    EXPECT_EQ(storage->put(key, "temp"), PutResult::Ok);
    EXPECT_TRUE(storage->exists(key));

    EXPECT_TRUE(storage->del(key));
    EXPECT_FALSE(storage->exists(key));
    EXPECT_FALSE(storage->get(key).has_value());
}

TEST_F(TestDiskObjectStorage, delNonExistentKeyReturnsFalse)
{
    EXPECT_FALSE(storage->del("nonexistent.txt"));
}

TEST_F(TestDiskObjectStorage, delLeavesEmptyDirectories)
{
    std::string key = "dir1/dir2/file.txt";
    EXPECT_EQ(storage->put(key, "content"), PutResult::Ok);
    EXPECT_TRUE(storage->exists(key));

    EXPECT_TRUE(storage->del(key));
    EXPECT_FALSE(storage->exists(key));

    // Check that the directories still exist
    EXPECT_TRUE(fs::exists(testRootDir / "dir1"));
    EXPECT_TRUE(fs::exists(testRootDir / "dir1" / "dir2"));
}
