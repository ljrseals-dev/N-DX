@echo off
setlocal
call "%~dp0config.bat"
echo ==== N-DX: running Steinberg VST3 validator ====

if exist "%PLUGIN_OUT%" goto :havebuild
echo Plugin not built yet - run 04_build.bat first.
goto :fail
:havebuild

set "VALIDATOR="
for /f "delims=" %%f in ('dir /s /b "%BUILD_DIR%\validator.exe" 2^>nul') do set "VALIDATOR=%%f"
if not "%VALIDATOR%"=="" goto :runval

echo Validator not built yet - building the SDK validator target.
cmake --build "%BUILD_DIR%" --config %NDX_CONFIG% --target validator --parallel
for /f "delims=" %%f in ('dir /s /b "%BUILD_DIR%\validator.exe" 2^>nul') do set "VALIDATOR=%%f"
if "%VALIDATOR%"=="" goto :novalidator

:runval
echo Using %VALIDATOR%
"%VALIDATOR%" "%PLUGIN_OUT%"
if errorlevel 1 goto :fail
echo.
echo ==== VALIDATION PASSED ====
if not defined NDX_NOPAUSE pause
endlocal & exit /b 0

:novalidator
echo The SDK validator target is not available in this SDK version - skipping validation.
if not defined NDX_NOPAUSE pause
endlocal & exit /b 0

:fail
echo.
echo Validation failed or could not run.
if not defined NDX_NOPAUSE pause
endlocal & exit /b 1
