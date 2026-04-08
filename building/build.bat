@echo off
setlocal

:: Get the directory of this script
set SCRIPT_DIR=%~dp0

:: Remove trailing backslash (optional but cleaner)
if "%SCRIPT_DIR:~-1%"=="\" set SCRIPT_DIR=%SCRIPT_DIR:~0,-1%

cd /d "%SCRIPT_DIR%"

:: Create directory (mkdir handles existing dirs fine)
mkdir "%SCRIPT_DIR%\..\lib\bin"

:: Run CMake configure
cmake -S . -B W:/b -Wno-dev

:: Build with Release config
cmake --build W:/b --config Release -- /m:16

endlocal