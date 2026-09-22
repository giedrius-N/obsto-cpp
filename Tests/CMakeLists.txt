cmake_minimum_required(VERSION 3.15)
project(obsto_tests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(GTest REQUIRED)

add_executable(obsto_tests
	# Unit tests
	# Storage
	${CMAKE_CURRENT_SOURCE_DIR}/UnitTests/TestObjectStorage.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/UnitTests/TestBoundedObjectStorage.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/UnitTests/TestDiskObjectStorage.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/UnitTests/TestStoragePersistency.cpp
	# Streaming
	${CMAKE_CURRENT_SOURCE_DIR}/UnitTests/TestDiskWriteSession.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/UnitTests/TestDiskReadSession.cpp
	# Protocol
	${CMAKE_CURRENT_SOURCE_DIR}/UnitTests/TestProtocolHandler.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/UnitTests/TestRespParser.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/UnitTests/TestRespProtocolHandler.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/UnitTests/TestHttpParser.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/UnitTests/TestHttpProtocolHandler.cpp
	
	# Integration tests
	${CMAKE_CURRENT_SOURCE_DIR}/IntgrTests/TestIntegration.cpp
)

target_include_directories(obsto_tests PRIVATE
	${CMAKE_SOURCE_DIR}/Core
	${CMAKE_SOURCE_DIR}/Core/Storage
	${CMAKE_CURRENT_SOURCE_DIR}/Mock
)

target_link_libraries(obsto_tests PRIVATE 
	GTest::gtest_main 
	obsto_core 
	asio::asio 
	ws2_32
)

set_target_properties(obsto_tests PROPERTIES
	RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/Tests"
)

add_custom_command(TARGET obsto_tests POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy_if_different
		$<TARGET_FILE:obsto_core>
		$<TARGET_FILE_DIR:obsto_tests>
)