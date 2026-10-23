@echo off
setlocal
call "%~dp0config.bat"
echo ==== N-DX: building %NDX_NAME% [%NDX_CONFIG%] ====

if exist "%BUILD_DIR%\CMakeCache.txt" goto :built_cfg
set "WAS_NP=%NDX_NOPAUSE%"
set "NDX_NOPAUSE=1"
call "%ROOT%\03_configure.bat"
set "CFG_RC=%errorlevel%"
set "NDX_NOPAUSE=%WAS_NP%"
if not "%CFG_RC%"=="0" goto :fail
:built_cfg

cmake --build "%BUILD_DIR%" --config %NDX_CONFIG% --target %NDX_NAME% --parallel
if errorlevel 1 goto :fail

if exist "%PLUGIN_OUT%" goto :found
echo.
echo Build finished but the expected output was not found:
echo   %PLUGIN_OUT%
echo Look under %BUILD_DIR%\VST3 for the .vst3 file or folder.
goto :fail

:found
echo.
echo ==== BUILD OK ====
echo   %PLUGIN_OUT%
if not defined NDX_NOPAUSE pause
endlocal & exit /b 0

:fail
echo.
echo Build failed - scroll up for the first error.
if not defined NDX_NOPAUSE pause
endlocal & exit /b 1
