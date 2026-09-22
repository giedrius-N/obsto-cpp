#include "HttpParser.h"
#include <gtest/gtest.h>
#include <string>

class TestHttpParser : public ::testing::Test
{
protected:
    http::HttpRequest request;
    size_t bytesConsumed = 0;

    void reset()
    {
        request = http::HttpRequest{};
        bytesConsumed = 0;
    }

    bool parseComplete(const std::string& buffer)
    {
        auto result = HttpParser::parse(buffer, request, bytesConsumed);
        return result == HttpParser::ParseResult::Complete;
    }

    bool parseIncomplete(const std::string& buffer)
    {
        auto result = HttpParser::parse(buffer, request, bytesConsumed);
        return result == HttpParser::ParseResult::Incomplete;
    }

    bool parseError(const std::string& buffer)
    {
        auto result = HttpParser::parse(buffer, request, bytesConsumed);
        return result == HttpParser::ParseResult::Error;
    }
};

TEST_F(TestHttpParser, parseSimpleGetRoot)
{
    std::string buffer = "GET / HTTP/1.1\r\nHost: localhost\r\n\r\n";

    EXPECT_TRUE(parseComplete(buffer));
    EXPECT_EQ(bytesConsumed, buffer.size());
    EXPECT_EQ(request.method, "GET");
    EXPECT_EQ(request.path, "/");
    EXPECT_EQ(request.version, "HTTP/1.1");
    EXPECT_EQ(request.headers.at("host"), "localhost");
}

TEST_F(TestHttpParser, parseGetWithPath)
{
    std::string buffer = "GET /myobject HTTP/1.1\r\nHost: localhost\r\n\r\n";

    EXPECT_TRUE(parseComplete(buffer));
    EXPECT_EQ(request.method, "GET");
    EXPECT_EQ(request.path, "/myobject");
}

TEST_F(TestHttpParser, headerNamesAreLowercasedForLookup)
{
    std::string buffer = "GET / HTTP/1.1\r\nHOST: localhost\r\nX-Custom-Header: value\r\n\r\n";

    EXPECT_TRUE(parseComplete(buffer));
    EXPECT_EQ(request.headers.at("host"), "localhost");
    EXPECT_EQ(request.headers.at("x-custom-header"), "value");
}

TEST_F(TestHttpParser, parsePutWithBody)
{
    std::string buffer = "PUT /path HTTP/1.1\r\nContent-Length: 5\r\n\r\nhello";

    EXPECT_TRUE(parseComplete(buffer));
    EXPECT_EQ(bytesConsumed, buffer.size());
    EXPECT_EQ(request.method, "PUT");
    EXPECT_EQ(request.path, "/path");
    EXPECT_EQ(request.body, "hello");
}

TEST_F(TestHttpParser, noContentLengthMeansEmptyBody)
{
    std::string buffer = "GET / HTTP/1.1\r\nHost: localhost\r\n\r\n";

    EXPECT_TRUE(parseComplete(buffer));
    EXPECT_EQ(request.body, "");
    EXPECT_EQ(bytesConsumed, buffer.size());
}

TEST_F(TestHttpParser, contentLengthZeroMeansEmptyBody)
{
    std::string buffer = "PUT /path HTTP/1.1\r\nContent-Length: 0\r\n\r\n";

    EXPECT_TRUE(parseComplete(buffer));
    EXPECT_EQ(request.body, "");
    EXPECT_EQ(bytesConsumed, buffer.size());
}

TEST_F(TestHttpParser, bodyContainingBinaryDataIsPreserved)
{
    std::string body("ab\0cd", 5);
    std::string buffer = "PUT /path HTTP/1.1\r\nContent-Length: " + std::to_string(body.size()) + "\r\n\r\n" + body;

    EXPECT_TRUE(parseComplete(buffer));
    EXPECT_EQ(request.body, body);
    EXPECT_EQ(request.body.size(), 5);
}

TEST_F(TestHttpParser, missingHeaderTerminatorIsIncomplete)
{
    std::string buffer = "GET / HTTP/1.1\r\nHost: localhost\r\n";

    EXPECT_TRUE(parseIncomplete(buffer));
    EXPECT_EQ(bytesConsumed, 0);
}

TEST_F(TestHttpParser, bodyNotFullyArrivedIsIncomplete)
{
    std::string buffer = "PUT /path HTTP/1.1\r\nContent-Length: 10\r\n\r\nhel";

    EXPECT_TRUE(parseIncomplete(buffer));
    EXPECT_EQ(bytesConsumed, 0);
}

TEST_F(TestHttpParser, emptyBufferIsIncomplete)
{
    std::string buffer = "";

    EXPECT_TRUE(parseIncomplete(buffer));
    EXPECT_EQ(bytesConsumed, 0);
}

TEST_F(TestHttpParser, malformedRequestLineIsError)
{
    std::string buffer = "GARBAGE\r\n\r\n";

    EXPECT_TRUE(parseError(buffer));
}

TEST_F(TestHttpParser, headerLineWithoutColonIsError)
{
    std::string buffer = "GET / HTTP/1.1\r\nNotAValidHeaderLine\r\n\r\n";

    EXPECT_TRUE(parseError(buffer));
}

TEST_F(TestHttpParser, negativeContentLengthIsError)
{
    std::string buffer = "PUT /path HTTP/1.1\r\nContent-Length: -5\r\n\r\n";

    EXPECT_TRUE(parseError(buffer));
}

TEST_F(TestHttpParser, nonNumericContentLengthIsError)
{
    std::string buffer = "PUT /path HTTP/1.1\r\nContent-Length: notanumber\r\n\r\n";

    EXPECT_TRUE(parseError(buffer));
}

TEST_F(TestHttpParser, largeContentLengthIsErrorNotHang)
{
    std::string buffer = "PUT /path HTTP/1.1\r\nContent-Length: 999999999999999999\r\n\r\n";

    EXPECT_TRUE(parseError(buffer));
}

TEST_F(TestHttpParser, oversizedHeaderSectionIsErrorNotHang)
{
    std::string hugeHeaders = "GET / HTTP/1.1\r\n";
    hugeHeaders += std::string(20 * 1024, 'x');

    EXPECT_TRUE(parseError(hugeHeaders));
}

TEST_F(TestHttpParser, sameBufferReparsedAfterMoreBytesArriveSucceeds)
{
    std::string partial = "GET / HTTP/1.1\r\nHost: localhost\r\n";
    EXPECT_TRUE(parseIncomplete(partial));

    std::string complete = partial + "\r\n";
    EXPECT_TRUE(parseComplete(complete));
    EXPECT_EQ(request.method, "GET");
}
