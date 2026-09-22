#pragma once

#include "IObjectStorage.h"
#include "IStreamingObjectStorage.h"
#include "Persistency/StoragePersistency.h"
#include <memory>
#include <mutex>
#include <unordered_map>

class BoundedObjectStorage : public IObjectStorage, public IStreamingObjectStorage
{
public:
    BoundedObjectStorage(std::unique_ptr<IObjectStorage> inner, size_t maxBytes);
    BoundedObjectStorage(std::unique_ptr<IObjectStorage> inner, StoragePersistency& persistency, size_t maxBytes);
    ~BoundedObjectStorage() override = default;

    // IStreamingObjectStorage interface
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
    bool wouldFit(const std::string& path, size_t incomingBytes) const;
    size_t totalBytes() const;
    std::optional<size_t> objectSize(const std::string& path) const;
    bool contains(const std::string& path) const;
    void upsert(const std::string& path, size_t size);
    void erase(const std::string& path);

    std::unique_ptr<IObjectStorage> m_innerStorage;
    IStreamingObjectStorage* m_innerStreaming = nullptr;
    StoragePersistency* m_persistency = nullptr;
    size_t m_maxBytes;
    size_t m_currentBytes = 0;
    std::unordered_map<std::string, size_t> m_sizes;

    mutable std::mutex m_lock;
};