#pragma once

#include "HttpTypes.h"
#include <string>
#include <unordered_map>
#include <optional>


class HttpParser
{
public:
    enum class ParseResult { Complete, Incomplete, Error };

    static ParseResult parse(
        const std::string& buffer,
        http::HttpRequest& outRequest,
        size_t& bytesConsumed
    );

    static ParseResult parseHeadersOnly(
        const std::string& buffer, 
        http::HttpRequest& outRequest, 
        size_t& headerBytesConsumed
    );

    static std::optional<size_t> parseContentLength(const std::string& value);

private:
    static bool parseRequestLine(const std::string& line, http::HttpRequest& outRequest);
    static bool parseHeaderLine(const std::string& line, http::HttpRequest& outRequest);
    static std::string toLower(std::string s);
};
