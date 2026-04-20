@echo off

REM Remove build directory if it exists
if exist build (
rmdir /s /q build
)

REM Set Nuke path (update this to your actual install path)
set Nuke_ROOT="C:\Program Files\Nuke17.0v1"

REM Configure project
cmake -S . -B build -Wno-dev -DNuke_ROOT=%Nuke_ROOT% -DOPTIX=OFF -DOIDN_CPU=ON -DOIDN_CUDA=OFF -DOIDN_SYCL=OFF -DOIDN_METAL=OFF -DOIDN_HIP=OFF

REM Build and install
cmake --build build --config Release --target install

echo Done.
pause