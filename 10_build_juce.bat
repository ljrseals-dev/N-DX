@echo off
setlocal
call "%~dp0config.bat"
rem Usage: 10_build_juce.bat [install]
rem Builds the full-GUI JUCE edition (VST3 + Standalone) that wraps the same N-DX engine.
rem JUCE is downloaded automatically by CMake the first time (needs internet + git).
echo ==== N-DX: building JUCE edition [%NDX_CONFIG%] ====

set "JUCE_BUILD=%ROOT%\build_juce"
set "ARCH_ARG="
echo "%NDX_GENERATOR%" | findstr /i /c:"Visual Studio" >nul && set "ARCH_ARG=-A %NDX_ARCH%"

cmake -S "%ROOT%\juce" -B "%JUCE_BUILD%" -G "%NDX_GENERATOR%" %ARCH_ARG%
if errorlevel 1 goto :fail
cmake --build "%JUCE_BUILD%" --config %NDX_CONFIG% --parallel
if errorlevel 1 goto :fail

set "ART=%JUCE_BUILD%\NDXJuce_artefacts\%NDX_CONFIG%"
set "JUCE_VST3=%ART%\VST3\N-DX.vst3"
set "JUCE_EXE=%ART%\Standalone\N-DX.exe"
echo.
echo ==== JUCE BUILD OK ====
if exist "%JUCE_VST3%" echo   VST3:       %JUCE_VST3%
if exist "%JUCE_EXE%" echo   Standalone: %JUCE_EXE%

if /i not "%~1"=="install" goto :done
set "DEST=%LOCALAPPDATA%\Programs\Common\VST3"
if not exist "%DEST%" mkdir "%DEST%"
if exist "%JUCE_VST3%\" xcopy "%JUCE_VST3%" "%DEST%\N-DX.vst3\" /E /I /Y /Q >nul
if errorlevel 1 goto :fail
echo Installed to %DEST%
set "NDX_NOPAUSE_SAVE=%NDX_NOPAUSE%"
set "NDX_NOPAUSE=1"
call "%ROOT%\09_make_presets.bat" install
set "NDX_NOPAUSE=%NDX_NOPAUSE_SAVE%"

:done
if not defined NDX_NOPAUSE pause
endlocal & exit /b 0

:fail
echo.
echo JUCE build failed - scroll up for the first error. If the JUCE download failed, check your internet connection and git.
if not defined NDX_NOPAUSE pause
endlocal & exit /b 1
