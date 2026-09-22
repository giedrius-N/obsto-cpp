#include "RespParser.h"
#include <gtest/gtest.h>
#include <vector>
#include <string>

using namespace resp;

class TestRespParser : public ::testing::Test
{
protected:
    std::vector<std::string> commands;
    size_t bytesConsumed = 0;
    
    void reset()
    {
        commands.clear();
        bytesConsumed = 0;
    }
    
    bool parseComplete(const std::string& buffer)
    {
        auto result = RespParser::parse(buffer, commands, bytesConsumed);
        return result == RespParser::ParseResult::Complete;
    }
    
    bool parseIncomplete(const std::string& buffer)
    {
        auto result = RespParser::parse(buffer, commands, bytesConsumed);
        return result == RespParser::ParseResult::Incomplete;
    }
};

TEST_F(TestRespParser, parseArrayWithPing)
{
    std::string buffer = "*1\r\n$4\r\nPING\r\n";
    
    EXPECT_TRUE(parseComplete(buffer));
    EXPECT_EQ(bytesConsumed, buffer.size());
    ASSERT_EQ(commands.size(), 1);
    EXPECT_EQ(commands[0], "PING");
}

TEST_F(TestRespParser, parseArrayWithGet)
{
    std::string buffer = "*2\r\n$3\r\nGET\r\n$5\r\nmykey\r\n";
    
    EXPECT_TRUE(parseComplete(buffer));
    EXPECT_EQ(bytesConsumed, buffer.size());
    ASSERT_EQ(commands.size(), 2);
    EXPECT_EQ(commands[0], "GET");
    EXPECT_EQ(commands[1], "mykey");
}

TEST_F(TestRespParser, parseArrayWithSet)
{
    std::string buffer = "*3\r\n$3\r\nSET\r\n$3\r\nkey\r\n$5\r\nvalue\r\n";
    
    EXPECT_TRUE(parseComplete(buffer));
    EXPECT_EQ(bytesConsumed, buffer.size());
    ASSERT_EQ(commands.size(), 3);
    EXPECT_EQ(commands[0], "SET");
    EXPECT_EQ(commands[1], "key");
    EXPECT_EQ(commands[2], "value");
}

TEST_F(TestRespParser, parseArrayWithDel)
{
    std::string buffer = "*2\r\n$3\r\nDEL\r\n$3\r\nkey\r\n";
    
    EXPECT_TRUE(parseComplete(buffer));
    EXPECT_EQ(bytesConsumed, buffer.size());
    ASSERT_EQ(commands.size(), 2);
    EXPECT_EQ(commands[0], "DEL");
    EXPECT_EQ(commands[1], "key");
}

TEST_F(TestRespParser, parseArrayWithKeys)
{
    std::string buffer = "*2\r\n$4\r\nKEYS\r\n$1\r\n*\r\n";
    
    EXPECT_TRUE(parseComplete(buffer));
    EXPECT_EQ(bytesConsumed, buffer.size());
    ASSERT_EQ(commands.size(), 2);
    EXPECT_EQ(commands[0], "KEYS");
    EXPECT_EQ(commands[1], "*");
}

TEST_F(TestRespParser, parseArrayWithSetNx)
{
    std::string buffer = "*3\r\n$5\r\nSETNX\r\n$5\r\nmykey\r\n$20\r\nhello object storage\r\n";
    
    EXPECT_TRUE(parseComplete(buffer));
    EXPECT_EQ(bytesConsumed, buffer.size());
    ASSERT_EQ(commands.size(), 3);
    EXPECT_EQ(commands[0], "SETNX");
    EXPECT_EQ(commands[1], "mykey");
    EXPECT_EQ(commands[2], "hello object storage");
}

TEST_F(TestRespParser, parseArrayWithMultipleElements)
{
    std::string buffer = "*4\r\n$3\r\nCMD\r\n$4\r\narg1\r\n$4\r\narg2\r\n$4\r\narg3\r\n";
    
    EXPECT_TRUE(parseComplete(buffer));
    EXPECT_EQ(bytesConsumed, buffer.size());
    ASSERT_EQ(commands.size(), 4);
    EXPECT_EQ(commands[0], "CMD");
    EXPECT_EQ(commands[1], "arg1");
    EXPECT_EQ(commands[2], "arg2");
    EXPECT_EQ(commands[3], "arg3");
}

TEST_F(TestRespParser, parseArrayWithMixedTypes)
{
    std::string buffer = "*3\r\n$3\r\nSET\r\n+key\r\n:42\r\n";
    
    EXPECT_TRUE(parseComplete(buffer));
    EXPECT_EQ(bytesConsumed, buffer.size());
    ASSERT_EQ(commands.size(), 3);
    EXPECT_EQ(commands[0], "SET");
    EXPECT_EQ(commands[1], "key");
    EXPECT_EQ(commands[2], "42");
}

TEST_F(TestRespParser, parseArrayWithEmptyElements)
{
    std::string buffer = "*2\r\n$0\r\n\r\n$0\r\n\r\n";
    
    EXPECT_TRUE(parseComplete(buffer));
    EXPECT_EQ(bytesConsumed, buffer.size());
    ASSERT_EQ(commands.size(), 2);
    EXPECT_EQ(commands[0], "");
    EXPECT_EQ(commands[1], "");
}

TEST_F(TestRespParser, parseBulkString)
{
    std::string buffer = "$5\r\nhello\r\n";
    
    EXPECT_TRUE(parseComplete(buffer));
    EXPECT_EQ(bytesConsumed, buffer.size());
    ASSERT_EQ(commands.size(), 1);
    EXPECT_EQ(commands[0], "hello");
}

TEST_F(TestRespParser, parseBulkStringWithEmptyData)
{
    std::string buffer = "$0\r\n\r\n";
    
    EXPECT_TRUE(parseComplete(buffer));
    EXPECT_EQ(bytesConsumed, buffer.size());
    ASSERT_EQ(commands.size(), 1);
    EXPECT_EQ(commands[0], "");
}

TEST_F(TestRespParser, parseBulkStringWithBinaryData)
{
    std::string data("ab\0cd\0ef", 8);
    std::string buffer = "$" + std::to_string(data.size()) + "\r\n" + data + "\r\n";

    EXPECT_TRUE(parseComplete(buffer));
    EXPECT_EQ(bytesConsumed, buffer.size());
    ASSERT_EQ(commands.size(), 1);
    EXPECT_EQ(commands[0], data);
    EXPECT_EQ(commands[0].size(), 8);
}

TEST_F(TestRespParser, parseBulkStringIncomplete)
{
    std::string buffer = "$5\r\nhel";
    
    EXPECT_TRUE(parseIncomplete(buffer));
    EXPECT_EQ(bytesConsumed, 0);
    EXPECT_TRUE(commands.empty());
}

TEST_F(TestRespParser, parseBulkStringWithNegativeLength)
{
    std::string buffer = "$-1\r\n";
    
    EXPECT_TRUE(parseComplete(buffer));
    EXPECT_EQ(bytesConsumed, buffer.size());
    ASSERT_EQ(commands.size(), 1);
    EXPECT_EQ(commands[0], "");
}

TEST_F(TestRespParser, parseBulkStringWithLargeData)
{
    std::string largeData(1000, 'x');
    std::string buffer = "$" + std::to_string(largeData.size()) + "\r\n" + largeData + "\r\n";
    
    EXPECT_TRUE(parseComplete(buffer));
    EXPECT_EQ(bytesConsumed, buffer.size());
    ASSERT_EQ(commands.size(), 1);
    EXPECT_EQ(commands[0], largeData);
}

TEST_F(TestRespParser, parseSimpleString)
{
    std::string buffer = "+OK\r\n";
    
    EXPECT_TRUE(parseComplete(buffer));
    EXPECT_EQ(bytesConsumed, buffer.size());
    ASSERT_EQ(commands.size(), 1);
    EXPECT_EQ(commands[0], "OK");
}

TEST_F(TestRespParser, parseSimpleStringWithSpaces)
{
    std::string buffer = "+Hello World\r\n";
    
    EXPECT_TRUE(parseComplete(buffer));
    EXPECT_EQ(bytesConsumed, buffer.size());
    ASSERT_EQ(commands.size(), 1);
    EXPECT_EQ(commands[0], "Hello World");
}

TEST_F(TestRespParser, parseSimpleStringWithEmpty)
{
    std::string buffer = "+\r\n";
    
    EXPECT_TRUE(parseComplete(buffer));
    EXPECT_EQ(bytesConsumed, buffer.size());
    ASSERT_EQ(commands.size(), 1);
    EXPECT_EQ(commands[0], "");
}

TEST_F(TestRespParser, parseSimpleStringIncomplete)
{
    std::string buffer = "+OK";
    
    EXPECT_TRUE(parseIncomplete(buffer));
    EXPECT_EQ(bytesConsumed, 0);
    EXPECT_TRUE(commands.empty());
}

TEST_F(TestRespParser, parseError)
{
    std::string buffer = "-ERR unknown command\r\n";
    
    EXPECT_TRUE(parseComplete(buffer));
    EXPECT_EQ(bytesConsumed, buffer.size());
    ASSERT_EQ(commands.size(), 1);
    EXPECT_EQ(commands[0], "ERROR:ERR unknown command");
}

TEST_F(TestRespParser, parseErrorWithEmptyMessage)
{
    std::string buffer = "-\r\n";
    
    EXPECT_TRUE(parseComplete(buffer));
    EXPECT_EQ(bytesConsumed, buffer.size());
    ASSERT_EQ(commands.size(), 1);
    EXPECT_EQ(commands[0], "ERROR:");
}

TEST_F(TestRespParser, parseInteger)
{
    std::string buffer = ":42\r\n";
    
    EXPECT_TRUE(parseComplete(buffer));
    EXPECT_EQ(bytesConsumed, buffer.size());
    ASSERT_EQ(commands.size(), 1);
    EXPECT_EQ(commands[0], "INT:42");
}

TEST_F(TestRespParser, parseIntegerWithZero)
{
    std::string buffer = ":0\r\n";
    
    EXPECT_TRUE(parseComplete(buffer));
    EXPECT_EQ(bytesConsumed, buffer.size());
    ASSERT_EQ(commands.size(), 1);
    EXPECT_EQ(commands[0], "INT:0");
}

TEST_F(TestRespParser, parseIntegerWithNegative)
{
    std::string buffer = ":-1\r\n";
    
    EXPECT_TRUE(parseComplete(buffer));
    EXPECT_EQ(bytesConsumed, buffer.size());
    ASSERT_EQ(commands.size(), 1);
    EXPECT_EQ(commands[0], "INT:-1");
}

TEST_F(TestRespParser, parseIntegerWithLargeValue)
{
    std::string buffer = ":1000000\r\n";
    
    EXPECT_TRUE(parseComplete(buffer));
    EXPECT_EQ(bytesConsumed, buffer.size());
    ASSERT_EQ(commands.size(), 1);
    EXPECT_EQ(commands[0], "INT:1000000");
}

TEST_F(TestRespParser, parseRawPing)
{
    std::string buffer = "PING\r\n";
    
    EXPECT_TRUE(parseComplete(buffer));
    EXPECT_EQ(bytesConsumed, buffer.size());
    ASSERT_EQ(commands.size(), 1);
    EXPECT_EQ(commands[0], "PING");
}

TEST_F(TestRespParser, parseRawGet)
{
    std::string buffer = "GET mykey\r\n";
    
    EXPECT_TRUE(parseComplete(buffer));
    EXPECT_EQ(bytesConsumed, buffer.size());
    ASSERT_EQ(commands.size(), 1);
    EXPECT_EQ(commands[0], "GET mykey");
}

TEST_F(TestRespParser, parseRawIncomplete)
{
    std::string buffer = "PING";
    
    EXPECT_TRUE(parseIncomplete(buffer));
    EXPECT_EQ(bytesConsumed, 0);
    EXPECT_TRUE(commands.empty());
}

TEST_F(TestRespParser, parseEmptyBuffer)
{
    std::string buffer = "";
    
    EXPECT_TRUE(parseIncomplete(buffer));
    EXPECT_EQ(bytesConsumed, 0);
    EXPECT_TRUE(commands.empty());
}

TEST_F(TestRespParser, parseBufferWithOnlyNewline)
{
    std::string buffer = "\r\n";
    
    EXPECT_TRUE(parseComplete(buffer));
    EXPECT_EQ(bytesConsumed, buffer.size());
    ASSERT_EQ(commands.size(), 1);
    EXPECT_EQ(commands[0], "");
}

TEST_F(TestRespParser, parseBufferWithMultipleCommands)
{
    std::string buffer = "*1\r\n$4\r\nPING\r\n*1\r\n$4\r\nPING\r\n";
    
    EXPECT_TRUE(parseComplete(buffer));
    EXPECT_LT(bytesConsumed, buffer.size());
    ASSERT_EQ(commands.size(), 1);
    EXPECT_EQ(commands[0], "PING");
}

TEST_F(TestRespParser, parseBufferWithLeadingWhitespace)
{
    std::string buffer = "  PING\r\n";
    
    EXPECT_TRUE(parseComplete(buffer));
    EXPECT_EQ(bytesConsumed, buffer.size());
    ASSERT_EQ(commands.size(), 1);
    EXPECT_EQ(commands[0], "  PING");
}

TEST_F(TestRespParser, parseBufferWithNullBytes)
{
    std::string expected("ab\0cd", 5);
    std::string buffer = "$" + std::to_string(expected.size()) + "\r\n" + expected + "\r\n";

    EXPECT_TRUE(parseComplete(buffer));
    EXPECT_EQ(bytesConsumed, buffer.size());
    ASSERT_EQ(commands.size(), 1);
    EXPECT_EQ(commands[0], expected);
    EXPECT_EQ(commands[0].size(), 5);
}

TEST_F(TestRespParser, parseBufferWithUnicode)
{
    std::string buffer = "$12\r\nHello 世界\r\n";
    
    EXPECT_TRUE(parseComplete(buffer));
    EXPECT_EQ(bytesConsumed, buffer.size());
    ASSERT_EQ(commands.size(), 1);
    EXPECT_EQ(commands[0], "Hello 世界");
}

TEST_F(TestRespParser, parseVeryLargeArray)
{
    std::string buffer = "*1000\r\n";
    for (int i = 0; i < 1000; i++) {
        buffer += "$" + std::to_string(std::to_string(i).size()) + "\r\n" + std::to_string(i) + "\r\n";
    }
    
    EXPECT_TRUE(parseComplete(buffer));
    EXPECT_EQ(bytesConsumed, buffer.size());
    ASSERT_EQ(commands.size(), 1000);
    EXPECT_EQ(commands[0], "0");
    EXPECT_EQ(commands[999], "999");
}

TEST_F(TestRespParser, parsePingResponse)
{
    std::string buffer = "+PONG\r\n";
    
    EXPECT_TRUE(parseComplete(buffer));
    ASSERT_EQ(commands.size(), 1);
    EXPECT_EQ(commands[0], "PONG");
}

TEST_F(TestRespParser, parseBulkStringResponse)
{
    std::string buffer = "$5\r\nHello\r\n";
    
    EXPECT_TRUE(parseComplete(buffer));
    ASSERT_EQ(commands.size(), 1);
    EXPECT_EQ(commands[0], "Hello");
}

TEST_F(TestRespParser, parseNullBulkStringResponse)
{
    std::string buffer = "$-1\r\n";
    
    EXPECT_TRUE(parseComplete(buffer));
    ASSERT_EQ(commands.size(), 1);
    EXPECT_EQ(commands[0], "");
}

TEST_F(TestRespParser, parseArrayResponse)
{
    std::string buffer = "*2\r\n$3\r\nfoo\r\n$3\r\nbar\r\n";
    
    EXPECT_TRUE(parseComplete(buffer));
    ASSERT_EQ(commands.size(), 2);
    EXPECT_EQ(commands[0], "foo");
    EXPECT_EQ(commands[1], "bar");
}

TEST_F(TestRespParser, parseErrorResponse)
{
    std::string buffer = "-ERR something went wrong\r\n";
    
    EXPECT_TRUE(parseComplete(buffer));
    ASSERT_EQ(commands.size(), 1);
    EXPECT_EQ(commands[0], "ERROR:ERR something went wrong");
}

TEST_F(TestRespParser, parseIntResponse)
{
    std::string buffer = ":100\r\n";
    
    EXPECT_TRUE(parseComplete(buffer));
    ASSERT_EQ(commands.size(), 1);
    EXPECT_EQ(commands[0], "INT:100");
}

TEST_F(TestRespParser, extractCommandFromArray)
{
    std::string buffer = "*2\r\n$3\r\nGET\r\n$5\r\nmykey\r\n";
    
    EXPECT_TRUE(parseComplete(buffer));
    ASSERT_EQ(commands.size(), 2);
    EXPECT_EQ(commands[0], "GET");
    EXPECT_EQ(commands[1], "mykey");
}

TEST_F(TestRespParser, extractCommandFromBulkString)
{
    std::string buffer = "$4\r\nPING\r\n";
    
    EXPECT_TRUE(parseComplete(buffer));
    ASSERT_EQ(commands.size(), 1);
    EXPECT_EQ(commands[0], "PING");
}

TEST_F(TestRespParser, extractCommandFromSimpleString)
{
    std::string buffer = "+PING\r\n";
    
    EXPECT_TRUE(parseComplete(buffer));
    ASSERT_EQ(commands.size(), 1);
    EXPECT_EQ(commands[0], "PING");
}

TEST_F(TestRespParser, maxArrayElementsLimit)
{
    std::string buffer = "*100001\r\n";
    for (int i = 0; i < 100001; i++) {
        buffer += "$1\r\na\r\n";
    }
    
    auto result = RespParser::parse(buffer, commands, bytesConsumed);
    EXPECT_EQ(result, RespParser::ParseResult::Error);
    EXPECT_TRUE(commands.empty());
}
