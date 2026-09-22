#pragma once

#include <optional>
#include <string>

class IReadSession
{
public:
    virtual ~IReadSession() = default;
    
    virtual std::optional<std::string> readChunk(size_t maxBytes) = 0;
};
