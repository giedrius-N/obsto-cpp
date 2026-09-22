#include "DiskReadSession.h"

DiskReadSession::DiskReadSession(const std::filesystem::path& path)
    : m_file(path, std::ios::binary)
{
}

std::optional<std::string> DiskReadSession::readChunk(size_t maxBytes)
{
    if (!m_file || m_file.eof()) 
        return std::nullopt;

    std::string buffer(maxBytes, '\0');
    m_file.read(buffer.data(), static_cast<std::streamsize>(maxBytes));
    std::streamsize got = m_file.gcount();

    if (got == 0) 
        return std::nullopt;

    buffer.resize(static_cast<size_t>(got));
    return buffer;
}
