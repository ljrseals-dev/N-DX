@echo off
setlocal
call "%~dp0config.bat"
rem Usage: 06_install.bat [user|system]
rem   system = %CommonProgramFiles%\VST3                 (needs an Administrator prompt)
rem   user   = %LOCALAPPDATA%\Programs\Common\VST3       (no admin needed)
rem Default: system if running as admin, otherwise user.
echo ==== N-DX: installing ====

if exist "%PLUGIN_OUT%" goto :havebuild
echo Plugin not built yet - run 04_build.bat first.
goto :fail
:havebuild

set "TARGET=%~1"
if not "%TARGET%"=="" goto :havetarget
net session >nul 2>&1
if errorlevel 1 (set "TARGET=user") else (set "TARGET=system")
:havetarget

if /i "%TARGET%"=="system" set "DEST=%CommonProgramFiles%\VST3"
if /i "%TARGET%"=="user" set "DEST=%LOCALAPPDATA%\Programs\Common\VST3"
if "%DEST%"=="" (
    echo Unknown target %TARGET% - use user or system.
    goto :fail
)

echo Installing to %DEST%
if not exist "%DEST%" mkdir "%DEST%"
if errorlevel 1 goto :permfail

if exist "%PLUGIN_OUT%\" goto :isdir
copy /Y "%PLUGIN_OUT%" "%DEST%\" >nul
if errorlevel 1 goto :permfail
goto :copied
:isdir
xcopy "%PLUGIN_OUT%" "%DEST%\%NDX_NAME%.vst3\" /E /I /Y /Q >nul
if errorlevel 1 goto :permfail
:copied

echo.
echo ==== INSTALLED ====
echo Rescan plug-ins in your DAW. Presets: run 09_make_presets.bat install
if not defined NDX_NOPAUSE pause
endlocal & exit /b 0

:permfail
echo.
echo Copy failed. For the system folder open an Administrator prompt, or run: 06_install.bat user
:fail
if not defined NDX_NOPAUSE pause
endlocal & exit /b 1
