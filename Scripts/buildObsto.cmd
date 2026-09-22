@echo off
setlocal

set "ROOT_DIR=%~dp0.."
for %%I in ("%ROOT_DIR%") do set "ROOT_DIR=%%~fI"

set "SKIP_CONFIGURE=0"
set "BUILD_TYPE=Debug"
set "BUILD_FLAVOR=debug"

:parse_args
if "%~1"=="" goto args_done

if /I "%~1"=="-q" (
	set "SKIP_CONFIGURE=1"
) else if /I "%~1"=="-debug" (
	set "BUILD_TYPE=Debug"
	set "BUILD_FLAVOR=debug"
) else if /I "%~1"=="-release" (
	set "BUILD_TYPE=Release"
	set "BUILD_FLAVOR=release"
) else (
	echo Unknown argument: %~1
	exit /b 1
)

shift
goto parse_args

:args_done
set "BUILD_DIR=%ROOT_DIR%\bld\%BUILD_FLAVOR%"
set "OUT_DIR=%ROOT_DIR%\bld\out\%BUILD_FLAVOR%"

if "%SKIP_CONFIGURE%"=="0" (
	echo === Configuring %BUILD_TYPE% ===
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

	cmake -S "%ROOT_DIR%" -B "%BUILD_DIR%" -DCMAKE_TOOLCHAIN_FILE="%BUILD_DIR%\conan_toolchain.cmake" -DCMAKE_BUILD_TYPE=%BUILD_TYPE%
	if errorlevel 1 (
		echo CMake configure failed.
		exit /b 1
	)
) else (
	if not exist "%BUILD_DIR%\CMakeCache.txt" (
		echo Build directory is not configured: %BUILD_DIR%
		echo Run without -q first.
		exit /b 1
	)
)

echo === Building %BUILD_TYPE% ===
cmake --build "%BUILD_DIR%"
if errorlevel 1 (
	echo Build failed.
	exit /b 1
)

if not exist "%OUT_DIR%" mkdir "%OUT_DIR%"

echo === Installing to %OUT_DIR% ===
cmake --install "%BUILD_DIR%" --prefix "%OUT_DIR%"
if errorlevel 1 (
	echo Install failed.
	exit /b 1
)

echo === Done ===
endlocal
