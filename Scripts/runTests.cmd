@echo off
setlocal

set "ROOT_DIR=%~dp0.."
for %%I in ("%ROOT_DIR%") do set "ROOT_DIR=%%~fI"

set "BUILD_DIR=%ROOT_DIR%\bld\tests\debug"
set "TEST_EXE=%BUILD_DIR%\Tests\obsto_tests.exe"

if not exist "%TEST_EXE%" (
	echo Test executable not found: %TEST_EXE%
	echo Building tests first...
	call "%ROOT_DIR%\Scripts\buildTests.cmd"
	if errorlevel 1 (
		exit /b 1
	)
)

pushd "%BUILD_DIR%\Tests"
call "%TEST_EXE%" %*
set "EXIT_CODE=%errorlevel%"
popd

exit /b %EXIT_CODE%
