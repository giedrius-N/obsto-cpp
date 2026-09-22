#include "ProtocolDetectorHandler.h"
#include "ObjectStorage.h"
#include "BoundedObjectStorage.h"
#include <gtest/gtest.h>
#include <string>

class TestProtocolDetectorHandler : public ::testing::Test
{
protected:
    BoundedObjectStorage storage{std::make_unique<ObjectStorage>(), 100 * 1024 * 1024};
    ProtocolDetectorHandler detector {storage};

    static std::string toString(const std::vector<std::byte>& bytes)
    {
        return std::string(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    }
};

TEST_F(TestProtocolDetectorHandler, emptyDataProducesNoReplyYet)
{
    EXPECT_TRUE(detector.onBytesReceived(""));
    EXPECT_TRUE(detector.takeOutgoingBytes().empty());
}

// To be filled when protocol detection is implemented