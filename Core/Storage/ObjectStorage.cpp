#include "ObjectStorage.h"

PutResult ObjectStorage::put(const std::string& path, std::string bytes)
{
    if (path.empty())
    {
        return PutResult::InvalidPath;
    }

    std::lock_guard<std::mutex> lock(m_lock);
    m_storage[path] = std::move(bytes);
    return PutResult::Ok;
}

std::optional<std::string> ObjectStorage::get(const std::string& path) const
{
    std::lock_guard<std::mutex> lock(m_lock);
    auto it = m_storage.find(path);
    if (it != m_storage.end())
    {
        return it->second;
    }
    return std::nullopt;
}

bool ObjectStorage::del(const std::string& path)
{
    std::lock_guard<std::mutex> lock(m_lock);
    return m_storage.erase(path) > 0;
}

std::vector<std::string> ObjectStorage::keys() const
{
    std::lock_guard<std::mutex> lock(m_lock);
    std::vector<std::string> result;
    result.reserve(m_storage.size());

    for (const auto& pair : m_storage)
    {
        result.push_back(pair.first);
    }
    return result;
}

bool ObjectStorage::exists(const std::string& path) const
{
    std::lock_guard<std::mutex> lock(m_lock);
    return m_storage.contains(path);
}

SetNxResult ObjectStorage::setnx(const std::string& path, std::string bytes)
{
    if (path.empty())
    {
        return SetNxResult::InvalidPath;
    }

    std::lock_guard<std::mutex> lock(m_lock);
    auto [it, inserted] = m_storage.try_emplace(path, std::move(bytes));
    if (inserted)
    {
        return SetNxResult::Inserted;
    }
    return SetNxResult::AlreadyExists;
}
