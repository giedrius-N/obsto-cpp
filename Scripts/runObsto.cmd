@echo off
setlocal

set "ROOT_DIR=%~dp0.."
for %%I in ("%ROOT_DIR%") do set "ROOT_DIR=%%~fI"

set "BUILD_TYPE=Debug"
set "BUILD_FLAVOR=debug"
set "APP_ARGS="

:parse_args
if "%~1"=="" goto args_done

if /I "%~1"=="-debug" (
	set "BUILD_TYPE=Debug"
	set "BUILD_FLAVOR=debug"
) else if /I "%~1"=="-release" (
	set "BUILD_TYPE=Release"
	set "BUILD_FLAVOR=release"
) else (
	set "APP_ARGS=%APP_ARGS% %1"
)

shift
goto parse_args

:args_done
set "APP_DIR=%ROOT_DIR%\bld\out\%BUILD_FLAVOR%"
set "APP_EXE=%APP_DIR%\bin\obsto.exe"

if not exist "%APP_EXE%" (
	echo App not found: %APP_EXE%
	echo Build it first with Scripts\buildObsto.cmd -%BUILD_FLAVOR%
	exit /b 1
)

pushd "%APP_DIR%"
"%APP_EXE%" %APP_ARGS%
set "EXIT_CODE=%errorlevel%"
popd

exit /b %EXIT_CODE%