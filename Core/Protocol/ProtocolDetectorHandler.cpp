#include "ProtocolDetectorHandler.h"
#include "RespProtocolHandler.h"
#include "HttpProtocolHandler.h"
#include <iostream>

namespace
{
    constexpr http::Method kSupportedHttpMethods[] = {
        http::Method::Get,
        http::Method::Put,
        http::Method::Delete
    };
}

ProtocolDetectorHandler::ProtocolDetectorHandler(IObjectStorage &storage) 
    : m_storage(storage)
{
}

bool ProtocolDetectorHandler::hasMoreToSend() const
{
    if (!m_delegate)
    {
        return false;
    }

    return m_delegate->hasMoreToSend();
}

bool ProtocolDetectorHandler::onBytesReceived(std::string_view data)
{
    printf("Protocol detection handler in progress, received %zu bytes\n", data.size());

    if (!m_delegate)
    {
        if (data.empty())
        {
            return true;
        }

        char firstByte = data[0];
        if (isRespPrefix(firstByte))
        {
            std::cout << "[Detector] Identified RESP protocol!\n";
            m_delegate = std::make_unique<RespProtocolHandler>(m_storage);
        }
        else
        {
            auto httpMatch = isHttp(data);
            if (httpMatch == HttpMatchResult::Match)
            {
                std::cout << "[Detector] Identified HTTP protocol!\n";
                m_delegate = std::make_unique<HttpProtocolHandler>(m_storage);
            }
            else if (httpMatch == HttpMatchResult::NeedMoreData)
            {
                return true; // Wait for more data
            }
            else
            {
                std::cout << "[Detector] Unknown protocol!\n";
                return false;
            }
        }
    }
        
    return m_delegate->onBytesReceived(data);
}

std::vector<std::byte> ProtocolDetectorHandler::takeOutgoingBytes()
{
    if (!m_delegate)
    {
        return {};
    }

    return m_delegate->takeOutgoingBytes();
}

bool ProtocolDetectorHandler::isRespPrefix(char c) const
{
    switch (c) 
    {
        // RESP2
        case '+': // Simple string
        case '-': // Simple err
        case ':': // Integer
        case '$': // Bulk string
        case '*': // Array
            return true;
        default:
            return false;
    }
}

ProtocolDetectorHandler::HttpMatchResult ProtocolDetectorHandler::isHttp(std::string_view data) const
{
    bool anyCouldStillMatch = false;

    for (const auto& method : kSupportedHttpMethods)
    {
        std::string methodStr = std::string(http::toString(method)) + " ";

        size_t compareLen = std::min(data.size(), methodStr.size());

        if (data.substr(0, compareLen) == methodStr.substr(0, compareLen))
        {
            if (data.size() >= methodStr.size())
            {
                return HttpMatchResult::Match;
            }
            anyCouldStillMatch = true;
        }
    }

    return anyCouldStillMatch ? HttpMatchResult::NeedMoreData : HttpMatchResult::NoMatch;
}
