#include "HttpProtocolHandler.h"
#include "StreamingConfig.h"
#include <algorithm>

HttpProtocolHandler::HttpProtocolHandler(IObjectStorage &storage)
    : m_storage(storage)
    , m_streamingStorage(dynamic_cast<IStreamingObjectStorage*>(&storage))
{
}

bool HttpProtocolHandler::hasMoreToSend() const
{
    return m_streamingRead;
}

bool HttpProtocolHandler::onBytesReceived(std::string_view data)
{
    if (m_state == State::StreamingBody)
    {
        return continueStreamingBody(data);
    }

    m_incomingBuffer.append(data.data(), data.size());

    http::HttpRequest headerPeek;
    size_t headerBytesConsumed = 0;
    auto headerResult = HttpParser::parseHeadersOnly(m_incomingBuffer, headerPeek, headerBytesConsumed);

    if (headerResult == HttpParser::ParseResult::Complete)
    {
        size_t contentLength = 0;
        bool hasValidContentLength = true;
        auto it = headerPeek.headers.find("content-length");
        if (it != headerPeek.headers.end())
        {
            auto parsed = HttpParser::parseContentLength(it->second);
            if (parsed)
            {
                contentLength = *parsed;
            }
            else
            {
                // Invalid Content-Length (negative, non-numeric, etc.) —
                // don't treat as a streaming candidate. Fall through to
                // the full parser below, which will correctly reject it
                // with a proper 400.
                hasValidContentLength = false;
            }
        }

        bool isPut = (headerPeek.method == "PUT");
        bool wantsStreaming = isPut
            && hasValidContentLength
            && m_streamingStorage != nullptr
            && contentLength > kStreamingThreshold;

        if (wantsStreaming)
        {
            std::string_view leftoverBody = std::string_view(m_incomingBuffer).substr(headerBytesConsumed);
            m_incomingBuffer.clear();
            bool res = beginStreamingBody(headerPeek, leftoverBody);
            return res;
        }
    }

    // Non-streaming path
    http::HttpRequest request;
    size_t bytesConsumed = 0;
    auto result = HttpParser::parse(m_incomingBuffer, request, bytesConsumed);

    if (result == HttpParser::ParseResult::Complete)
    {
        processRequest(request);
        m_incomingBuffer.erase(0, bytesConsumed);
    }
    else if (result == HttpParser::ParseResult::Error)
    {
        queueResponse(buildResponse(http::StatusCode::BadRequest, "Malformed request"));
        m_incomingBuffer.clear();
        return false;
    }

    return true;
}

std::string HttpProtocolHandler::buildResponse(
    http::StatusCode code,
    const std::string& body,
    const std::string& contentType)
{
    int numericCode = static_cast<int>(code);
    std::string statusText = std::string(http::getStatusCodeStr(code));
    
    return buildResponse(numericCode, statusText, body, contentType);
}

std::string HttpProtocolHandler::buildResponse(
    int statusCode, const std::string& statusText,
    const std::string& body, const std::string& contentType)
{
    return "HTTP/1.1 " + std::to_string(statusCode) + " " + statusText + "\r\n"
         + "Content-Type: " + contentType + "\r\n"
         + "Content-Length: " + std::to_string(body.size()) + "\r\n\r\n"
         + body;
}

std::string HttpProtocolHandler::handleRequest(const http::HttpRequest& request)
{
    if (request.method == http::toString(http::Method::Get) && request.path == "/")
    {
        return handleGetRoot();
    }
    else if (request.method == http::toString(http::Method::Get))
    {
        return handleGetPath(request.path);
    }
    else if (request.method == http::toString(http::Method::Put))
    {
        return handlePutPath(request.path, request.body);
    }
    else if (request.method == http::toString(http::Method::Delete))
    {
        return handleDeletePath(request.path);
    }

    return buildResponse(http::StatusCode::MethodNotAllowed, "Method Not Allowed", "Method not supported");
}

std::string HttpProtocolHandler::handleGetRoot()
{
    std::string contentType = "text/plain";

    auto keys = m_storage.keys();
    std::string body;
    for (const auto& key : keys)
    {
        body += key + "\n";
    }
    return buildResponse(http::StatusCode::OK, body, contentType);
}

std::string HttpProtocolHandler::handlePutPath(const std::string& path, const std::string& body)
{
    std::string key = path.size() > 1 ? path.substr(1) : "";

    PutResult result = m_storage.put(key, body);
    switch (result)
    {
        case PutResult::Ok:
            return buildResponse(http::StatusCode::OK, "", "");
        case PutResult::CapacityExceeded:
            return buildResponse(http::StatusCode::InsufficientStorage, "Storage capacity exceeded", "text/plain");
        case PutResult::InvalidPath:
            return buildResponse(http::StatusCode::BadRequest, "Bad Request", "Invalid path");
    }
    return buildResponse(http::StatusCode::InternalServerError, "Internal Server Error", "");
}

std::string HttpProtocolHandler::handleDeletePath(const std::string& path)
{
    std::string key = path.size() > 1 ? path.substr(1) : "";

    if (!m_storage.exists(key))
    {
        return buildResponse(http::StatusCode::NotFound, "No object at this path", "text/plain");
    }

    bool success = m_storage.del(key);
    if (success)
    {
        return buildResponse(http::StatusCode::OK, "", "");
    }
    else
    {
        return buildResponse(http::StatusCode::InternalServerError, "Failed to delete object", "text/plain");
    }
}

std::vector<std::byte> HttpProtocolHandler::takeOutgoingBytes()
{
    if (m_outgoingBuffer.empty() && m_streamingRead)
    {
        pumpReadSession();
    }
    std::vector<std::byte> result = std::move(m_outgoingBuffer);
    m_outgoingBuffer.clear();
    return result;
}

bool HttpProtocolHandler::beginStreamingBody(const http::HttpRequest& request, std::string_view leftoverBodyBytes)
{
    std::string key = request.path.size() > 1 ? request.path.substr(1) : "";

    auto headerIt = request.headers.find("content-length");
    if (headerIt == request.headers.end())
    {
        queueResponse(buildResponse(http::StatusCode::BadRequest, "Missing Content-Length"));
        return false;
    }

    auto parsed = HttpParser::parseContentLength(headerIt->second);
    if (!parsed)
    {
        queueResponse(buildResponse(http::StatusCode::BadRequest, "Invalid Content-Length"));
        return false;
    }
    size_t contentLength = *parsed;

    auto beginResult = m_streamingStorage->beginPut(key, contentLength);

    switch (beginResult.status)
    {
        case PutResult::Ok:
            break;
        case PutResult::CapacityExceeded:
            queueResponse(buildResponse(http::StatusCode::InsufficientStorage, "Storage capacity exceeded"));
            return false;
        case PutResult::InvalidPath:
            queueResponse(buildResponse(http::StatusCode::BadRequest, "Invalid path"));
            return false;
        default:
            queueResponse(buildResponse(http::StatusCode::InternalServerError, "Unknown error"));
            return false;
    }

    m_activeSession = std::move(beginResult.session);
    m_bodyBytesRemaining = contentLength;
    m_pendingPutKey = key;
    m_state = State::StreamingBody;

    if (!leftoverBodyBytes.empty())
    {
        return continueStreamingBody(leftoverBodyBytes);
    }
    return true;
}

bool HttpProtocolHandler::continueStreamingBody(std::string_view data)
{
    size_t toConsume = std::min(data.size(), m_bodyBytesRemaining);
    std::string_view chunk = data.substr(0, toConsume);

    if (!m_activeSession->writeChunk(chunk))
    {
        m_activeSession->abort();
        m_activeSession.reset();
        m_state = State::ParsingHeaders;
        queueResponse(buildResponse(http::StatusCode::InternalServerError, "Write failed"));
        return false;
    }

    m_bodyBytesRemaining -= toConsume;

    if (m_bodyBytesRemaining == 0)
    {
        finishStreamingBody();

        std::string_view remainder = data.substr(toConsume);
        m_state = State::ParsingHeaders;
        if (!remainder.empty())
        {
            return onBytesReceived(remainder);
        }
    }

    return true;
}

void HttpProtocolHandler::finishStreamingBody()
{
    PutResult result = m_activeSession->finish();
    m_activeSession.reset();

    switch (result)
    {
        case PutResult::Ok:              
            queueResponse(buildResponse(http::StatusCode::OK, "")); 
            break;
        case PutResult::CapacityExceeded:  
            queueResponse(buildResponse(http::StatusCode::InsufficientStorage, "Storage capacity exceeded")); 
            break;
        case PutResult::InvalidPath:       
            queueResponse(buildResponse(http::StatusCode::BadRequest, "Invalid path")); 
            break;
    }
}

std::string HttpProtocolHandler::handleGetPath(const std::string& path)
{
    std::string key = path.size() > 1 ? path.substr(1) : "";

    if (m_streamingStorage)
    {
        auto result = m_streamingStorage->beginGet(key);
        if (result.found)
        {
            std::string headers = "HTTP/1.1 200 OK\r\n"
                "Content-Type: application/octet-stream\r\n"
                "Content-Length: " + std::to_string(result.size) + "\r\n\r\n";
            queueResponse(headers);

            m_activeReadSession = std::move(result.session);
            m_streamingRead = true;
            return "";
        }
        return buildResponse(http::StatusCode::NotFound, "No object at this path");
    }

    auto value = m_storage.get(key);
    if (!value.has_value())
    {
        return buildResponse(http::StatusCode::NotFound, "No object at this path");
    }
    return buildResponse(http::StatusCode::OK, *value, "application/octet-stream");
}

void HttpProtocolHandler::pumpReadSession()
{
    auto chunk = m_activeReadSession->readChunk(kReadChunkSize);
    if (!chunk)
    {
        m_activeReadSession.reset();
        m_streamingRead = false;
        return;
    }
    queueResponse(*chunk);
}

void HttpProtocolHandler::processRequest(const http::HttpRequest& request)
{
    std::string response = handleRequest(request);
    if (!response.empty())
    {
        queueResponse(response);
    }
}

void HttpProtocolHandler::queueResponse(const std::string& response)
{
    for (char c : response)
    {
        m_outgoingBuffer.push_back(static_cast<std::byte>(c));
    }
}
