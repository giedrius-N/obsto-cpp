#pragma once

#include <string>
#include <vector>
#include <optional>

enum class PutResult { Ok, CapacityExceeded, InvalidPath, FailedToWrite };
enum class SetNxResult { Inserted, AlreadyExists, CapacityExceeded, InvalidPath };

class IObjectStorage
{
public:
    virtual ~IObjectStorage() = default;

    virtual PutResult put(const std::string& path, std::string bytes) = 0;
    virtual std::optional<std::string> get(const std::string& path) const = 0;
    virtual bool del(const std::string& path) = 0;
    virtual std::vector<std::string> keys() const = 0;
    virtual bool exists(const std::string& path) const = 0;
    virtual SetNxResult setnx(const std::string& path, std::string bytes) = 0;
};
