#pragma once

#include "IObjectStorage.h"
#include <unordered_map>
#include <mutex>

class ObjectStorage : public IObjectStorage
{
public:
    ObjectStorage() = default;
    ~ObjectStorage() override = default;
    
    PutResult put(const std::string& path, std::string bytes) override;
    std::optional<std::string> get(const std::string& path) const override;
    bool del(const std::string& path) override;
    std::vector<std::string> keys() const override;
    bool exists(const std::string& path) const override;
    SetNxResult setnx(const std::string& path, std::string bytes) override;

private:
    std::unordered_map<std::string, std::string> m_storage;
    mutable std::mutex m_lock;
};
