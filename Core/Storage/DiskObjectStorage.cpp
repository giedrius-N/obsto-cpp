#include "DiskObjectStorage.h"
#include "DiskWriteSession.h"
#include "DiskReadSession.h"
#include <fstream>
#include <sstream>

DiskObjectStorage::DiskObjectStorage(std::filesystem::path rootDir)
    : m_rootDir(std::move(rootDir))
{
    std::filesystem::create_directories(m_rootDir);
}

std::optional<std::filesystem::path> DiskObjectStorage::mapPath(const std::string& path) const
{
    if (path.empty())
    {
        return std::nullopt;
    }

    std::filesystem::path requested(path);

    // Absolute paths are not valid object
    if (requested.is_absolute())
    {
        return std::nullopt;
    }

    // Reject explicit traversal
    for (const auto& part : requested)
    {
        if (part == "..")
        {
            return std::nullopt;
        }
    }

    std::filesystem::path targetPath = (m_rootDir / requested).lexically_normal();

    auto [rootEnd, targetEnd] = std::mismatch(
        m_rootDir.begin(), m_rootDir.end(),
        targetPath.begin(), targetPath.end()
    );

    if (rootEnd != m_rootDir.end())
    {
        return std::nullopt;
    }

    return targetPath;
}

PutResult DiskObjectStorage::put(const std::string& path, std::string bytes)
{
    auto resolved = mapPath(path);
    if (!resolved)
    {
        return PutResult::InvalidPath;
    }

    std::lock_guard<std::mutex> lock(m_lock);

    std::error_code ec;
    std::filesystem::create_directories(resolved->parent_path(), ec);
    if (ec)
    {
        return PutResult::InvalidPath;
    }

    std::ofstream file(*resolved, std::ios::binary | std::ios::trunc);
    if (!file)
    {
        return PutResult::InvalidPath;
    }

    file.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    if (!file)
    {
        return PutResult::InvalidPath;
    }

    return PutResult::Ok;
}

std::optional<std::string> DiskObjectStorage::get(const std::string& path) const
{
    auto resolved = mapPath(path);
    if (!resolved)
    {
        return std::nullopt;
    }

    std::lock_guard<std::mutex> lock(m_lock);

    std::ifstream file(*resolved, std::ios::binary);
    if (!file)
    {
        return std::nullopt;
    }

    std::ostringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

bool DiskObjectStorage::del(const std::string& path)
{
    auto resolved = mapPath(path);
    if (!resolved)
    {
        return false;
    }

    std::lock_guard<std::mutex> lock(m_lock);

    std::error_code ec;
    return std::filesystem::remove(*resolved, ec) && !ec;
}

std::vector<std::string> DiskObjectStorage::keys() const
{
    std::lock_guard<std::mutex> lock(m_lock);

    std::vector<std::string> result;
    if (!std::filesystem::exists(m_rootDir))
    {
        return result;
    }

    for (const auto& entry : std::filesystem::recursive_directory_iterator(m_rootDir))
    {
        if (entry.is_regular_file() && entry.path().extension() != ".tmp")
        {
            auto relative = std::filesystem::relative(entry.path(), m_rootDir);
            result.push_back(relative.generic_string());
        }
    }
    return result;
}

bool DiskObjectStorage::exists(const std::string& path) const
{
    auto resolved = mapPath(path);
    if (!resolved)
    {
        return false;
    }

    std::lock_guard<std::mutex> lock(m_lock);
    return std::filesystem::exists(*resolved) && std::filesystem::is_regular_file(*resolved);
}

SetNxResult DiskObjectStorage::setnx(const std::string& path, std::string bytes)
{
    auto resolved = mapPath(path);
    if (!resolved)
    {
        return SetNxResult::InvalidPath;
    }

    std::lock_guard<std::mutex> lock(m_lock);

    if (std::filesystem::exists(*resolved))
    {
        return SetNxResult::AlreadyExists;
    }

    std::error_code ec;
    std::filesystem::create_directories(resolved->parent_path(), ec);
    if (ec)
    {
        return SetNxResult::InvalidPath;
    }

    std::ofstream file(*resolved, std::ios::binary | std::ios::trunc);
    if (!file)
    {
        return SetNxResult::InvalidPath;
    }

    file.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    return SetNxResult::Inserted;
}

BeginPutResult DiskObjectStorage::beginPut(const std::string& path, size_t)
{
    auto resolved = mapPath(path);
    if (!resolved)
    {
        return {PutResult::InvalidPath, nullptr};
    }

    std::error_code ec;
    std::filesystem::create_directories(resolved->parent_path(), ec);
    if (ec)
    {
        return {PutResult::InvalidPath, nullptr};
    }

    auto tempPath = *resolved;
    tempPath += ".tmp";
    return {PutResult::Ok, std::make_unique<DiskWriteSession>(tempPath, *resolved)};
}

BeginGetResult DiskObjectStorage::beginGet(const std::string& path)
{
    auto resolved = mapPath(path);
    if (!resolved)
    {
        return {false, 0, nullptr};
    }

    std::lock_guard<std::mutex> lock(m_lock);

    std::error_code ec;
    if (!std::filesystem::exists(*resolved, ec) || !std::filesystem::is_regular_file(*resolved, ec))
    {
        return {false, 0, nullptr};
    }

    auto size = std::filesystem::file_size(*resolved, ec);
    if (ec)
    {
        return {false, 0, nullptr};
    }

    return {true, size, std::make_unique<DiskReadSession>(*resolved)};
}
