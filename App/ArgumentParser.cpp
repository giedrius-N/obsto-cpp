#include "ArgumentParser.h"
#include <iostream>
#include <stdexcept>

ArgumentParser::ArgumentParser(int argc, char* argv[])
    : m_argc(argc)
    , m_argv(argv)
{
}

std::string ArgumentParser::nextValue(int& index)
{
    if (index + 1 >= m_argc)
    {
        std::cerr << "Missing value for argument: " << m_argv[index] << std::endl;
        printHelp();
        exit(1);
    }
    return m_argv[++index];
}

unsigned short ArgumentParser::nextValueAsPort(int& index)
{
    std::string flagName = m_argv[index];
    std::string value = nextValue(index);

    try
    {
        int parsed = std::stoi(value);
        if (parsed < 0 || parsed > 65535)
        {
            throw std::out_of_range("Port must be between 0 and 65535");
        }
        return static_cast<unsigned short>(parsed);
    }
    catch (const std::exception& e)
    {
        std::cerr << "Invalid value '" << value << "' for " << flagName << ": " << e.what() << std::endl;
        printHelp();
        exit(1);
    }
}

size_t ArgumentParser::nextValueAsSize(int& index)
{
    std::string flagName = m_argv[index];
    std::string value = nextValue(index);

    try
    {
        return static_cast<size_t>(std::stoull(value));
    }
    catch (const std::exception& e)
    {
        std::cerr << "Invalid value '" << value << "' for " << flagName << ": " << e.what() << std::endl;
        printHelp();
        exit(1);
    }
}

ServerConfig ArgumentParser::parse()
{
    ServerConfig config;

    for (int i = 1; i < m_argc; i++)
    {
        std::string arg = m_argv[i];

        if (arg == "--port" || arg == "-p")
        {
            config.port = nextValueAsPort(i);
        }
        else if (arg == "--capacity" || arg == "-c")
        {
            config.capacityMB = nextValueAsSize(i);
        }
        else if (arg == "--path" || arg == "-P")
        {
            config.storagePath = nextValue(i);
        }
        else if (arg == "--help" || arg == "-h")
        {
            config.showHelp = true;
            printHelp();
        }
        else
        {
            std::cerr << "Unknown argument: " << arg << std::endl;
            printHelp();
            exit(1);
        }
    }

    return config;
}

void ArgumentParser::printHelp() const
{
    std::cout << "\nUsage: " << m_argv[0] << " [options]\n"
              << "\nOptions:\n"
              << "  --port, -p     Server port (default: 8000)\n"
              << "  --capacity, -c Storage capacity in MB (default: 200)\n"
              << "  --path, -P     Storage path (default: C:\\develop\\obsto)\n"
              << "  --help, -h     Show this help message\n"
              << "\nExample:\n"
              << "  " << m_argv[0] << " --port 8080 --capacity 500 --path /data/storage\n"
              << "  " << m_argv[0] << " -p 8080 -c 500 -P /data/storage\n";
}
