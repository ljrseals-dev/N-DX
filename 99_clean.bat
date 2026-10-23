@echo off
setlocal
call "%~dp0config.bat"
rem Usage: 99_clean.bat [all]
rem   (no arg) removes build\, build_tests\ and dist\
rem   all      also removes the downloaded VST3 SDK in external\
echo ==== N-DX: clean ====

if exist "%BUILD_DIR%" rmdir /s /q "%BUILD_DIR%" & echo removed build
if exist "%ROOT%\build_juce" rmdir /s /q "%ROOT%\build_juce" & echo removed build_juce
if exist "%TEST_DIR%" rmdir /s /q "%TEST_DIR%" & echo removed build_tests
if exist "%DIST_DIR%" rmdir /s /q "%DIST_DIR%" & echo removed dist

if /i not "%~1"=="all" goto :done
if not exist "%SDK_DIR%" goto :done
set /p "ANS=Delete the downloaded SDK at %SDK_DIR% - Y or N? "
if /i "%ANS%"=="Y" rmdir /s /q "%SDK_DIR%" & echo removed SDK

:done
echo Clean complete.
if not defined NDX_NOPAUSE pause
endlocal & exit /b 0
