#pragma once

#include "IObjectStorage.h"
#include <optional>
#include <unordered_map>
#include <vector>

class MockObjectStorage : public IObjectStorage
{
public:
    void setNextPutResult(PutResult result)
    {
        m_forcedPutResult = result;
    }

    std::optional<std::string> get(const std::string& path) const override
    {
        auto it = m_storage.find(path);

        if (it == m_storage.end())
        {
            return std::nullopt;
        }

        return it->second;
    }


    PutResult put(const std::string& path, std::string bytes) override
    {
        if (path.empty())
        {
            return PutResult::InvalidPath;
        }

        if (m_forcedPutResult.has_value())
        {
            PutResult result = m_forcedPutResult.value();
            m_forcedPutResult.reset();
            return result;
        }

        m_storage[path] = std::move(bytes);
        return PutResult::Ok;
    }


    SetNxResult setnx(const std::string& path, std::string bytes) override
    {
        if (path.empty())
        {
            return SetNxResult::InvalidPath;
        }

        if (m_storage.contains(path))
        {
            return SetNxResult::AlreadyExists;
        }

        m_storage[path] = std::move(bytes);

        return SetNxResult::Inserted;
    }


    bool del(const std::string& path) override
    {
        return m_storage.erase(path) > 0;
    }


    std::vector<std::string> keys() const override
    {
        std::vector<std::string> result;

        for (const auto& [key, value] : m_storage)
        {
            result.push_back(key);
        }

        return result;
    }


    bool exists(const std::string& path) const override
    {
        return m_storage.contains(path);
    }


private:
    std::unordered_map<std::string, std::string> m_storage;
    std::optional<PutResult> m_forcedPutResult;
};
