#pragma once

#include <string>
#include <string_view>
#include <unordered_map>

namespace http
{
enum class Method { Unknown, Get, Put, Delete};

constexpr std::string_view toString(Method method)
{
    switch (method)
    {
        case Method::Get: return "GET";
        case Method::Put: return "PUT";
        case Method::Delete: return "DELETE";
        default:          return "UNKNOWN";
    }
}

struct HttpRequest
{
    std::string method;
    std::string path;
    std::string version;
    std::unordered_map<std::string, std::string> headers;
    std::string body;
};

enum class StatusCode : int
{
    OK = 200,
    Created = 201,
    BadRequest = 400,
    NotFound = 404,
    MethodNotAllowed = 405,
    InternalServerError = 500,
    InsufficientStorage = 507
};

constexpr std::string_view getStatusCodeStr(StatusCode code)
{
    switch (code)
    {
        case StatusCode::OK:                  return "OK";
        case StatusCode::Created:             return "Created";
        case StatusCode::BadRequest:          return "Bad Request";
        case StatusCode::NotFound:            return "Not Found";
        case StatusCode::MethodNotAllowed:    return "Method Not Allowed";
        case StatusCode::InternalServerError: return "Internal Server Error";
        case StatusCode::InsufficientStorage: return "Insufficient Storage";
        default:                              return "Unknown Status";
    }
}
} // namespace http
