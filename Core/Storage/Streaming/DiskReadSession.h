#pragma once

#include "IReadSession.h"
#include <fstream>
#include <filesystem>

class DiskReadSession : public IReadSession
{
public:
    explicit DiskReadSession(const std::filesystem::path& path);

    std::optional<std::string> readChunk(size_t maxBytes) override;

private:
    std::ifstream m_file;
};
