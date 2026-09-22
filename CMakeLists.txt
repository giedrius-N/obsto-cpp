cmake_minimum_required(VERSION 3.15)
project(obsto LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_BUILD_PARALLEL_LEVEL 4)

find_package(asio REQUIRED)

add_subdirectory(Core)

add_executable(${PROJECT_NAME}
	App/ArgumentParser.cpp
	App/main.cpp
)

target_include_directories(${PROJECT_NAME} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/App
)

if (WIN32)
	target_compile_definitions(${PROJECT_NAME} PRIVATE _WIN32_WINNT=0x0A00) # For Windows 10 SDK
endif()

target_link_libraries(${PROJECT_NAME} PRIVATE asio::asio obsto_core)

if (WIN32)
	target_link_libraries(${PROJECT_NAME} PRIVATE ws2_32)
endif()

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy_if_different
		$<TARGET_FILE:obsto_core>
		$<TARGET_FILE_DIR:${PROJECT_NAME}>
)

install(TARGETS ${PROJECT_NAME}
	RUNTIME DESTINATION bin
)

if (BUILD_TESTING)
	add_subdirectory(Tests)
endif()
