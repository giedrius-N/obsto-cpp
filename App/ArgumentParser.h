#pragma once

#include "ServerConfig.h"
#include <string>

class ArgumentParser
{
public:
    ArgumentParser(int argc, char* argv[]);

    ServerConfig parse();

private:
    std::string nextValue(int& index);
    unsigned short nextValueAsPort(int& index);
    size_t nextValueAsSize(int& index);

    void printHelp() const;

    int m_argc;
    char** m_argv;
};
