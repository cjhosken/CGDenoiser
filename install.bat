@echo off

REM Remove build directory if it exists
if exist build (
rmdir /s /q build
)

REM Set Nuke path (update this to your actual install path)
set Nuke_ROOT="C:\Program Files\Nuke15.2v4"

REM Configure project
cmake -S . -B build -Wno-dev -DOPTIX=OFF

REM Build and install
cmake --build build --config Release --target install

echo Done.
pause