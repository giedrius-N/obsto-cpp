#include "RespProtocolHandler.h"
#include "IObjectStorage.h"
#include "MockObjectStorage.h"
#include <gtest/gtest.h>
#include <unordered_map>


class TestRespProtocolHandler : public ::testing::Test
{
public:

    MockObjectStorage storage;
    RespProtocolHandler handler{storage};

    std::string sendCommand(const std::string& command)
    {
        handler.onBytesReceived(command);

        auto bytes = handler.takeOutgoingBytes();

        return std::string(
            reinterpret_cast<const char*>(bytes.data()),
            bytes.size()
        );
    }
};

TEST_F(TestRespProtocolHandler, pingReturnsPong)
{
    auto response = sendCommand("*1\r\n$4\r\nPING\r\n");
    EXPECT_EQ(response, "+PONG\r\n");
}

TEST_F(TestRespProtocolHandler, pingWithArgumentReturnsArgument)
{
    auto response = sendCommand("*2\r\n$4\r\nPING\r\n$5\r\nhello\r\n");
    EXPECT_EQ(response, "+hello\r\n");
}

TEST_F(TestRespProtocolHandler, setStoresValue)
{
    auto response = sendCommand("*3\r\n$3\r\nSET\r\n$5\r\nmykey\r\n$5\r\nvalue\r\n");
    EXPECT_EQ(response, "+OK\r\n");
    EXPECT_EQ(storage.get("mykey").value(), "value");
}

TEST_F(TestRespProtocolHandler, getReturnsExistingValue)
{
    storage.put("mykey", "hello");
    auto response = sendCommand("*2\r\n$3\r\nGET\r\n$5\r\nmykey\r\n");
    EXPECT_EQ(response, "$5\r\nhello\r\n");
}

TEST_F(TestRespProtocolHandler, getMissingKeyReturnsNil)
{
    auto response = sendCommand("*2\r\n$3\r\nGET\r\n$7\r\nmissing\r\n");
    EXPECT_EQ(response, "$-1\r\n");
}

TEST_F(TestRespProtocolHandler, setNxInsertsNewKey)
{
    auto response = sendCommand("*3\r\n$5\r\nSETNX\r\n$5\r\nmykey\r\n$5\r\nvalue\r\n");
    EXPECT_EQ(response, ":1\r\n");
}

TEST_F(TestRespProtocolHandler, setNxDoesNotOverwriteExistingKey)
{
    storage.put("mykey", "old");
    auto response = sendCommand("*3\r\n$5\r\nSETNX\r\n$5\r\nmykey\r\n$5\r\nvalue\r\n");
    EXPECT_EQ(response, ":0\r\n");
    EXPECT_EQ(storage.get("mykey").value(), "old");
}

TEST_F(TestRespProtocolHandler, unknownCommandReturnsError)
{
    auto response = sendCommand("*1\r\n$3\r\nABC\r\n");
    EXPECT_EQ(response, "-ERR unknown command 'ABC'\r\n");
}

TEST_F(TestRespProtocolHandler, setWithWrongArgumentsReturnsError)
{
    auto response = sendCommand("*2\r\n$3\r\nSET\r\n$5\r\nmykey\r\n");
    EXPECT_EQ(response, "-ERR wrong number of arguments for 'set' command\r\n");
}

TEST_F(TestRespProtocolHandler, keysReturnsStoredKeys)
{
    storage.put("one", "1");
    storage.put("two", "2");

    auto response = sendCommand("*1\r\n$4\r\nKEYS\r\n");
    EXPECT_TRUE(response.find("*2\r\n") != std::string::npos);
    EXPECT_TRUE(response.find("one") != std::string::npos);
    EXPECT_TRUE(response.find("two") != std::string::npos);
}

TEST_F(TestRespProtocolHandler, delRemovesKey)
{
    storage.put("todelete", "data");

    auto response = sendCommand("*2\r\n$3\r\nDEL\r\n$8\r\ntodelete\r\n");
    EXPECT_EQ(response, ":1\r\n");
    EXPECT_FALSE(storage.exists("todelete"));
}

TEST_F(TestRespProtocolHandler, delNonExistentKeyReturnsZero)
{
    auto response = sendCommand("*2\r\n$3\r\nDEL\r\n$7\r\nmissing\r\n");
    EXPECT_EQ(response, ":0\r\n");
}

TEST_F(TestRespProtocolHandler, delWithWrongArgumentsReturnsError)
{
    auto response = sendCommand("*1\r\n$3\r\nDEL\r\n");
    EXPECT_EQ(response, "-ERR wrong number of arguments for 'del' command\r\n");
}
