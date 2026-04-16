@echo off
:: 1. Set the Nuke Directory
set "NUKE_DIR=C:\Program Files\Nuke17.0v1"

:: 2. Add Nuke to the PATH so the OS can find DDImage.dll
set "PATH=%NUKE_DIR%;%PATH%"

:: 3. Launch Nuke
start "" "%NUKE_DIR%\Nuke17.0.exe"