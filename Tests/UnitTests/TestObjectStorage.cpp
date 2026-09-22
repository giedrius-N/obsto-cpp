#include "ObjectStorage.h"
#include <gtest/gtest.h>
#include <algorithm>

class TestObjectStorage : public ::testing::Test
{
protected:
    ObjectStorage storage;
    const std::string path = "test/path";
    const std::string data = "testing data";
};


TEST_F(TestObjectStorage, putAndGet)
{
    storage.put(path, data);
    auto retrievedData = storage.get(path);

    ASSERT_TRUE(retrievedData.has_value());
    EXPECT_EQ(retrievedData.value(), data);
}

TEST_F(TestObjectStorage, del)
{
    storage.put(path, data);
    EXPECT_TRUE(storage.del(path));
    EXPECT_FALSE(storage.exists(path));
}

TEST_F(TestObjectStorage, getKeys)
{
    storage.put(path, data);
    storage.put("another/path", "more data");
    storage.put("yet/another/path", "even more data");
    auto keys = storage.keys();

    EXPECT_EQ(keys.size(), 3);
    EXPECT_NE(std::find(keys.begin(), keys.end(), path), keys.end());
    EXPECT_NE(std::find(keys.begin(), keys.end(), "another/path"), keys.end());
    EXPECT_NE(std::find(keys.begin(), keys.end(), "yet/another/path"), keys.end());
}

TEST_F(TestObjectStorage, keysWhenNoData)
{
    auto keys = storage.keys();
    EXPECT_TRUE(keys.empty());
}

TEST_F(TestObjectStorage, exists)
{
    storage.put(path, data);
    EXPECT_TRUE(storage.exists(path));
    storage.del(path);
    EXPECT_FALSE(storage.exists(path));
}

TEST_F(TestObjectStorage, setnx)
{
    EXPECT_EQ(storage.setnx(path, data), SetNxResult::Inserted);
    EXPECT_EQ(storage.setnx(path, "new data"), SetNxResult::AlreadyExists);

    auto retrievedData = storage.get(path);
    ASSERT_TRUE(retrievedData.has_value());
    EXPECT_EQ(retrievedData.value(), data);
}

TEST_F(TestObjectStorage, getNonExistentKey)
{
    auto retrievedData = storage.get("nonexistent/path");
    EXPECT_FALSE(retrievedData.has_value());
}

TEST_F(TestObjectStorage, delNonExistentKey)
{
    EXPECT_FALSE(storage.del("nonexistent/path"));
}

TEST_F(TestObjectStorage, doublePut)
{
    storage.put(path, data);
    storage.put(path, "new data");

    auto retrievedData = storage.get(path);
    ASSERT_TRUE(retrievedData.has_value());
    EXPECT_EQ(retrievedData.value(), "new data");
}

TEST_F(TestObjectStorage, emptyStringDataIsStored)
{
    storage.put(path, "");
    auto retrievedData = storage.get(path);

    ASSERT_TRUE(retrievedData.has_value());
    EXPECT_EQ(retrievedData.value(), "");
}

TEST_F(TestObjectStorage, emptyPathIsRejected)
{
    storage.put("", data);

    EXPECT_FALSE(storage.exists(""));
    EXPECT_FALSE(storage.get("").has_value());
}

TEST_F(TestObjectStorage, putCopiesBytesFromCallerString)
{
    std::string callerValue = "owned by caller";
    storage.put(path, callerValue);

    callerValue.clear();

    auto retrievedData = storage.get(path);
    ASSERT_TRUE(retrievedData.has_value());
    EXPECT_EQ(retrievedData.value(), "owned by caller");
}

TEST_F(TestObjectStorage, setnxDoesNotConsumeBytesWhenKeyAlreadyExists)
{
    EXPECT_EQ(storage.setnx(path, data), SetNxResult::Inserted);

    std::string callerValue = "should stay intact";
    SetNxResult result = storage.setnx(path, callerValue);

    EXPECT_EQ(result, SetNxResult::AlreadyExists);
    EXPECT_EQ(callerValue, "should stay intact");

    auto retrievedData = storage.get(path);
    ASSERT_TRUE(retrievedData.has_value());
    EXPECT_EQ(retrievedData.value(), data);
}

TEST_F(TestObjectStorage, embeddedNullBytesAreStoredAndReturned)
{
    std::string payload = std::string("ab\0cd", 5);
    storage.put(path, payload);

    auto retrievedData = storage.get(path);
    ASSERT_TRUE(retrievedData.has_value());
    EXPECT_EQ(retrievedData.value(), payload);
    EXPECT_EQ(retrievedData.value().size(), payload.size());
}
