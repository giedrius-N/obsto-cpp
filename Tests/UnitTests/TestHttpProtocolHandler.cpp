#include "HttpProtocolHandler.h"
#include "IObjectStorage.h"
#include "MockObjectStorage.h"
#include <gtest/gtest.h>

class TestHttpProtocolHandler : public ::testing::Test
{
public:
    MockObjectStorage storage;
    HttpProtocolHandler handler{storage};

    std::string sendRequest(const std::string& requestStr)
    {
        handler.onBytesReceived(requestStr);

        auto bytes = handler.takeOutgoingBytes();

        return std::string(
            reinterpret_cast<const char*>(bytes.data()),
            bytes.size()
        );
    }
};

TEST_F(TestHttpProtocolHandler, getRootListsAllKeys)
{
    storage.put("key1", "val1");
    storage.put("key2", "val2");

    auto response = sendRequest("GET / HTTP/1.1\r\nHost: localhost\r\n\r\n");

    EXPECT_NE(response.find("HTTP/1.1 200 OK"), std::string::npos);
    EXPECT_NE(response.find("key1\n"), std::string::npos);
    EXPECT_NE(response.find("key2\n"), std::string::npos);
}

TEST_F(TestHttpProtocolHandler, getExistingPathReturns200AndContent)
{
    storage.put("mykey", "hello world");

    auto response = sendRequest("GET /mykey HTTP/1.1\r\nHost: localhost\r\n\r\n");

    EXPECT_NE(response.find("HTTP/1.1 200 OK"), std::string::npos);
    EXPECT_NE(response.find("Content-Length: 11"), std::string::npos);
    EXPECT_NE(response.find("hello world"), std::string::npos);
}

TEST_F(TestHttpProtocolHandler, getMissingPathReturns404)
{
    auto response = sendRequest("GET /nonexistent HTTP/1.1\r\nHost: localhost\r\n\r\n");

    EXPECT_NE(response.find("HTTP/1.1 404 Not Found"), std::string::npos);
}

TEST_F(TestHttpProtocolHandler, putPathStoresData)
{
    auto response = sendRequest("PUT /mykey HTTP/1.1\r\nHost: localhost\r\nContent-Length: 13\r\n\r\nhello storage");

    EXPECT_NE(response.find("HTTP/1.1 200 OK"), std::string::npos);
    
    auto storedValue = storage.get("mykey");
    ASSERT_TRUE(storedValue.has_value());
    EXPECT_EQ(*storedValue, "hello storage");
}

TEST_F(TestHttpProtocolHandler, putPathOverwritesExistingData)
{
    storage.put("mykey", "old_value");

    auto response = sendRequest("PUT /mykey HTTP/1.1\r\nHost: localhost\r\nContent-Length: 9\r\n\r\nnew_value");

    EXPECT_NE(response.find("HTTP/1.1 200 OK"), std::string::npos);
    
    auto storedValue = storage.get("mykey");
    ASSERT_TRUE(storedValue.has_value());
    EXPECT_EQ(*storedValue, "new_value");
}

TEST_F(TestHttpProtocolHandler, unsupportedMethodReturns405)
{
    auto response = sendRequest("POST /mykey HTTP/1.1\r\nHost: localhost\r\nContent-Length: 4\r\n\r\ntest");

    EXPECT_NE(response.find("HTTP/1.1 405 Method Not Allowed"), std::string::npos);
}

TEST_F(TestHttpProtocolHandler, putReturns507WhenCapacityExceeded)
{
    storage.setNextPutResult(PutResult::CapacityExceeded);

    auto response = sendRequest("PUT /largefile HTTP/1.1\r\nHost: localhost\r\nContent-Length: 5\r\n\r\n12345");

    EXPECT_NE(response.find("HTTP/1.1 507 Insufficient Storage"), std::string::npos);
}

TEST_F(TestHttpProtocolHandler, putReturns400OnInvalidPath)
{
    auto response = sendRequest("PUT / HTTP/1.1\r\nHost: localhost\r\nContent-Length: 4\r\n\r\ndata");

    EXPECT_NE(response.find("HTTP/1.1 400 Bad Request"), std::string::npos);
}

TEST_F(TestHttpProtocolHandler, malformedRequestReturns400)
{
    auto response = sendRequest("INVALID_HTTP_LINE_WITHOUT_HEADERS\r\n\r\n");

    EXPECT_NE(response.find("HTTP/1.1 400 Bad Request"), std::string::npos);
}

TEST_F(TestHttpProtocolHandler, handlesPartialBytesStream)
{
    storage.put("mykey", "chunked data");

    handler.onBytesReceived("GET /my");
    EXPECT_TRUE(handler.takeOutgoingBytes().empty());

    handler.onBytesReceived("key HTTP/1.1\r\nHost: localhost\r\n\r\n");
    auto bytes = handler.takeOutgoingBytes();

    std::string response(reinterpret_cast<const char*>(bytes.data()), bytes.size());

    EXPECT_NE(response.find("HTTP/1.1 200 OK"), std::string::npos);
    EXPECT_NE(response.find("chunked data"), std::string::npos);
}

TEST_F(TestHttpProtocolHandler, deleteExistingPathReturns200)
{
    storage.put("todelete", "some data");

    auto response = sendRequest("DELETE /todelete HTTP/1.1\r\nHost: localhost\r\n\r\n");

    EXPECT_NE(response.find("HTTP/1.1 200 OK"), std::string::npos);
    EXPECT_FALSE(storage.exists("todelete"));
}

TEST_F(TestHttpProtocolHandler, deleteMissingPathReturns404)
{
    auto response = sendRequest("DELETE /nonexistent HTTP/1.1\r\nHost: localhost\r\n\r\n");

    EXPECT_NE(response.find("HTTP/1.1 404 Not Found"), std::string::npos);
}

TEST_F(TestHttpProtocolHandler, deleteNestedPathRemovesOnlyTargetResource)
{
    storage.put("subdir/file.txt", "file content");
    storage.put("subdir/other.txt", "other content");

    auto response = sendRequest("DELETE /subdir/file.txt HTTP/1.1\r\nHost: localhost\r\n\r\n");

    EXPECT_NE(response.find("HTTP/1.1 200 OK"), std::string::npos);
    EXPECT_FALSE(storage.exists("subdir/file.txt"));
    EXPECT_TRUE(storage.exists("subdir/other.txt"));
}
