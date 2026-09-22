#include "Server.h"
#include "ProtocolDetectorHandler.h"
#include "BoundedObjectStorage.h"
#include "DiskObjectStorage.h"
#include <gtest/gtest.h>
#include <asio.hpp>
#include <thread>
#include <filesystem>

namespace fs = std::filesystem;

class TestIntegration : public ::testing::Test
{
protected:
    fs::path testDir;
    std::unique_ptr<DiskObjectStorage> diskStorage;
    std::unique_ptr<BoundedObjectStorage> storage;
    std::unique_ptr<asio::io_context> ioContext;
    std::unique_ptr<Server> server;
    std::thread serverThread;
    unsigned short port = 0;

    void SetUp() override
    {
        testDir = fs::temp_directory_path() / ("obsto_integration_" + std::to_string(std::rand()));
        fs::remove_all(testDir);

        auto disk = std::make_unique<DiskObjectStorage>(testDir);
        storage = std::make_unique<BoundedObjectStorage>(std::move(disk), 50 * 1024 * 1024); // 50 MB cap

        ioContext = std::make_unique<asio::io_context>();
        port = 15000 + (std::rand() % 5000);

        server = std::make_unique<Server>(*ioContext, port, [this]()
        {
            return std::make_unique<ProtocolDetectorHandler>(*storage);
        });

        serverThread = std::thread([this] { ioContext->run(); });
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    void TearDown() override
    {
        ioContext->stop();
        if (serverThread.joinable()) serverThread.join();
        std::error_code ec;
        fs::remove_all(testDir, ec);
    }

    std::string sendRaw(const std::string& data, int readTimeoutMs = 500)
    {
        asio::io_context clientCtx;
        asio::ip::tcp::socket socket(clientCtx);
        socket.connect(asio::ip::tcp::endpoint(asio::ip::address::from_string("127.0.0.1"), port));

        std::error_code writeEc;
        asio::write(socket, asio::buffer(data), writeEc);

        std::string response;
        std::array<char, 4096> buf;
        std::error_code ec;
        socket.non_blocking(true);

        auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(readTimeoutMs);
        while (std::chrono::steady_clock::now() < deadline)
        {
            size_t n = socket.read_some(asio::buffer(buf), ec);
            if (n > 0) response.append(buf.data(), n);
            if (ec == asio::error::would_block)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }
            if (ec) break;
        }
        return response;
    }

    static std::string respBulkString(const std::string& s)
    {
        return "$" + std::to_string(s.size()) + "\r\n" + s + "\r\n";
    }

    static std::string respArray(std::initializer_list<std::string> elements)
    {
        std::string result = "*" + std::to_string(elements.size()) + "\r\n";
        for (const auto& e : elements)
        {
            result += respBulkString(e);
        }
        return result;
    }
};

TEST_F(TestIntegration, putViaHttpGetViaResp)
{
    std::string body = "hello from http";
    std::string putRequest = "PUT /myobj HTTP/1.1\r\nHost: localhost\r\nContent-Length: "
        + std::to_string(body.size()) + "\r\n\r\n" + body;

    auto putResponse = sendRaw(putRequest);
    EXPECT_TRUE(putResponse.find("200") != std::string::npos);

    std::string getCmd = "*2\r\n$3\r\nGET\r\n$5\r\nmyobj\r\n";
    auto getResponse = sendRaw(getCmd);
    EXPECT_EQ(getResponse, "$15\r\nhello from http\r\n");
}

TEST_F(TestIntegration, putViaRespGetViaHttp)
{
    std::string value = "hello from resp";
    std::string setCmd = "*3\r\n$3\r\nSET\r\n$6\r\nmyobj2\r\n$" + std::to_string(value.size()) + "\r\n" + value + "\r\n";
    sendRaw(setCmd);

    auto httpResponse = sendRaw("GET /myobj2 HTTP/1.1\r\nHost: localhost\r\n\r\n");
    EXPECT_TRUE(httpResponse.find("200") != std::string::npos);
    EXPECT_TRUE(httpResponse.find(value) != std::string::npos);
}

TEST_F(TestIntegration, deleteViaHttpGetViaResp)
{
    std::string value = "to be deleted";
    sendRaw(respArray({"SET", "delete1", value}));

    auto delResponse = sendRaw("DELETE /delete1 HTTP/1.1\r\nHost: localhost\r\n\r\n");
    EXPECT_TRUE(delResponse.find("200") != std::string::npos);

    auto getResponse = sendRaw(respArray({"GET", "delete1"}));
    EXPECT_EQ(getResponse, "$-1\r\n");
}

TEST_F(TestIntegration, deleteViaRespGetViaHttp)
{
    std::string value = "to be deleted";
    sendRaw(respArray({"SET", "delete2", value}));

    auto delResponse = sendRaw(respArray({"DEL", "delete2"}));
    EXPECT_EQ(delResponse, ":1\r\n");

    auto getResponse = sendRaw("GET /delete2 HTTP/1.1\r\nHost: localhost\r\n\r\n");
    EXPECT_TRUE(getResponse.find("404") != std::string::npos);
}

TEST_F(TestIntegration, putViaHttpDeleteViaResp)
{
    std::string body = "to be deleted via resp";
    std::string putRequest = "PUT /delete3 HTTP/1.1\r\nHost: localhost\r\nContent-Length: "
        + std::to_string(body.size()) + "\r\n\r\n" + body;

    auto putResponse = sendRaw(putRequest);
    EXPECT_TRUE(putResponse.find("200") != std::string::npos);

    auto delResponse = sendRaw("*2\r\n$3\r\nDEL\r\n$7\r\ndelete3\r\n");
    EXPECT_EQ(delResponse, ":1\r\n");

    auto getResponse = sendRaw("GET /delete3 HTTP/1.1\r\nHost: localhost\r\n\r\n");
    EXPECT_TRUE(getResponse.find("404") != std::string::npos);
}

TEST_F(TestIntegration, httpPutThenRespSetNxOnSameKeyIsRejected)
{
    std::string body = "original http value";
    std::string putRequest = "PUT /shared HTTP/1.1\r\nHost: localhost\r\nContent-Length: "
        + std::to_string(body.size()) + "\r\n\r\n" + body;
    auto putResponse = sendRaw(putRequest);
    EXPECT_TRUE(putResponse.find("200") != std::string::npos);

    // SETNX via RESP on a key that already exists (created via HTTP) must fail
    std::string setNxCmd = "*3\r\n$5\r\nSETNX\r\n$6\r\nshared\r\n$9\r\nnew_value\r\n";
    auto setNxResponse = sendRaw(setNxCmd);
    EXPECT_EQ(setNxResponse, ":0\r\n");

    // Confirm the original HTTP-stored value was untouched
    auto getResponse = sendRaw("*2\r\n$3\r\nGET\r\n$6\r\nshared\r\n");
    EXPECT_EQ(getResponse, "$19\r\noriginal http value\r\n");
}

TEST_F(TestIntegration, respSetThenHttpPutOverwritesSameKey)
{
    std::string respValue = "set via resp";
    std::string setCmd = "*3\r\n$3\r\nSET\r\n$9\r\novrwrtkey\r\n$"
        + std::to_string(respValue.size()) + "\r\n" + respValue + "\r\n";
    auto setResponse = sendRaw(setCmd);
    EXPECT_EQ(setResponse, "+OK\r\n");

    std::string httpValue = "overwritten via http";
    std::string putRequest = "PUT /ovrwrtkey HTTP/1.1\r\nHost: localhost\r\nContent-Length: "
        + std::to_string(httpValue.size()) + "\r\n\r\n" + httpValue;
    auto putResponse = sendRaw(putRequest);
    EXPECT_TRUE(putResponse.find("200") != std::string::npos);

    auto getResponse = sendRaw("*2\r\n$3\r\nGET\r\n$9\r\novrwrtkey\r\n");
    EXPECT_EQ(getResponse, "$20\r\noverwritten via http\r\n");
}

TEST_F(TestIntegration, respKeysReflectsObjectsStoredViaHttp)
{
    std::string body = "value one";
    std::string putRequest = "PUT /httpkey1 HTTP/1.1\r\nHost: localhost\r\nContent-Length: "
        + std::to_string(body.size()) + "\r\n\r\n" + body;
    sendRaw(putRequest);

    auto keysResponse = sendRaw("*1\r\n$4\r\nKEYS\r\n");
    EXPECT_NE(keysResponse.find("httpkey1"), std::string::npos);
}

TEST_F(TestIntegration, httpRootListingReflectsObjectsStoredViaResp)
{
    std::string value = "value two";
    std::string setCmd = "*3\r\n$3\r\nSET\r\n$8\r\nrespkey1\r\n$"
        + std::to_string(value.size()) + "\r\n" + value + "\r\n";
    sendRaw(setCmd);

    auto rootResponse = sendRaw("GET / HTTP/1.1\r\nHost: localhost\r\n\r\n");
    EXPECT_NE(rootResponse.find("respkey1"), std::string::npos);
}

TEST_F(TestIntegration, manyConcurrentClientsDoNotCorruptEachOther)
{
    constexpr int kClients = 20;
    std::vector<std::thread> threads;
    std::vector<bool> results(kClients, false);

    for (int i = 0; i < kClients; i++)
    {
        threads.emplace_back([this, i, &results]
        {
            std::string key = "key" + std::to_string(i);
            std::string value = "value_" + std::to_string(i);
            std::string cmd = "*3\r\n$3\r\nSET\r\n$" + std::to_string(key.size()) + "\r\n" + key
                + "\r\n$" + std::to_string(value.size()) + "\r\n" + value + "\r\n";
            auto reply = sendRaw(cmd);
            results[i] = (reply == "+OK\r\n");
        });
    }
    for (auto& t : threads) t.join();

    for (bool r : results) EXPECT_TRUE(r);

    for (int i = 0; i < kClients; i++)
    {
        std::string key = "key" + std::to_string(i);
        auto reply = sendRaw("*2\r\n$3\r\nGET\r\n$" + std::to_string(key.size()) + "\r\n" + key + "\r\n");
        EXPECT_TRUE(reply.find("value_" + std::to_string(i)) != std::string::npos);
    }
}

TEST_F(TestIntegration, garbageBytesDoNotCrashServer)
{
    sendRaw("this is not resp or http at all \x00\x01\x02", 200);
    // Server should still be alive and responsive after garbage input:
    auto pingReply = sendRaw("*1\r\n$4\r\nPING\r\n");
    EXPECT_EQ(pingReply, "+PONG\r\n");
}

TEST_F(TestIntegration, truncatedHttpRequestDoesNotHang)
{
    sendRaw("PUT /x HTTP/1.1\r\nContent-Length: 1000000\r\n\r\nonly a few bytes", 300);
    auto pingReply = sendRaw("*1\r\n$4\r\nPING\r\n");
    EXPECT_EQ(pingReply, "+PONG\r\n");
}

TEST_F(TestIntegration, malformedRespArrayGetsRejectedNotHung)
{
    auto reply = sendRaw("*1\r\n*1\r\n$4\r\nPING\r\n\r\n"); // nested array, unsupported
    // Should get SOME reply (error) or connection close within timeout, not silence forever
    auto pingReply = sendRaw("*1\r\n$4\r\nPING\r\n");
    EXPECT_EQ(pingReply, "+PONG\r\n");
}

TEST_F(TestIntegration, rejectsPutExceedingCapacity)
{
    std::string huge(60 * 1024 * 1024, 'x'); // > 50MB cap set in SetUp
    std::string req = "PUT /huge HTTP/1.1\r\nContent-Length: " + std::to_string(huge.size()) + "\r\n\r\n" + huge;
    auto response = sendRaw(req, 3000);
    EXPECT_TRUE(response.find("507") != std::string::npos || response.find("Insufficient") != std::string::npos);

    // Confirm nothing was actually stored
    auto keysReply = sendRaw("*1\r\n$4\r\nKEYS\r\n");
    EXPECT_TRUE(keysReply.find("huge") == std::string::npos);
}

TEST_F(TestIntegration, largeUploadRoundTripsCorrectly)
{
    std::string large(20 * 1024 * 1024, 'z');
    std::string put = "PUT /large HTTP/1.1\r\nContent-Length: " + std::to_string(large.size()) + "\r\n\r\n" + large;
    auto putResp = sendRaw(put, 5000);
    EXPECT_TRUE(putResp.find("200") != std::string::npos);

    auto getResp = sendRaw("GET /large HTTP/1.1\r\nHost: localhost\r\n\r\n", 5000);
    EXPECT_TRUE(getResp.size() >= large.size());
    EXPECT_NE(getResp.find(large.substr(0, 100)), std::string::npos);
}

TEST_F(TestIntegration, hugeRespArrayCountRejectedNotHung)
{
    auto reply = sendRaw("*999999999\r\n$4\r\nPING\r\n", 500);
    auto pingReply = sendRaw("*1\r\n$4\r\nPING\r\n");
    EXPECT_EQ(pingReply, "+PONG\r\n");
}

TEST_F(TestIntegration, abruptDisconnectMidUploadDoesNotCrashServer)
{
    {
        asio::io_context clientCtx;
        asio::ip::tcp::socket socket(clientCtx);
        socket.connect(asio::ip::tcp::endpoint(asio::ip::address::from_string("127.0.0.1"), port));

        std::string headers = "PUT /aborted HTTP/1.1\r\nHost: localhost\r\nContent-Length: 10000000\r\n\r\n";
        std::error_code writeEc;
        asio::write(socket, asio::buffer(headers), writeEc);
        asio::write(socket, asio::buffer(std::string(500000, 'x')), writeEc);
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    auto pingReply = sendRaw("*1\r\n$4\r\nPING\r\n");
    EXPECT_EQ(pingReply, "+PONG\r\n");

    auto keysReply = sendRaw("*1\r\n$4\r\nKEYS\r\n");
    EXPECT_TRUE(keysReply.find("aborted") == std::string::npos);
}

TEST_F(TestIntegration, manyIdleConnectionsDoNotStarveServer)
{
    constexpr int kIdleClients = 30;
    std::vector<std::unique_ptr<asio::io_context>> contexts;
    std::vector<std::unique_ptr<asio::ip::tcp::socket>> sockets;

    for (int i = 0; i < kIdleClients; i++)
    {
        auto ctx = std::make_unique<asio::io_context>();
        auto socket = std::make_unique<asio::ip::tcp::socket>(*ctx);
        socket->connect(asio::ip::tcp::endpoint(asio::ip::address::from_string("127.0.0.1"), port));
        contexts.push_back(std::move(ctx));
        sockets.push_back(std::move(socket));
    }

    auto pingReply = sendRaw("*1\r\n$4\r\nPING\r\n");
    EXPECT_EQ(pingReply, "+PONG\r\n");
}

TEST_F(TestIntegration, garbageBytesOnHttpLikeStartDoNotCrashServer)
{
    sendRaw("GET \x00\x01\xff malformed garbage HTTP/9.9\r\n\r\n", 300);

    auto pingReply = sendRaw("*1\r\n$4\r\nPING\r\n");
    EXPECT_EQ(pingReply, "+PONG\r\n");
}

TEST_F(TestIntegration, garbageBytesOnRespLikeStartDoNotCrashServer)
{
    sendRaw("*3\r\n$-99\r\ngarbage\r\n\xff\xfe\xfd", 300);

    auto pingReply = sendRaw("*1\r\n$4\r\nPING\r\n");
    EXPECT_EQ(pingReply, "+PONG\r\n");
}

TEST_F(TestIntegration, negativeContentLengthRejectedNotHung)
{
    auto response = sendRaw("PUT /neg HTTP/1.1\r\nHost: localhost\r\nContent-Length: -5\r\n\r\nhello", 300);
    EXPECT_TRUE(response.find("400") != std::string::npos || response.empty());

    auto pingReply = sendRaw("*1\r\n$4\r\nPING\r\n");
    EXPECT_EQ(pingReply, "+PONG\r\n");
}

TEST_F(TestIntegration, nonNumericRespBulkLengthRejectedNotHung)
{
    auto reply = sendRaw("*1\r\n$abc\r\nPING\r\n", 300);

    auto pingReply = sendRaw("*1\r\n$4\r\nPING\r\n");
    EXPECT_EQ(pingReply, "+PONG\r\n");
}

TEST_F(TestIntegration, extremelyLongSingleLineWithoutTerminatorDoesNotHang)
{
    std::string noTerminator(2 * 1024 * 1024, 'A'); // 2 MB, no \r\n at all
    sendRaw(noTerminator, 500);

    auto pingReply = sendRaw("*1\r\n$4\r\nPING\r\n");
    EXPECT_EQ(pingReply, "+PONG\r\n");
}

TEST_F(TestIntegration, mixedProtocolGarbageAcrossManySequentialConnections)
{
    std::vector<std::string> attacks = {
        "\x00\x00\x00\x00",
        "*-5\r\n",
        "PUT / HTTP/1.1\r\n\r\n",
        "$999999999999999999\r\n",
        "*1\r\n:notanumber\r\n",
        "POST /unsupported HTTP/1.1\r\n\r\n"
    };

    for (const auto& attack : attacks)
    {
        sendRaw(attack, 200);
    }

    auto pingReply = sendRaw("*1\r\n$4\r\nPING\r\n");
    EXPECT_EQ(pingReply, "+PONG\r\n");
}
