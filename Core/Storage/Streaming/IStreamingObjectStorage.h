#pragma once

#include "IWriteSession.h"
#include "IReadSession.h"
#include <memory>
#include <string>

struct BeginPutResult
{
    PutResult status;
    std::unique_ptr<IWriteSession> session;
};

struct BeginGetResult
{
    bool found;
    size_t size;
    std::unique_ptr<IReadSession> session;
};


class IStreamingObjectStorage
{
public:
    virtual ~IStreamingObjectStorage() = default;

    virtual BeginPutResult  beginPut(const std::string& path, size_t expectedSize) = 0;
    virtual BeginGetResult beginGet(const std::string& path) = 0;
};
