#pragma once

#include <string_view>
#include <vector>

class IProtocolHandler
{
public:
    virtual ~IProtocolHandler() = default;

    virtual bool onBytesReceived(std::string_view data) = 0;
    virtual std::vector<std::byte> takeOutgoingBytes() = 0;
    virtual bool hasMoreToSend() const { return false; }
};
