#pragma once

#include "IObjectStorage.h"
#include <string_view>

class IWriteSession
{
public:
    virtual ~IWriteSession() = default;

    virtual bool writeChunk(std::string_view chunk) = 0;
    virtual PutResult finish() = 0;
    virtual void abort() = 0;
};
