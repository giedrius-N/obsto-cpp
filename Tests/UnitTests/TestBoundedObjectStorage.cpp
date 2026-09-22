#include "BoundedObjectStorage.h"
#include "ObjectStorage.h"
#include <gtest/gtest.h>
#include <algorithm>

class TestBoundedObjectStorage : public ::testing::Test
{
protected:
    std::unique_ptr<IObjectStorage> innerStorage = std::make_unique<ObjectStorage>();
    BoundedObjectStorage storage{std::move(innerStorage), 100};
};

TEST_F(TestBoundedObjectStorage, putAndGet)
{
    std::string path = "test/path";
    std::string data = "testing data";
    EXPECT_EQ(storage.put(path, data), PutResult::Ok);
    auto retrievedData = storage.get(path);

    ASSERT_TRUE(retrievedData.has_value());
    EXPECT_EQ(retrievedData.value(), data);
}

TEST_F(TestBoundedObjectStorage, putExceedingCapacity)
{
    std::string path = "test/path";
    std::string data(200, 'x'); // 200 bytes of data
    EXPECT_EQ(storage.put(path, data), PutResult::CapacityExceeded);
    EXPECT_FALSE(storage.exists(path));
}

TEST_F(TestBoundedObjectStorage, putWithinCapacity)
{
    std::string path = "test/path";
    std::string data(50, 'x');
    EXPECT_EQ(storage.put(path, data), PutResult::Ok);
    EXPECT_TRUE(storage.exists(path));
}

TEST_F(TestBoundedObjectStorage, delReducesCurrentBytes)
{
    std::string path = "test/path";
    std::string data(50, 'x');
    EXPECT_EQ(storage.put(path, data), PutResult::Ok);
    EXPECT_TRUE(storage.exists(path));
    EXPECT_TRUE(storage.del(path));
    EXPECT_FALSE(storage.exists(path));
}

TEST_F(TestBoundedObjectStorage, setnxWithinCapacity)
{
    std::string path = "test/path";
    std::string data(50, 'x');
    EXPECT_EQ(storage.setnx(path, data), SetNxResult::Inserted);
    EXPECT_TRUE(storage.exists(path));
}
