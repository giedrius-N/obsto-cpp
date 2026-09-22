#include "BoundedObjectStorage.h"
#include "BoundedWriteSession.h"

BoundedObjectStorage::BoundedObjectStorage(std::unique_ptr<IObjectStorage> inner, size_t maxBytes)
{
    m_innerStorage = std::move(inner);
    m_innerStreaming = dynamic_cast<IStreamingObjectStorage*>(m_innerStorage.get());
    m_persistency = nullptr;
    m_maxBytes = maxBytes;
}

BoundedObjectStorage::BoundedObjectStorage(std::unique_ptr<IObjectStorage> inner, StoragePersistency& persistency, size_t maxBytes)
    : m_innerStorage(std::move(inner))
    , m_innerStreaming(dynamic_cast<IStreamingObjectStorage*>(m_innerStorage.get()))
    , m_persistency(&persistency)
    , m_maxBytes(maxBytes)
{
    std::lock_guard<std::mutex> lock(m_lock);
    for (const auto& [path, size] : m_persistency->entries())
    {
        m_sizes[path] = size;
        m_currentBytes += size;
    }
}

PutResult BoundedObjectStorage::put(const std::string& path, std::string bytes)
{
    if (path.empty())
    {
        return PutResult::InvalidPath;
    }

    std::lock_guard<std::mutex> lock(m_lock);
    size_t incomingBytes = bytes.size();
    if (!wouldFit(path, incomingBytes))
    {
        return PutResult::CapacityExceeded;
    }

    auto result = m_innerStorage->put(path, std::move(bytes));
    if (result == PutResult::Ok)
    {
        upsert(path, incomingBytes);
    }
    return result;
}

std::optional<std::string> BoundedObjectStorage::get(const std::string& path) const
{
    return m_innerStorage->get(path);
}

bool BoundedObjectStorage::del(const std::string& path)
{
    std::lock_guard<std::mutex> lock(m_lock);
    if (!m_innerStorage->del(path))
    {
        return false;
    }

    erase(path);
    return true;
}

std::vector<std::string> BoundedObjectStorage::keys() const
{
    return m_innerStorage->keys();
}

bool BoundedObjectStorage::exists(const std::string& path) const
{
    return m_innerStorage->exists(path);
}

SetNxResult BoundedObjectStorage::setnx(const std::string& path, std::string bytes)
{
    if (path.empty())
    {
        return SetNxResult::InvalidPath;
    }

    std::lock_guard<std::mutex> lock(m_lock);
    if (contains(path))
    {
        return SetNxResult::AlreadyExists;
    }

    size_t incomingBytes = bytes.size();
    if (!wouldFit(path, incomingBytes))
    {
        return SetNxResult::CapacityExceeded;
    }

    auto result = m_innerStorage->setnx(path, std::move(bytes));
    if (result == SetNxResult::Inserted)
    {
        upsert(path, incomingBytes);
    }
    return result;
}

bool BoundedObjectStorage::wouldFit(const std::string& path, size_t incomingBytes) const
{
    auto existingSize = objectSize(path).value_or(0);
    size_t newTotalBytes = totalBytes() - existingSize + incomingBytes;
    return newTotalBytes <= m_maxBytes;
}

BeginPutResult BoundedObjectStorage::beginPut(const std::string& path, size_t expectedSize)
{
    if (!m_innerStreaming)
    {
        return {PutResult::InvalidPath, nullptr};
    }

    std::lock_guard<std::mutex> lock(m_lock);

    if (!wouldFit(path, expectedSize))
    {
        return {PutResult::CapacityExceeded, nullptr};
    }

    auto innerResult = m_innerStreaming->beginPut(path, expectedSize);
    if (innerResult.status != PutResult::Ok)
    {
        return innerResult;
    }

    auto previousSize = objectSize(path);
    upsert(path, expectedSize);

    return {PutResult::Ok, std::make_unique<BoundedWriteSession>(
        std::move(innerResult.session),
        [this, path](size_t) {},
        [this, path, previousSize](size_t)
        {
            if (previousSize)
            {
                upsert(path, *previousSize);
            }
            else
            {
                erase(path);
            }
        })
    };
}

size_t BoundedObjectStorage::totalBytes() const
{
    if (m_persistency)
    {
        return m_persistency->totalBytes();
    }
    return m_currentBytes;
}

std::optional<size_t> BoundedObjectStorage::objectSize(const std::string& path) const
{
    if (m_persistency)
    {
        return m_persistency->objectSize(path);
    }

    auto it = m_sizes.find(path);
    if (it == m_sizes.end())
    {
        return std::nullopt;
    }
    return it->second;
}

bool BoundedObjectStorage::contains(const std::string& path) const
{
    if (m_persistency)
    {
        return m_persistency->contains(path);
    }
    return m_sizes.contains(path);
}

void BoundedObjectStorage::upsert(const std::string& path, size_t size)
{
    if (m_persistency)
    {
        m_persistency->upsert(path, size);
        return;
    }

    auto it = m_sizes.find(path);
    if (it != m_sizes.end())
    {
        m_currentBytes -= it->second;
        it->second = size;
    }
    else
    {
        m_sizes.emplace(path, size);
    }

    m_currentBytes += size;
}

void BoundedObjectStorage::erase(const std::string& path)
{
    if (m_persistency)
    {
        m_persistency->erase(path);
        return;
    }

    auto it = m_sizes.find(path);
    if (it == m_sizes.end())
    {
        return;
    }

    m_currentBytes -= it->second;
    m_sizes.erase(it);
}

BeginGetResult BoundedObjectStorage::beginGet(const std::string& path)
{
    if (!m_innerStreaming)
    {
        return {false, 0, nullptr};
    }
    return m_innerStreaming->beginGet(path);
}
