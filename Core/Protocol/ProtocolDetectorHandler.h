#pragma once

#include "IProtocolHandler.h"
#include "IObjectStorage.h"
#include <memory>

class ProtocolDetectorHandler : public IProtocolHandler
{
enum class HttpMatchResult { Match, NoMatch, NeedMoreData };

public:
    ProtocolDetectorHandler(IObjectStorage &storage);
    ~ProtocolDetectorHandler() override = default;

    bool onBytesReceived(std::string_view data) override;
    std::vector<std::byte> takeOutgoingBytes() override;
    bool hasMoreToSend() const override;

private:
    bool isRespPrefix(char c) const;
    HttpMatchResult isHttp(std::string_view data) const;

    std::unique_ptr<IProtocolHandler> m_delegate;
    IObjectStorage &m_storage;
};