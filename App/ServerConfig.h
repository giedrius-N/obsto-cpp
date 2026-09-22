#pragma once

#include <string>

struct ServerConfig
{
    std::string storagePath = "C:\\develop\\obsto";
    unsigned short port = 8000;
    size_t capacityMB = 200;
    bool showHelp = false;
};
