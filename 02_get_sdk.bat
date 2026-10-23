@echo off
setlocal
call "%~dp0config.bat"
echo ==== N-DX: fetching Steinberg VST3 SDK ====

where git >nul 2>&1
if errorlevel 1 (
    echo [FAIL] git not found.
    goto :fail
)

if exist "%SDK_DIR%\CMakeLists.txt" goto :update

echo Cloning %SDK_REPO% into %SDK_DIR% - this downloads several hundred MB with submodules.
git clone --recursive "%SDK_REPO%" "%SDK_DIR%"
if errorlevel 1 goto :fail
goto :pin

:update
echo SDK already present - making sure submodules are initialised.
git -C "%SDK_DIR%" submodule update --init --recursive
if errorlevel 1 goto :fail

:pin
if "%SDK_REF%"=="" goto :ok
echo Pinning SDK to %SDK_REF%
git -C "%SDK_DIR%" fetch --tags
git -C "%SDK_DIR%" checkout "%SDK_REF%"
if errorlevel 1 goto :fail
git -C "%SDK_DIR%" submodule update --init --recursive
if errorlevel 1 goto :fail

:ok
echo.
echo VST3 SDK ready: %SDK_DIR%
echo Check the licence of the SDK version you cloned before you distribute anything built with it.
if not defined NDX_NOPAUSE pause
endlocal & exit /b 0

:fail
echo.
echo SDK fetch failed. If you are offline, download the SDK from https://www.steinberg.net/developers/
echo and unpack it so that %SDK_DIR%\CMakeLists.txt exists.
if not defined NDX_NOPAUSE pause
endlocal & exit /b 1
