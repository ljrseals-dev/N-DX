@echo off
setlocal
rem ============================================================
rem  N-DX one-click pipeline
rem    00_RUN_ALL.bat            check -> SDK -> DSP tests -> configure -> build -> validate
rem    00_RUN_ALL.bat install    ... and then install plug-in + presets for the current user
rem    00_RUN_ALL.bat release    ... and then package a zip into dist\
rem    00_RUN_ALL.bat juce       ... and also build the full-GUI JUCE edition (VST3 + Standalone)
rem ============================================================
call "%~dp0config.bat"
set "NDX_NOPAUSE=1"
set "DO_INSTALL=0"
set "DO_RELEASE=0"
set "DO_JUCE=0"
if /i "%~1"=="install" set "DO_INSTALL=1"
if /i "%~1"=="release" set "DO_RELEASE=1"
if /i "%~1"=="juce" set "DO_JUCE=1"

echo.
echo [1/7] environment
call "%ROOT%\01_check_env.bat"
if errorlevel 1 goto :fail

echo.
echo [2/7] Steinberg VST3 SDK
call "%ROOT%\02_get_sdk.bat"
if errorlevel 1 goto :fail

echo.
echo [3/7] DSP unit tests
call "%ROOT%\08_test_dsp.bat"
if errorlevel 1 goto :fail

echo.
echo [4/7] configure
call "%ROOT%\03_configure.bat"
if errorlevel 1 goto :fail

echo.
echo [5/7] build
call "%ROOT%\04_build.bat"
if errorlevel 1 goto :fail

echo.
echo [6/7] validate
call "%ROOT%\05_validate.bat"
if errorlevel 1 goto :fail

echo.
echo [7/7] presets
call "%ROOT%\09_make_presets.bat"

if "%DO_INSTALL%"=="1" (
    call "%ROOT%\06_install.bat" user
    if errorlevel 1 goto :fail
    call "%ROOT%\09_make_presets.bat" install
)
if "%DO_JUCE%"=="1" (
    call "%ROOT%\10_build_juce.bat"
    if errorlevel 1 goto :fail
)
if "%DO_RELEASE%"=="1" (
    call "%ROOT%\07_package.bat"
    if errorlevel 1 goto :fail
)

echo.
echo ================================================
echo  N-DX PIPELINE COMPLETE
echo  %PLUGIN_OUT%
echo ================================================
set "NDX_NOPAUSE="
pause
endlocal & exit /b 0

:fail
echo.
echo ================================================
echo  PIPELINE STOPPED - see the messages above
echo ================================================
set "NDX_NOPAUSE="
pause
endlocal & exit /b 1
