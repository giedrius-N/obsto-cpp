#pragma once

#include <filesystem>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>
#include <utility>

class StoragePersistency
{
public:
	explicit StoragePersistency(std::filesystem::path rootDir);

	void refresh();

	std::vector<std::string> keys() const;
	std::vector<std::pair<std::string, size_t>> entries() const;
	std::optional<size_t> objectSize(const std::string& path) const;
	size_t totalBytes() const;
	bool contains(const std::string& path) const;

	void upsert(const std::string& path, size_t size);
	bool erase(const std::string& path);

	std::filesystem::path rootDir() const;

private:
	static bool isTempFile(const std::filesystem::path& path);
	static std::optional<std::string> toRelativeKey(const std::filesystem::path& rootDir, const std::filesystem::path& filePath);

	void rebuildIndexUnlocked();

	std::filesystem::path m_rootDir;
	mutable std::mutex m_lock;
	std::unordered_map<std::string, size_t> m_sizes;
	size_t m_totalBytes = 0;
};
