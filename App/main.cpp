#include "Server.h"
#include "ProtocolDetectorHandler.h"
#include "BoundedObjectStorage.h"
#include "DiskObjectStorage.h"
#include "StoragePersistency.h"
#include "ArgumentParser.h"
#include <iostream>

int main(int argc, char* argv[])
{
    try
    {
        ArgumentParser parser(argc, argv);
        ServerConfig config = parser.parse();

        if (config.showHelp)
        {
            return 0;
        }

        std::cout << "\n========= O B S T O =========" << std::endl;
        std::cout << "=== Server  Configuration ===" << std::endl;
        std::cout << "Port: " << config.port << std::endl;
        std::cout << "Capacity: " << config.capacityMB << " MB" << std::endl;
        std::cout << "Storage Path: " << config.storagePath << std::endl;
        std::cout << "============================\n" << std::endl;

        std::unique_ptr<IObjectStorage> diskStorage =
            std::make_unique<DiskObjectStorage>(config.storagePath);
        StoragePersistency persistency(config.storagePath);

        BoundedObjectStorage storage(
            std::move(diskStorage),
            persistency,
            config.capacityMB * 1024 * 1024
        );

        asio::io_context ioContext;
        Server server(ioContext, config.port, [&storage]()
        {
            return std::make_unique<ProtocolDetectorHandler>(storage);
        });

        std::cout << "Server is running..." << std::endl;
        ioContext.run();
    }
    catch (const std::exception& e)
    {
        std::cerr << "\nException: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
