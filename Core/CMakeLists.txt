cmake_minimum_required(VERSION 3.15)
project(ObstoCore LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_BUILD_PARALLEL_LEVEL 4)

find_package(asio REQUIRED)

add_library(obsto_core SHARED
    # Storage
    Storage/BoundedObjectStorage.cpp
    Storage/ObjectStorage.cpp
    Storage/DiskObjectStorage.cpp
    # Streaming
    Storage/Streaming/DiskWriteSession.cpp
    Storage/Streaming/BoundedWriteSession.cpp
    Storage/Streaming/DiskReadSession.cpp
    # Persistency
    Storage/Persistency/StoragePersistency.cpp
    # Network
    Net/Server.cpp
    Net/Session.cpp
    # Protocol
    Protocol/ProtocolDetectorHandler.cpp
    Protocol/Resp/RespProtocolHandler.cpp
    Protocol/Resp/RespParser.cpp
    Protocol/Http/HttpProtocolHandler.cpp
    Protocol/Http/HttpParser.cpp
)

target_include_directories(obsto_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/Storage
    ${CMAKE_CURRENT_SOURCE_DIR}/Storage/Streaming
    ${CMAKE_CURRENT_SOURCE_DIR}/Storage/Persistency
    ${CMAKE_CURRENT_SOURCE_DIR}/Net
    ${CMAKE_CURRENT_SOURCE_DIR}/Protocol
    ${CMAKE_CURRENT_SOURCE_DIR}/Protocol/Resp
    ${CMAKE_CURRENT_SOURCE_DIR}/Protocol/Http
    ${CMAKE_CURRENT_SOURCE_DIR}/Utils
)

target_link_libraries(obsto_core PRIVATE asio::asio)

if(WIN32)
    target_link_libraries(obsto_core PRIVATE ws2_32 mswsock)
endif()

target_compile_features(obsto_core PUBLIC cxx_std_20)

set_target_properties(obsto_core PROPERTIES
    OUTPUT_NAME obsto_core
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

install(TARGETS obsto_core
    LIBRARY DESTINATION bin
    RUNTIME DESTINATION bin
    ARCHIVE DESTINATION lib
)

if(WIN32)
    target_compile_definitions(obsto_core PRIVATE _WIN32_WINNT=0x0A00 WIN32_LEAN_AND_MEAN)
endif()
