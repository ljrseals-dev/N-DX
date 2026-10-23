@echo off
rem ============================================================
rem  N-DX build configuration - shared by every script.
rem  Override any value by setting it in your environment first.
rem ============================================================
set "NDX_NAME=NDX"
set "NDX_VERSION=1.0.0"
set "NDX_COMPANY=Lewis Seals"
set "NDX_PRESET_NAME=N-DX"

set "ROOT=%~dp0"
if "%ROOT:~-1%"=="\" set "ROOT=%ROOT:~0,-1%"

set "SDK_DIR=%ROOT%\external\vst3sdk"
set "SDK_REPO=https://github.com/steinbergmedia/vst3sdk.git"
rem SDK_REF = optional tag or commit to pin (empty = default branch)
if not defined SDK_REF set "SDK_REF="

set "BUILD_DIR=%ROOT%\build"
set "TEST_DIR=%ROOT%\build_tests"
set "DIST_DIR=%ROOT%\dist"
if not defined NDX_CONFIG set "NDX_CONFIG=Release"
if not defined NDX_ARCH set "NDX_ARCH=x64"

set "PLUGIN_OUT=%BUILD_DIR%\VST3\%NDX_CONFIG%\%NDX_NAME%.vst3"

rem ---- pick a Visual Studio generator unless NDX_GENERATOR is already set
if defined NDX_GENERATOR goto :cfg_done
set "NDX_GENERATOR=Visual Studio 17 2022"
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" goto :cfg_done
set "VSVER="
for /f "usebackq tokens=1 delims=." %%a in (`"%VSWHERE%" -latest -products * -property installationVersion`) do set "VSVER=%%a"
if "%VSVER%"=="16" set "NDX_GENERATOR=Visual Studio 16 2019"
if "%VSVER%"=="18" set "NDX_GENERATOR=Visual Studio 18 2026"
:cfg_done
exit /b 0
