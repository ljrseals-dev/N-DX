@echo off
setlocal
call "%~dp0config.bat"
echo ==== N-DX: checking build environment ====
set "FAIL=0"

where git >nul 2>&1
if errorlevel 1 (
    echo [FAIL] git not found. Install Git for Windows: https://git-scm.com
    set "FAIL=1"
) else (
    echo [ ok ] git
)

where cmake >nul 2>&1
if errorlevel 1 (
    echo [FAIL] cmake not found. Install CMake 3.20 or newer: https://cmake.org/download
    set "FAIL=1"
) else (
    echo [ ok ] cmake
)

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" goto :novs
set "VSPATH="
for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSPATH=%%i"
if "%VSPATH%"=="" goto :novs
echo [ ok ] Visual Studio C++ tools found
echo        generator: %NDX_GENERATOR%
goto :vsdone
:novs
echo [FAIL] Visual Studio C++ tools not found.
echo        Install Visual Studio 2022 or Build Tools with the workload Desktop development with C++.
set "FAIL=1"
:vsdone

where python >nul 2>&1
if errorlevel 1 (
    echo [warn] python not found - only needed for 09_make_presets.bat and tools\make_weights.py
) else (
    echo [ ok ] python
)

if "%FAIL%"=="1" goto :bad
echo.
echo Environment looks good.
if not defined NDX_NOPAUSE pause
endlocal & exit /b 0
:bad
echo.
echo Fix the items marked FAIL, then run this script again.
if not defined NDX_NOPAUSE pause
endlocal & exit /b 1
