@echo off
setlocal
call "%~dp0config.bat"
echo ==== N-DX: DSP unit tests - no VST3 SDK needed ====

set "ARCH_ARG="
echo "%NDX_GENERATOR%" | findstr /i /c:"Visual Studio" >nul && set "ARCH_ARG=-A %NDX_ARCH%"

cmake -S "%ROOT%\tests" -B "%TEST_DIR%" -G "%NDX_GENERATOR%" %ARCH_ARG%
if errorlevel 1 goto :fail
cmake --build "%TEST_DIR%" --config %NDX_CONFIG% --parallel
if errorlevel 1 goto :fail

pushd "%TEST_DIR%"
ctest -C %NDX_CONFIG% --output-on-failure
set "TEST_RC=%errorlevel%"
popd
if not "%TEST_RC%"=="0" goto :fail

echo.
echo ==== ALL DSP TESTS PASSED ====
if not defined NDX_NOPAUSE pause
endlocal & exit /b 0

:fail
echo.
echo DSP tests failed.
if not defined NDX_NOPAUSE pause
endlocal & exit /b 1
