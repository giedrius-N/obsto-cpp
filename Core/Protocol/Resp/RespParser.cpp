#include "RespParser.h"
#include <iostream>
#include <sstream>

namespace resp
{
std::optional<BulkString> RespParser::parseBulkString(const std::string& buffer, size_t& pos)
{
    if (pos >= buffer.size() || buffer[pos] != '$')
        return std::nullopt;
    
    size_t end = buffer.find("\r\n", pos);
    if (end == std::string::npos) 
        return std::nullopt;
    
    std::string lengthStr = buffer.substr(pos + 1, end - pos - 1);
    
    try 
    {
        int length = std::stoi(lengthStr);
        
        if (length == -1) 
        {
            pos = end + 2;
            return BulkString{-1, ""};
        }
        
        if (length < -1)
            return std::nullopt;

        size_t dataStart = end + 2;
        size_t dataLength = static_cast<size_t>(length);
        
        if (dataStart + dataLength + 2 > buffer.size()) 
        {
            return std::nullopt; // Not enough data yet
        }
        
        std::string data = buffer.substr(dataStart, dataLength);
        
        pos = dataStart + dataLength + 2;
        
        return BulkString{length, data};
    } 
    catch (...) 
    {
        return std::nullopt;
    }
}

std::optional<SimpleString> RespParser::parseSimpleString(const std::string& buffer, size_t& pos)
{
    if (pos >= buffer.size() || buffer[pos] != '+')
        return std::nullopt;
    
    size_t end = buffer.find("\r\n", pos);
    if (end == std::string::npos)
        return std::nullopt;
    
    std::string data = buffer.substr(pos + 1, end - pos - 1);
    pos = end + 2;
    
    return SimpleString{data};
}

std::optional<Error> RespParser::parseError(const std::string& buffer, size_t& pos)
{
    if (pos >= buffer.size() || buffer[pos] != '-')
        return std::nullopt;
    
    size_t end = buffer.find("\r\n", pos);
    if (end == std::string::npos)
        return std::nullopt;
    
    std::string message = buffer.substr(pos + 1, end - pos - 1);
    pos = end + 2;
    
    return Error{message};
}

std::optional<Integer> RespParser::parseInt(const std::string& buffer, size_t& pos)
{
    if (pos >= buffer.size() || buffer[pos] != ':')
        return std::nullopt;
    
    size_t end = buffer.find("\r\n", pos);
    if (end == std::string::npos)
        return std::nullopt;
    
    std::string valueStr = buffer.substr(pos + 1, end - pos - 1);
    pos = end + 2;
    
    try 
    {
        int value = std::stoi(valueStr);
        return Integer{value};
    } 
    catch (...) 
    {
        return std::nullopt;
    }
}

RespParser::ParseResult RespParser::parseArray(const std::string& buffer, size_t& pos, Array& outArray)
{
    if (pos >= buffer.size() || buffer[pos] != '*')
        return ParseResult::Error;

    size_t end = buffer.find("\r\n", pos);
    if (end == std::string::npos)
        return ParseResult::Incomplete;
    std::string countStr = buffer.substr(pos + 1, end - pos - 1);
    size_t savedPos = pos;
    pos = end + 2;

    int count = 0;
    try
    {
        count = std::stoi(countStr);
    }
    catch (...)
    {
        pos = savedPos;
        return ParseResult::Error;
    }

    if (count < 0 || count > kMaxArrayElements)
    {
        pos = savedPos;
        return ParseResult::Error;
    }

    std::vector<std::string> elements;
    for (int i = 0; i < count; i++)
    {
        if (pos >= buffer.size())
        {
            pos = savedPos;
            return ParseResult::Incomplete;
        }

        char elemType = buffer[pos];

        if (elemType == '$')
        {
            auto bulk = parseBulkString(buffer, pos);
            if (!bulk)
            {
                pos = savedPos;
                return ParseResult::Incomplete;
            }
            elements.push_back(bulk->data);
        }
        else if (elemType == '+')
        {
            auto simple = parseSimpleString(buffer, pos);
            if (!simple) { pos = savedPos; return ParseResult::Incomplete; }
            elements.push_back(simple->data);
        }
        else if (elemType == ':')
        {
            auto integer = parseInt(buffer, pos);
            if (!integer) { pos = savedPos; return ParseResult::Incomplete; }
            elements.push_back(std::to_string(integer->value));
        }
        else
        {
            pos = savedPos;
            return ParseResult::Error;
        }
    }

    outArray = Array{count, elements};
    return ParseResult::Complete;
}

RespParser::ParseResult RespParser::parse(const std::string& buffer, std::vector<std::string>& commands, size_t& bytesConsumed)
{
    size_t pos = 0;
    bytesConsumed = 0;
    
    if (buffer.empty())
        return ParseResult::Incomplete;
    
    char type = buffer[pos];
    
    if (type == '*') 
    {
        Array array;
        auto result = parseArray(buffer, pos, array);

        if (result != ParseResult::Complete)
        {
            return result;
        }

        if (!array.elements.empty()) 
        {
            commands.push_back(array.elements[0]);
            for (size_t i = 1; i < array.elements.size(); i++) 
            {
                commands.push_back(array.elements[i]);
            }
        }
        bytesConsumed = pos;
        return ParseResult::Complete;
    }
    else if (type == '$') 
    {
        auto bulk = parseBulkString(buffer, pos);
        if (!bulk)
            return ParseResult::Incomplete;
        commands.push_back(bulk->data);
        bytesConsumed = pos;
        return ParseResult::Complete;
    }
    else if (type == '+') 
    {
        auto simple = parseSimpleString(buffer, pos);
        if (!simple)
            return ParseResult::Incomplete;
        commands.push_back(simple->data);
        bytesConsumed = pos;
        return ParseResult::Complete;
    }
    else if (type == '-') 
    {
        auto error = parseError(buffer, pos);
        if (!error)
            return ParseResult::Incomplete;
        commands.push_back("ERROR:" + error->message);
        bytesConsumed = pos;
        return ParseResult::Complete;
    }
    else if (type == ':') 
    {
        auto integer = parseInt(buffer, pos);
        if (!integer)
            return ParseResult::Incomplete;
        commands.push_back("INT:" + std::to_string(integer->value));
        bytesConsumed = pos;
        return ParseResult::Complete;
    }
    else 
    {
        // Raw data - parse line by line
        size_t end = buffer.find("\r\n", pos);
        if (end == std::string::npos)
            return ParseResult::Incomplete;
        commands.push_back(buffer.substr(pos, end - pos));
        bytesConsumed = end + 2;
        return ParseResult::Complete;
    }
}
} // namespace resp
