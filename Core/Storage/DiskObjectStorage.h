#pragma once

#include "IObjectStorage.h"
#include "IStreamingObjectStorage.h"
#include <filesystem>
#include <mutex>

class DiskObjectStorage : public IObjectStorage, public IStreamingObjectStorage
{
public:
    explicit DiskObjectStorage(std::filesystem::path rootDir);
    ~DiskObjectStorage() override = default;

    // IObjectStorage interface
    PutResult put(const std::string& path, std::string bytes) override;
    std::optional<std::string> get(const std::string& path) const override;
    bool del(const std::string& path) override;
    std::vector<std::string> keys() const override;
    bool exists(const std::string& path) const override;
    SetNxResult setnx(const std::string& path, std::string bytes) override;
    // IStreamingObjectStorage interface
    BeginPutResult beginPut(const std::string& path, size_t expectedSize) override;
    BeginGetResult beginGet(const std::string& path) override;

private:
    std::optional<std::filesystem::path> mapPath(const std::string& path) const;

    std::filesystem::path m_rootDir;
    mutable std::mutex m_lock;
};
