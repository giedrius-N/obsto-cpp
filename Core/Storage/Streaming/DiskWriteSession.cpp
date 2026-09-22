#include "DiskWriteSession.h"

DiskWriteSession::DiskWriteSession(std::filesystem::path tempPath, std::filesystem::path finalPath)
    : m_tempPath(std::move(tempPath))
    , m_finalPath(std::move(finalPath))
    , m_file(m_tempPath, std::ios::binary | std::ios::trunc)
{
}

bool DiskWriteSession::writeChunk(std::string_view chunk)
{
    if (!m_file)
    {
        return false;
    }
    m_file.write(chunk.data(), static_cast<std::streamsize>(chunk.size()));
    return static_cast<bool>(m_file);
}

PutResult DiskWriteSession::finish()
{
    m_file.close();
    if (!m_file)
    {
        std::error_code ec;
        std::filesystem::remove(m_tempPath, ec);
        return PutResult::FailedToWrite;
    }

    std::error_code ec;
    std::filesystem::rename(m_tempPath, m_finalPath, ec);
    if (ec)
    {
        std::filesystem::remove(m_tempPath, ec);
        return PutResult::FailedToWrite;
    }

    m_finished = true;
    return PutResult::Ok;
}

void DiskWriteSession::abort()
{
    m_file.close();
    std::error_code ec;
    std::filesystem::remove(m_tempPath, ec);
}
