@echo off
setlocal

set "ROOT_DIR=%~dp0.."
for %%I in ("%ROOT_DIR%") do set "ROOT_DIR=%%~fI"

set "BUILD_TYPE=Debug"
set "BUILD_DIR=%ROOT_DIR%\bld\tests\debug"

echo === Configuring test build %BUILD_TYPE% ===
conan profile detect --exist-ok
if errorlevel 1 (
	echo Conan profile detection failed.
	exit /b 1
)

conan install "%ROOT_DIR%" --output-folder="%BUILD_DIR%" --build=missing -s build_type=%BUILD_TYPE%
if errorlevel 1 (
	echo Conan install failed.
	exit /b 1
)

if exist "%ROOT_DIR%\CMakeUserPresets.json" del /q "%ROOT_DIR%\CMakeUserPresets.json"
if exist "%BUILD_DIR%\CMakePresets.json" del /q "%BUILD_DIR%\CMakePresets.json"

if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%" 2>nul

cmake -S "%ROOT_DIR%" -B "%BUILD_DIR%" -DCMAKE_TOOLCHAIN_FILE="%BUILD_DIR%\conan_toolchain.cmake" -DCMAKE_BUILD_TYPE=%BUILD_TYPE% -DBUILD_TESTING=ON
if errorlevel 1 (
	echo CMake configure failed.
	exit /b 1
)

echo === Building tests %BUILD_TYPE% ===
cmake --build "%BUILD_DIR%"
if errorlevel 1 (
	echo Test build failed.
	exit /b 1
)

echo === Done ===
endlocal
