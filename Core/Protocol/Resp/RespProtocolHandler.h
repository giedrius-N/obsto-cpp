#pragma once

#include "IProtocolHandler.h"
#include "IObjectStorage.h"
#include "IStreamingObjectStorage.h"
#include "IWriteSession.h"
#include "IReadSession.h"
#include <memory>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

class RespProtocolHandler : public IProtocolHandler
{
public:
    explicit RespProtocolHandler(IObjectStorage& storage);

    bool onBytesReceived(std::string_view data) override;
    std::vector<std::byte> takeOutgoingBytes() override;
    bool hasMoreToSend() const override;

private:
    using CommandFn = std::string (RespProtocolHandler::*)(std::span<const std::string>);

    void registerCommands();
    void processCommands(const std::vector<std::string>& commands);
    std::string handleCommand(const std::string& command, std::span<const std::string> args);

    std::string handlePing(std::span<const std::string> args);
    std::string handleGet(std::span<const std::string> args);
    std::string handleSet(std::span<const std::string> args);
    std::string handleDel(std::span<const std::string> args);
    std::string handleKeys(std::span<const std::string> args);
    std::string handleSetNx(std::span<const std::string> args);

    // Streaming
    enum class State { ParsingCommands, StreamingValue, DiscardingRejectedValue };

    bool tryBeginStreamingSetValue();
    bool continueStreamingValue(std::string_view data);
    bool continueDiscardingValue(std::string_view data);
    void finishStreamingValue();
    void queueResponse(const std::string& response);

    IObjectStorage& m_storage;
    IStreamingObjectStorage* m_streamingStorage;

    std::string m_incomingBuffer;
    std::vector<std::byte> m_outgoingBuffer;
    std::unordered_map<std::string, CommandFn> m_commands;

    bool m_streamingRead = false;
    std::unique_ptr<IReadSession> m_activeReadSession;
    void pumpReadSession();

    State m_state = State::ParsingCommands;
    std::unique_ptr<IWriteSession> m_activeSession;
    size_t m_valueBytesRemaining = 0;
    size_t m_trailingCrlfRemaining = 0;
    size_t m_discardBytesRemaining = 0;
    bool m_isSetNx = false;
    std::string m_pendingKey;
};
