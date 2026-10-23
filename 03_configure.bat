@echo off
setlocal
call "%~dp0config.bat"
echo ==== N-DX: configuring with CMake ====

if exist "%SDK_DIR%\CMakeLists.txt" goto :havesdk
echo SDK missing - fetching it first.
set "WAS_NP=%NDX_NOPAUSE%"
set "NDX_NOPAUSE=1"
call "%ROOT%\02_get_sdk.bat"
set "SDK_RC=%errorlevel%"
set "NDX_NOPAUSE=%WAS_NP%"
if not "%SDK_RC%"=="0" goto :fail
:havesdk

set "ARCH_ARG="
echo "%NDX_GENERATOR%" | findstr /i /c:"Visual Studio" >nul && set "ARCH_ARG=-A %NDX_ARCH%"

echo Generator: %NDX_GENERATOR%  %ARCH_ARG%
cmake -S "%ROOT%" -B "%BUILD_DIR%" -G "%NDX_GENERATOR%" %ARCH_ARG% -DVST3_SDK_ROOT="%SDK_DIR%" -DCMAKE_BUILD_TYPE=%NDX_CONFIG%
if errorlevel 1 goto :fail

echo.
echo Configure OK. Build tree: %BUILD_DIR%
if not defined NDX_NOPAUSE pause
endlocal & exit /b 0

:fail
echo.
echo Configure failed. Common fixes:
echo   - wrong generator: set NDX_GENERATOR=Visual Studio 17 2022 and run again
echo   - SDK older than 3.7: add -DNDX_LEGACY_INITMODULE=ON to the cmake line above
echo   - stale cache: run 99_clean.bat
if not defined NDX_NOPAUSE pause
endlocal & exit /b 1
