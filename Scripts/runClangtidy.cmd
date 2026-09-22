@echo off
setlocal EnableExtensions

set "ROOT_DIR=%~dp0.."
for %%I in ("%ROOT_DIR%") do set "ROOT_DIR=%%~fI"

set "BUILD_DIR=%ROOT_DIR%\bld\debug"
set "CLANG_TIDY_CONFIG=%ROOT_DIR%\.clang-tidy"
set "RESULTS_FILE=%ROOT_DIR%\clangtidy-results.txt"

type nul > "%RESULTS_FILE%"

if not exist "%BUILD_DIR%\compile_commands.json" (
	call :Log Build database not found: %BUILD_DIR%\compile_commands.json
	call :Log Run the project build first so CMake generates compile_commands.json.
	exit /b 1
)

if not exist "%CLANG_TIDY_CONFIG%" (
	call :Log clang-tidy config not found: %CLANG_TIDY_CONFIG%
	exit /b 1
)

where clang-tidy >nul 2>nul
if errorlevel 1 (
	call :Log clang-tidy was not found on PATH.
	call :Log Install LLVM/Clang and make sure clang-tidy is available from a Developer Command Prompt or regular shell.
	exit /b 1
)

call :Log === Running clang-tidy ===
for /r "%ROOT_DIR%\App" %%F in (*.cpp) do call :RunTidy "%%~fF"
for /r "%ROOT_DIR%\Core" %%F in (*.cpp) do call :RunTidy "%%~fF"

call :Log === Done ===
call :Log Results written to %RESULTS_FILE%
endlocal
exit /b 0

:RunTidy
call :Log --- %~1
clang-tidy "%~1" -p "%BUILD_DIR%" --config-file="%CLANG_TIDY_CONFIG%" >> "%RESULTS_FILE%" 2>&1
if errorlevel 1 (
	call :Log clang-tidy reported issues for %~1
	exit /b 1
)
exit /b 0

:Log
echo %*
>> "%RESULTS_FILE%" echo %*
exit /b 0
