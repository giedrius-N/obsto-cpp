#include "HttpParser.h"
#include <sstream>
#include <cctype>

namespace
{
    constexpr size_t kMaxHeaderSectionSize = 16 * 1024; // 16 KB
    constexpr size_t kMaxBodySize = 5ull * 1024 * 1024 * 1024; // 5 GB
}

std::string HttpParser::toLower(std::string s)
{
    for (auto& c : s)
    {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return s;
}

bool HttpParser::parseRequestLine(const std::string& line, http::HttpRequest& outRequest)
{
    std::istringstream iss(line);
    if (!(iss >> outRequest.method >> outRequest.path >> outRequest.version))
    {
        return false;
    }
    return true;
}

bool HttpParser::parseHeaderLine(const std::string& line, http::HttpRequest& outRequest)
{
    size_t colon = line.find(':');
    if (colon == std::string::npos)
    {
        return false;
    }

    std::string name = toLower(line.substr(0, colon));

    size_t valueStart = colon + 1;
    while (valueStart < line.size() && line[valueStart] == ' ')
    {
        valueStart++;
    }

    outRequest.headers[name] = line.substr(valueStart);
    return true;
}

HttpParser::ParseResult HttpParser::parse(
    const std::string& buffer, http::HttpRequest& outRequest, size_t& bytesConsumed)
{
    bytesConsumed = 0;

    size_t headerEnd = buffer.find("\r\n\r\n");
    if (headerEnd == std::string::npos)
    {
        if (buffer.size() > kMaxHeaderSectionSize)
        {
            return ParseResult::Error;
        }
        return ParseResult::Incomplete;
    }

    // Split header section into lines.
    std::string headerSection = buffer.substr(0, headerEnd);
    std::istringstream headerStream(headerSection);
    std::string line;

    if (!std::getline(headerStream, line))
    {
        return ParseResult::Error;
    }
    if (!line.empty() && line.back() == '\r')
    {
        line.pop_back();
    }
    if (!parseRequestLine(line, outRequest))
    {
        return ParseResult::Error; // malformed request line
    }

    while (std::getline(headerStream, line))
    {
        if (!line.empty() && line.back() == '\r')
        {
            line.pop_back();
        }
        if (line.empty())
        {
            continue;
        }
        if (!parseHeaderLine(line, outRequest))
        {
            return ParseResult::Error;
        }
    }

    size_t bodyStart = headerEnd + 4; // skip past "\r\n\r\n"
    size_t contentLength = 0;

    auto it = outRequest.headers.find("content-length");
    if (it != outRequest.headers.end())
    {
        try
        {
            long len = std::stol(it->second);
            if (len < 0)
            {
                return ParseResult::Error;
            }
            contentLength = static_cast<size_t>(len);
        }
        catch (const std::exception&)
        {
            return ParseResult::Error;
        }

        if (contentLength > kMaxBodySize)
        {
            return ParseResult::Error;
        }
    }

    if (buffer.size() < bodyStart + contentLength)
    {
        return ParseResult::Incomplete;
    }

    outRequest.body = buffer.substr(bodyStart, contentLength);
    bytesConsumed = bodyStart + contentLength;

    return ParseResult::Complete;
}

HttpParser::ParseResult HttpParser::parseHeadersOnly(
    const std::string& buffer, http::HttpRequest& outRequest, size_t& headerBytesConsumed)
{
    headerBytesConsumed = 0;
    size_t headerEnd = buffer.find("\r\n\r\n");
    if (headerEnd == std::string::npos)
    {
        return buffer.size() > kMaxHeaderSectionSize ? ParseResult::Error : ParseResult::Incomplete;
    }

    std::istringstream headerStream(buffer.substr(0, headerEnd));
    std::string line;
    if (!std::getline(headerStream, line)) return ParseResult::Error;
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (!parseRequestLine(line, outRequest)) return ParseResult::Error;

    while (std::getline(headerStream, line))
    {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        if (!parseHeaderLine(line, outRequest)) return ParseResult::Error;
    }

    headerBytesConsumed = headerEnd + 4;
    return ParseResult::Complete;
}

std::optional<size_t> HttpParser::parseContentLength(const std::string& value)
{
    if (value.empty() || value[0] == '-')
    {
        return std::nullopt;
    }
    try
    {
        return static_cast<size_t>(std::stoul(value));
    }
    catch (...)
    {
        return std::nullopt;
    }
}
