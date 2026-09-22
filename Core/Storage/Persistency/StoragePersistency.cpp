#include "StoragePersistency.h"
#include <system_error>

StoragePersistency::StoragePersistency(std::filesystem::path rootDir)
	: m_rootDir(std::move(rootDir))
{
	std::error_code ec;
	std::filesystem::create_directories(m_rootDir, ec);
	refresh();
}

void StoragePersistency::refresh()
{
	std::lock_guard<std::mutex> lock(m_lock);
	rebuildIndexUnlocked();
}

std::vector<std::string> StoragePersistency::keys() const
{
	std::lock_guard<std::mutex> lock(m_lock);

	std::vector<std::string> result;
	result.reserve(m_sizes.size());
	for (const auto& [path, size] : m_sizes)
	{
		(void)size;
		result.push_back(path);
	}
	return result;
}

std::vector<std::pair<std::string, size_t>> StoragePersistency::entries() const
{
	std::lock_guard<std::mutex> lock(m_lock);

	std::vector<std::pair<std::string, size_t>> result;
	result.reserve(m_sizes.size());
	for (const auto& [path, size] : m_sizes)
	{
		result.emplace_back(path, size);
	}
	return result;
}

std::optional<size_t> StoragePersistency::objectSize(const std::string& path) const
{
	std::lock_guard<std::mutex> lock(m_lock);

	auto it = m_sizes.find(path);
	if (it == m_sizes.end())
	{
		return std::nullopt;
	}
	return it->second;
}

size_t StoragePersistency::totalBytes() const
{
	std::lock_guard<std::mutex> lock(m_lock);
	return m_totalBytes;
}

bool StoragePersistency::contains(const std::string& path) const
{
	std::lock_guard<std::mutex> lock(m_lock);
	return m_sizes.contains(path);
}

void StoragePersistency::upsert(const std::string& path, size_t size)
{
	std::lock_guard<std::mutex> lock(m_lock);

	auto it = m_sizes.find(path);
	if (it != m_sizes.end())
	{
		m_totalBytes -= it->second;
		it->second = size;
	}
	else
	{
		m_sizes.emplace(path, size);
	}

	m_totalBytes += size;
}

bool StoragePersistency::erase(const std::string& path)
{
	std::lock_guard<std::mutex> lock(m_lock);

	auto it = m_sizes.find(path);
	if (it == m_sizes.end())
	{
		return false;
	}

	m_totalBytes -= it->second;
	m_sizes.erase(it);
	return true;
}

std::filesystem::path StoragePersistency::rootDir() const
{
	return m_rootDir;
}

bool StoragePersistency::isTempFile(const std::filesystem::path& path)
{
	return path.extension() == ".tmp";
}

std::optional<std::string> StoragePersistency::toRelativeKey(const std::filesystem::path& rootDir, const std::filesystem::path& filePath)
{
	std::error_code ec;
	auto relative = std::filesystem::relative(filePath, rootDir, ec);
	if (ec)
	{
		return std::nullopt;
	}

	auto normalized = relative.lexically_normal();
	if (normalized.empty() || normalized == ".")
	{
		return std::nullopt;
	}

	for (const auto& part : normalized)
	{
		if (part == "..")
		{
			return std::nullopt;
		}
	}

	return normalized.generic_string();
}

void StoragePersistency::rebuildIndexUnlocked()
{
	m_sizes.clear();
	m_totalBytes = 0;

	std::error_code ec;
	if (!std::filesystem::exists(m_rootDir, ec) || ec)
	{
		return;
	}

	std::filesystem::recursive_directory_iterator end;
	for (std::filesystem::recursive_directory_iterator it(m_rootDir, ec); it != end && !ec; it.increment(ec))
	{
		if (!it->is_regular_file(ec) || ec)
		{
			continue;
		}

		const auto& filePath = it->path();
		if (isTempFile(filePath))
		{
			continue;
		}

		auto key = toRelativeKey(m_rootDir, filePath);
		if (!key)
		{
			continue;
		}

		auto fileSize = std::filesystem::file_size(filePath, ec);
		if (ec)
		{
			ec.clear();
			continue;
		}

		m_sizes.emplace(*key, static_cast<size_t>(fileSize));
		m_totalBytes += static_cast<size_t>(fileSize);
	}
}
