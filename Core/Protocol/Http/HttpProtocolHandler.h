#pragma once

#include "IProtocolHandler.h"
#include "IObjectStorage.h"
#include "IStreamingObjectStorage.h"
#include "HttpParser.h"

class HttpProtocolHandler : public IProtocolHandler
{
public:
    HttpProtocolHandler(IObjectStorage &storage);
    ~HttpProtocolHandler() override = default;

    bool onBytesReceived(std::string_view data) override;
    std::vector<std::byte> takeOutgoingBytes() override;
    bool hasMoreToSend() const override;

private:
    enum class State { ParsingHeaders, StreamingBody };

    void processRequest(const http::HttpRequest& request);
    std::string handleRequest(const http::HttpRequest& request);

    std::string handleGetRoot();
    std::string handleGetPath(const std::string& path);
    std::string handlePutPath(const std::string& path, const std::string& body);
    std::string handleDeletePath(const std::string& path);

    std::string buildResponse(
        int statusCode, const std::string& statusText,
        const std::string& body, const std::string& contentType = "text/plain"
    );
    std::string buildResponse(
        http::StatusCode code,
        const std::string& body,
        const std::string& contentType = "text/plain"
    );

    // Streaming
    bool beginStreamingBody(const http::HttpRequest& request, std::string_view leftoverBodyBytes);
    bool continueStreamingBody(std::string_view data);
    void finishStreamingBody();
    void queueResponse(const std::string& response);

    void beginStreamingGet(const std::string& key, size_t size);
    void pumpReadSession();

    IObjectStorage &m_storage;
    IStreamingObjectStorage* m_streamingStorage;
    std::string m_incomingBuffer;
    std::vector<std::byte> m_outgoingBuffer;

    State m_state = State::ParsingHeaders;
    std::unique_ptr<IWriteSession> m_activeSession;
    size_t m_bodyBytesRemaining = 0;
    std::string m_pendingPutKey;
    
    // Read streaming
    bool m_streamingRead = false;
    std::unique_ptr<IReadSession> m_activeReadSession;
};
