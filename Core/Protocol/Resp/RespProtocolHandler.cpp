#include "RespProtocolHandler.h"
#include "RespParser.h"
#include "IObjectStorage.h"
#include "StreamingConfig.h"
#include "Utils.h"
#include "RespReply.h"

RespProtocolHandler::RespProtocolHandler(IObjectStorage &storage) 
    : m_storage(storage)
    , m_streamingStorage(dynamic_cast<IStreamingObjectStorage*>(&storage))
{
    registerCommands();
}

bool RespProtocolHandler::hasMoreToSend() const
{
    return m_streamingRead;
}

void RespProtocolHandler::registerCommands()
{
    m_commands.emplace("PING", &RespProtocolHandler::handlePing);
    m_commands.emplace("GET", &RespProtocolHandler::handleGet);
    m_commands.emplace("SET", &RespProtocolHandler::handleSet);
    m_commands.emplace("DEL", &RespProtocolHandler::handleDel);
    m_commands.emplace("KEYS", &RespProtocolHandler::handleKeys);
    m_commands.emplace("SETNX", &RespProtocolHandler::handleSetNx);
}

void RespProtocolHandler::queueResponse(const std::string& response)
{
    const auto* bytes = reinterpret_cast<const std::byte*>(response.data());
    m_outgoingBuffer.insert(m_outgoingBuffer.end(), bytes, bytes + response.size());
}

void RespProtocolHandler::processCommands(const std::vector<std::string>& commands)
{
    if (commands.empty()) return;

    const auto& command = commands.front();
    std::span<const std::string> args(commands.data() + 1, commands.size() - 1);

    std::string response = handleCommand(command, args);
    if (!response.empty())
    {
        queueResponse(response);
    }
}

bool RespProtocolHandler::onBytesReceived(std::string_view data)
{
    if (m_state == State::StreamingValue)
    {
        return continueStreamingValue(data);
    }
    if (m_state == State::DiscardingRejectedValue)
    {
        return continueDiscardingValue(data);
    }

    m_incomingBuffer.append(data.data(), data.size());

    if (m_streamingStorage && tryBeginStreamingSetValue())
    {
        return true;
    }

    std::vector<std::string> commands;
    size_t bytesConsumed = 0;

    auto result = resp::RespParser::parse(m_incomingBuffer, commands, bytesConsumed);

    if (result == resp::RespParser::ParseResult::Complete)
    {
        processCommands(commands);
        m_incomingBuffer.erase(0, bytesConsumed);
    }
    else if (result == resp::RespParser::ParseResult::Error)
    {
        m_incomingBuffer.clear();
        return false;
    }

    return true;
}

bool RespProtocolHandler::tryBeginStreamingSetValue()
{
    const std::string& buf = m_incomingBuffer;
    size_t pos = 0;

    if (buf.empty() || buf[0] != '*') return false;
    size_t end = buf.find("\r\n", pos);
    if (end == std::string::npos) return false;
    if (buf.substr(pos, end - pos) != "*3") return false;
    pos = end + 2;

    auto cmdBulk = resp::RespParser::parseBulkString(buf, pos);
    if (!cmdBulk) return false;

    std::string cmdUpper = utils::toUpper(cmdBulk->data);
    bool isSet = (cmdUpper == "SET");
    bool isSetNx = (cmdUpper == "SETNX");
    if (!isSet && !isSetNx) return false;

    auto keyBulk = resp::RespParser::parseBulkString(buf, pos);
    if (!keyBulk) return false;

    if (pos >= buf.size() || buf[pos] != '$') return false;
    size_t valHeaderEnd = buf.find("\r\n", pos);
    if (valHeaderEnd == std::string::npos) return false;

    long valueLength = 0;
    try
    {
        valueLength = std::stol(buf.substr(pos + 1, valHeaderEnd - pos - 1));
    }
    catch (...)
    {
        return false;
    }

    if (valueLength < 0 || static_cast<size_t>(valueLength) <= kStreamingThreshold)
    {
        return false;
    }

    size_t expectedSize = static_cast<size_t>(valueLength);
    size_t valueDataStart = valHeaderEnd + 2;

    std::string leftoverOwned = buf.substr(std::min(valueDataStart, buf.size()));

    if (isSetNx && m_storage.exists(keyBulk->data))
    {
        queueResponse(respreply::integer(0));
        m_incomingBuffer.clear();

        m_discardBytesRemaining = expectedSize + 2;
        m_state = State::DiscardingRejectedValue;

        if (!leftoverOwned.empty())
        {
            return continueDiscardingValue(leftoverOwned);
        }
        return true;
    }

    auto beginResult = m_streamingStorage->beginPut(keyBulk->data, expectedSize);
    if (beginResult.status != PutResult::Ok)
    {
        queueResponse(respreply::putResultToReply(beginResult.status));
        m_incomingBuffer.clear();

        m_discardBytesRemaining = expectedSize + 2;
        m_state = State::DiscardingRejectedValue;

        if (!leftoverOwned.empty())
        {
            return continueDiscardingValue(leftoverOwned);
        }
        return true;
    }

    m_activeSession = std::move(beginResult.session);
    m_valueBytesRemaining = expectedSize;
    m_trailingCrlfRemaining = 2;
    m_isSetNx = isSetNx;
    m_pendingKey = keyBulk->data;
    m_state = State::StreamingValue;

    m_incomingBuffer.clear();

    if (!leftoverOwned.empty())
    {
        return continueStreamingValue(leftoverOwned);
    }
    return true;
}

bool RespProtocolHandler::continueStreamingValue(std::string_view data)
{
    if (m_valueBytesRemaining > 0)
    {
        size_t toConsume = std::min(data.size(), m_valueBytesRemaining);

        if (!m_activeSession->writeChunk(data.substr(0, toConsume)))
        {
            m_activeSession->abort();
            m_activeSession.reset();
            m_state = State::ParsingCommands;
            queueResponse(respreply::error("write failed"));
            return false;
        }

        m_valueBytesRemaining -= toConsume;
        data = data.substr(toConsume);
    }

    if (m_valueBytesRemaining > 0)
    {
        return true;
    }

    while (m_trailingCrlfRemaining > 0 && !data.empty())
    {
        data = data.substr(1);
        m_trailingCrlfRemaining--;
    }

    if (m_trailingCrlfRemaining > 0)
    {
        return true;
    }

    finishStreamingValue();
    m_state = State::ParsingCommands;

    if (!data.empty())
    {
        return onBytesReceived(data);
    }
    return true;
}

void RespProtocolHandler::finishStreamingValue()
{
    PutResult result = m_activeSession->finish();
    m_activeSession.reset();

    if (m_isSetNx)
    {
        queueResponse(result == PutResult::Ok ? respreply::integer(1)
                                                : respreply::putResultToReply(result));
    }
    else
    {
        queueResponse(respreply::putResultToReply(result));
    }
}

std::string RespProtocolHandler::handleCommand(
    const std::string& command,
    std::span<const std::string> args)
{
    auto it = m_commands.find(utils::toUpper(command));

    if (it == m_commands.end())
    {
        return respreply::error("unknown command '" + command + "'");
    }

    return (this->*(it->second))(args);
}

std::string RespProtocolHandler::handlePing(std::span<const std::string> args)
{
    if (!args.empty())
    {
        return respreply::simpleString(args[0]);
    }
    return respreply::pong();
}

std::string RespProtocolHandler::handleGet(std::span<const std::string> args)
{
    if (args.size() != 1)
    {
        return respreply::error("wrong number of arguments for 'get' command");
    }

    if (m_streamingStorage)
    {
        auto result = m_streamingStorage->beginGet(args[0]);
        if (result.found)
        {
            queueResponse("$" + std::to_string(result.size) + "\r\n");
            m_activeReadSession = std::move(result.session);
            m_streamingRead = true;
            return "";
        }
        return respreply::nil();
    }

    auto value = m_storage.get(args[0]);
    if (!value.has_value())
    {
        return respreply::nil();
    }
    return respreply::bulkString(*value);
}

std::string RespProtocolHandler::handleSet(std::span<const std::string> args)
{
    if (args.size() != 2)
    {
        return respreply::error("wrong number of arguments for 'set' command");
    }
    return respreply::putResultToReply(m_storage.put(args[0], args[1]));
}

std::string RespProtocolHandler::handleDel(std::span<const std::string> args)
{
    if (args.size() != 1)
    {
        return respreply::error("wrong number of arguments for 'del' command");
    }
    bool deleted = m_storage.del(args[0]);
    return respreply::integer(deleted ? 1 : 0);
}

std::string RespProtocolHandler::handleKeys(std::span<const std::string>)
{
    auto keys = m_storage.keys();
    std::string response = respreply::arrayHeader(keys.size());
    for (const auto& key : keys)
    {
        response += respreply::bulkString(key);
    }
    return response;
}

std::string RespProtocolHandler::handleSetNx(std::span<const std::string> args)
{
    if (args.size() != 2)
    {
        return respreply::error("wrong number of arguments for 'setnx' command");
    }
    return respreply::setNxResultToReply(m_storage.setnx(args[0], args[1]));
}

bool RespProtocolHandler::continueDiscardingValue(std::string_view data)
{
    size_t toDiscard = std::min(data.size(), m_discardBytesRemaining);
    m_discardBytesRemaining -= toDiscard;

    if (m_discardBytesRemaining > 0)
    {
        return true;
    }

    m_state = State::ParsingCommands;

    std::string_view remainder = data.substr(toDiscard);
    if (!remainder.empty())
    {
        return onBytesReceived(remainder);
    }
    return true;
}

void RespProtocolHandler::pumpReadSession()
{
    auto chunk = m_activeReadSession->readChunk(kReadChunkSize);
    if (chunk)
    {
        queueResponse(*chunk);
        return;
    }

    queueResponse("\r\n");
    m_activeReadSession.reset();
    m_streamingRead = false;
}

std::vector<std::byte> RespProtocolHandler::takeOutgoingBytes()
{
    if (m_outgoingBuffer.empty() && m_streamingRead)
    {
        pumpReadSession();
    }
    std::vector<std::byte> result = std::move(m_outgoingBuffer);
    m_outgoingBuffer.clear();
    return result;
}
