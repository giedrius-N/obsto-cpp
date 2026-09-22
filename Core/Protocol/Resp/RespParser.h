#pragma once

#include <string>
#include <vector>
#include <optional>

namespace resp 
{
constexpr int kMaxArrayElements = 100000;

struct Array {
    int count;
    std::vector<std::string> elements;
};

struct BulkString {
    int length;
    std::string data;
};

struct SimpleString {
    std::string data;
};

struct Error {
    std::string message;
};

struct Integer {
    int value;
};

class RespParser {
public:
    enum class ParseResult {
        Complete,
        Incomplete,
        Error
    };

    // Parse bulk string: $<length>\r\n<data>\r\n
    static std::optional<BulkString> parseBulkString(const std::string& buffer, size_t& pos);
    
    // Parse simple string: +<data>\r\n
    static std::optional<SimpleString> parseSimpleString(const std::string& buffer, size_t& pos);
    
    // Parse error: -<message>\r\n
    static std::optional<Error> parseError(const std::string& buffer, size_t& pos);
    
    // Parse integer: :<number>\r\n
    static std::optional<Integer> parseInt(const std::string& buffer, size_t& pos);
    
    // Parse array: *<count>\r\n...elements...
    static ParseResult parseArray(const std::string& buffer, size_t& pos, Array& outArray);
    
    // Parse any RESP type
    static ParseResult parse(const std::string& buffer, std::vector<std::string>& commands, size_t& bytesConsumed);
};
} // namespace resp
