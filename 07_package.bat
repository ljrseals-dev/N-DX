@echo off
setlocal
call "%~dp0config.bat"
echo ==== N-DX: packaging release zip ====

if exist "%PLUGIN_OUT%" goto :havebuild
echo Plugin not built yet - run 04_build.bat first.
goto :fail
:havebuild

set "JUCE_BUILD=%ROOT%\build_juce"
set "STAGE=%DIST_DIR%\stage\%NDX_NAME%-%NDX_VERSION%"
if exist "%DIST_DIR%\stage" rmdir /s /q "%DIST_DIR%\stage"
mkdir "%STAGE%" || goto :fail

if exist "%PLUGIN_OUT%\" goto :isdir
copy /Y "%PLUGIN_OUT%" "%STAGE%\" >nul
goto :copied
:isdir
xcopy "%PLUGIN_OUT%" "%STAGE%\%NDX_NAME%.vst3\" /E /I /Y /Q >nul
:copied

if exist "%JUCE_BUILD%\NDXJuce_artefacts\%NDX_CONFIG%\VST3\N-DX.vst3\" xcopy "%JUCE_BUILD%\NDXJuce_artefacts\%NDX_CONFIG%\VST3\N-DX.vst3" "%STAGE%\N-DX.vst3\" /E /I /Y /Q >nul
if exist "%ROOT%\presets\NDX" xcopy "%ROOT%\presets\NDX" "%STAGE%\Presets\" /E /I /Y /Q >nul
copy /Y "%ROOT%\README.md" "%STAGE%\" >nul
if exist "%ROOT%\press" xcopy "%ROOT%\press" "%STAGE%\press\" /E /I /Y /Q >nul
if exist "%ROOT%\tools\make_weights.py" copy /Y "%ROOT%\tools\make_weights.py" "%STAGE%\" >nul

set "ZIP=%DIST_DIR%\%NDX_NAME%-%NDX_VERSION%-win64.zip"
if exist "%ZIP%" del /q "%ZIP%"
powershell -NoProfile -Command "Compress-Archive -Path '%STAGE%' -DestinationPath '%ZIP%' -Force"
if errorlevel 1 goto :fail
rmdir /s /q "%DIST_DIR%\stage"

echo.
echo ==== PACKAGED ====
echo   %ZIP%
echo Reminder: VST is a trademark of Steinberg Media Technologies GmbH - keep that notice in your docs.
if not defined NDX_NOPAUSE pause
endlocal & exit /b 0

:fail
echo Packaging failed.
if not defined NDX_NOPAUSE pause
endlocal & exit /b 1
