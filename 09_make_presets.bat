@echo off
setlocal
call "%~dp0config.bat"
rem Usage: 09_make_presets.bat [install]
rem   (no arg) regenerates presets\NDX\*.vstpreset with Python
rem   install   also copies them to Documents\VST3 Presets\<company>\<plugin>
echo ==== N-DX: presets ====

where python >nul 2>&1
if errorlevel 1 goto :nopy
python "%ROOT%\tools\make_presets.py" "%ROOT%\presets\NDX"
if errorlevel 1 goto :fail
goto :gen_done
:nopy
echo python not found - using the presets already shipped in presets\NDX
:gen_done

if /i not "%~1"=="install" goto :done
set "PDEST=%USERPROFILE%\Documents\VST3 Presets\%NDX_COMPANY%\%NDX_PRESET_NAME%"
if not exist "%PDEST%" mkdir "%PDEST%"
xcopy "%ROOT%\presets\NDX\*.vstpreset" "%PDEST%\" /Y /Q >nul
if errorlevel 1 goto :fail
echo Installed presets to %PDEST%

:done
echo.
echo Done.
if not defined NDX_NOPAUSE pause
endlocal & exit /b 0

:fail
echo Preset step failed.
if not defined NDX_NOPAUSE pause
endlocal & exit /b 1
