#pragma once

#include "IWriteSession.h"
#include <filesystem>
#include <fstream>

class DiskWriteSession : public IWriteSession
{
public:
    DiskWriteSession(std::filesystem::path tempPath, std::filesystem::path finalPath);

    bool writeChunk(std::string_view chunk) override;
    PutResult finish() override;
    void abort() override;

private:
    std::filesystem::path m_tempPath;
    std::filesystem::path m_finalPath;
    std::ofstream m_file;
    bool m_finished = false;
};
