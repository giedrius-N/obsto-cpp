#pragma once

#include "IObjectStorage.h"
#include <string>

namespace respreply
{
std::string ok()
{
    return "+OK\r\n";
}

std::string pong()
{
    return "+PONG\r\n";
}

std::string simpleString(const std::string& value)
{
    return "+" + value + "\r\n";
}

std::string error(const std::string& message)
{
    return "-ERR " + message + "\r\n";
}

std::string integer(int value)
{
    return ":" + std::to_string(value) + "\r\n";
}

std::string bulkString(const std::string& value)
{
    return "$" + std::to_string(value.size()) + "\r\n" + value + "\r\n";
}

std::string nil()
{
    return "$-1\r\n";
}

std::string arrayHeader(size_t count)
{
    return "*" + std::to_string(count) + "\r\n";
}

std::string putResultToReply(PutResult result)
{
    switch (result)
    {
        case PutResult::Ok:
            return respreply::ok();

        case PutResult::CapacityExceeded:
            return respreply::error("capacity exceeded");

        case PutResult::InvalidPath:
            return respreply::error("invalid path");
    }

    return respreply::error("unknown error");
}

std::string setNxResultToReply(SetNxResult result)
{
    switch (result)
    {
        case SetNxResult::Inserted:
            return respreply::integer(1);

        case SetNxResult::AlreadyExists:
            return respreply::integer(0);

        case SetNxResult::CapacityExceeded:
            return respreply::error("capacity exceeded");

        case SetNxResult::InvalidPath:
            return respreply::error("invalid path");
    }

    return respreply::error("unknown error");
}
} // namespace respreply
